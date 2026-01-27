#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <elf.h>
#include <android/log.h>

#define LOG_TAG "native_integrity"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* ============================================================
 * CRC32
 * ============================================================ */
static uint32_t crc32_table[256];
static int crc_init = 0;

static void crc32_init(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        crc32_table[i] = c;
    }
    crc_init = 1;
}

static uint32_t crc32_calc(const uint8_t *buf, size_t len) {
    if (!crc_init) crc32_init();
    uint32_t c = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++)
        c = crc32_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFF;
}

/* ============================================================
 * PROC FILE HELPERS (UNCHANGED)
 * ============================================================ */
static jstring read_proc_file(JNIEnv* env, const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return (*env)->NewStringUTF(env, "Failed to open file");

    const size_t CHUNK = 64 * 1024; // 64 KB
    size_t cap = CHUNK;
    size_t len = 0;

    char *buf = malloc(cap);
    if (!buf) {
        close(fd);
        return (*env)->NewStringUTF(env, "OOM");
    }

    while (1) {
        if (len + CHUNK > cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) {
                free(buf);
                close(fd);
                return (*env)->NewStringUTF(env, "OOM");
            }
            buf = nb;
        }

        ssize_t r = read(fd, buf + len, CHUNK);
        if (r <= 0)
            break;

        len += r;
    }

    close(fd);

    buf[len] = '\0';

    jstring res = (*env)->NewStringUTF(env, buf);
    free(buf);
    return res;
}


JNIEXPORT jstring JNICALL
Java_com_example_selfmapsreader_MainActivity_readProcSelfStatus(
        JNIEnv* env, jobject thiz) {
    return read_proc_file(env, "/proc/self/status");
}

JNIEXPORT jstring JNICALL
Java_com_example_selfmapsreader_MainActivity_readProcSelfMaps(
        JNIEnv* env, jobject thiz) {
    return read_proc_file(env, "/proc/self/maps");
}

JNIEXPORT jstring JNICALL
Java_com_example_selfmapsreader_MainActivity_readProcSelfSmaps(
        JNIEnv* env, jobject thiz) {
    return read_proc_file(env, "/proc/self/smaps");
}


/* ============================================================
 * MAP ENTRY STRUCT
 * ============================================================ */
typedef struct {
    uintptr_t start;
    uintptr_t end;
    uintptr_t file_offset;
} map_entry_t;

/* ============================================================
 * Find mapping that backs file offset
 * ============================================================ */
static int find_mapping_for_offset(
        const char *soname,
        uintptr_t file_offset,
        map_entry_t *out) {

    FILE *fp = fopen("/proc/self/maps", "r");
    if (!fp) return 0;

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        if (!strstr(line, soname)) continue;

        uintptr_t start, end, off;
        char perms[8];

        if (sscanf(line, "%lx-%lx %4s %lx", &start, &end, perms, &off) != 4)
            continue;

        if (!(perms[0] == 'r' && perms[2] == 'x'))
            continue;

        if (file_offset >= off && file_offset < off + (end - start)) {
            out->start = start;
            out->end = end;
            out->file_offset = off;
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

/* ============================================================
 * ELF EXECUTABLE SEGMENT CHECK
 * ============================================================ */
static void check_library(
        const char *soname,
        const char *disk_path,
        char *out,
        size_t out_sz) {

    char *p = out;
    size_t left = out_sz;

    p += snprintf(p, left, "\n=== %s ===\n", soname);
    left = out_sz - (p - out);

    int fd = open(disk_path, O_RDONLY);
    if (fd < 0) {
        p += snprintf(p, left, "[-] Failed to open disk ELF\n");
        return;
    }

    struct stat st;
    fstat(fd, &st);

    uint8_t *disk = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (disk == MAP_FAILED) {
        p += snprintf(p, left, "[-] mmap failed\n");
        return;
    }

    Elf64_Ehdr *eh = (Elf64_Ehdr *)disk;
    Elf64_Phdr *ph = (Elf64_Phdr *)(disk + eh->e_phoff);

    for (int i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD) continue;
        if (!(ph[i].p_flags & PF_X)) continue;

        map_entry_t map;
        if (!find_mapping_for_offset(soname, ph[i].p_offset, &map)) {
            p += snprintf(p, left,
                          "[-] No runtime map for segment %d (offset 0x%lx)\n",
                          i, (unsigned long)ph[i].p_offset);
            continue;
        }

        uintptr_t mem_addr =
                map.start + (ph[i].p_offset - map.file_offset);

        uint32_t disk_crc =
                crc32_calc(disk + ph[i].p_offset, ph[i].p_filesz);

        uint32_t mem_crc =
                crc32_calc((uint8_t *)mem_addr, ph[i].p_filesz);

        p += snprintf(p, left,
                "Segment %d:\n"
                "  file_off=0x%lx size=%lu\n"
                "  runtime=0x%lx\n"
                "  disk_crc=0x%08x mem_crc=0x%08x\n",
                i,
                (unsigned long)ph[i].p_offset,
                (unsigned long)ph[i].p_filesz,
                (unsigned long)mem_addr,
                disk_crc,
                mem_crc);

        left = out_sz - (p - out);

        if (disk_crc != mem_crc)
            p += snprintf(p, left, "  !!! MODIFIED !!!\n");
    }

    //munmap(disk, st.st_size);
}

/* ============================================================
 * MAIN JNI ENTRY
 * ============================================================ */
JNIEXPORT jstring JNICALL
Java_com_example_selfmapsreader_MainActivity_getLibArtHash(
        JNIEnv *env, jobject thiz) {

    char result[8192];
    memset(result, 0, sizeof(result));

    check_library(
            "libc.so",
            "/apex/com.android.runtime/lib64/bionic/libc.so",
            result + strlen(result),
            sizeof(result) - strlen(result));

    check_library(
            "libart.so",
            "/apex/com.android.art/lib64/libart.so",
            result + strlen(result),
            sizeof(result) - strlen(result));

    return (*env)->NewStringUTF(env, result);
}
