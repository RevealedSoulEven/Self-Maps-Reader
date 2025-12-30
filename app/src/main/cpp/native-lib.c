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
 * CRC32 (fast + enough for tamper detection)
 * ============================================================ */
static uint32_t crc32_table[256];
static int crc32_init_done = 0;

static void crc32_init(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        crc32_table[i] = c;
    }
    crc32_init_done = 1;
}

static uint32_t crc32_calc(const uint8_t *buf, size_t len) {
    if (!crc32_init_done)
        crc32_init();

    uint32_t c = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++)
        c = crc32_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);

    return c ^ 0xFFFFFFFF;
}

/* ============================================================
 * /proc helpers (UNCHANGED FROM YOUR ORIGINAL DESIGN)
 * ============================================================ */
static jstring read_proc_file(JNIEnv* env, const char* path) {
    FILE* f = fopen(path, "r");
    if (!f)
        return (*env)->NewStringUTF(env, "Failed to open file");

    char* buf = NULL;
    size_t len = 0;
    char line[512];

    while (fgets(line, sizeof(line), f)) {
        size_t l = strlen(line);
        char* nbuf = realloc(buf, len + l + 1);
        if (!nbuf) {
            free(buf);
            fclose(f);
            return (*env)->NewStringUTF(env, "OOM");
        }
        buf = nbuf;
        memcpy(buf + len, line, l);
        len += l;
        buf[len] = 0;
    }
    fclose(f);

    jstring res = (*env)->NewStringUTF(env, buf ? buf : "");
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

/* ============================================================
 * Find runtime base address of loaded library
 * ============================================================ */
static uintptr_t find_runtime_base(const char *soname) {
    FILE *fp = fopen("/proc/self/maps", "r");
    if (!fp) return 0;

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, soname) && strstr(line, "r-xp")) {
            uintptr_t base = 0;
            sscanf(line, "%lx-%*lx", &base);
            fclose(fp);
            return base;
        }
    }
    fclose(fp);
    return 0;
}

/* ============================================================
 * ELF-aware executable segment comparison
 * ============================================================ */
static int check_library_integrity(
        const char *soname,
        const char *disk_path,
        char *out,
        size_t out_sz) {

    char *p = out;
    size_t left = out_sz;

    p += snprintf(p, left, "\n=== %s ===\n", soname);
    left = out_sz - (p - out);

    uintptr_t mem_base = find_runtime_base(soname);
    if (!mem_base) {
        p += snprintf(p, left, "[-] Failed to find runtime base\n");
        return -1;
    }

    p += snprintf(p, left, "[+] Runtime base: 0x%lx\n", mem_base);
    left = out_sz - (p - out);

    int fd = open(disk_path, O_RDONLY);
    if (fd < 0) {
        p += snprintf(p, left, "[-] Failed to open disk ELF\n");
        return -1;
    }

    struct stat st;
    fstat(fd, &st);

    void *disk_map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (disk_map == MAP_FAILED) {
        p += snprintf(p, left, "[-] mmap failed\n");
        return -1;
    }

    Elf64_Ehdr *eh = (Elf64_Ehdr *)disk_map;
    Elf64_Phdr *ph = (Elf64_Phdr *)((char *)disk_map + eh->e_phoff);

    int bad = 0;

    for (int i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD)
            continue;

        if (!(ph[i].p_flags & PF_X))
            continue;

        uint8_t *disk_seg =
                (uint8_t *)disk_map + ph[i].p_offset;
        uint8_t *mem_seg =
                (uint8_t *)(mem_base + ph[i].p_vaddr);

        size_t sz = ph[i].p_filesz;

        uint32_t disk_crc = crc32_calc(disk_seg, sz);
        uint32_t mem_crc  = crc32_calc(mem_seg,  sz);

        p += snprintf(
                p, left,
                "Segment %d:\n"
                "  offset=0x%lx vaddr=0x%lx size=%zu\n"
                "  disk_crc=0x%08x mem_crc=0x%08x\n",
                i,
                (unsigned long)ph[i].p_offset,
                (unsigned long)ph[i].p_vaddr,
                sz,
                disk_crc,
                mem_crc);

        left = out_sz - (p - out);

        if (disk_crc != mem_crc) {
            p += snprintf(p, left,
                          "  !!! EXECUTABLE SEGMENT MODIFIED !!!\n");
            bad = 1;
        }
    }

    munmap(disk_map, st.st_size);

    if (!bad)
        p += snprintf(p, left, "[+] Integrity OK\n");

    return bad ? -1 : 0;
}

/* ============================================================
 * MAIN JNI: libc + libart integrity check
 * ============================================================ */
JNIEXPORT jstring JNICALL
Java_com_example_selfmapsreader_MainActivity_getLibArtHash(
        JNIEnv *env, jobject thiz) {

    char result[8192];
    memset(result, 0, sizeof(result));

    /* libc */
    check_library_integrity(
            "libc.so",
            "/apex/com.android.runtime/lib64/bionic/libc.so",
            result + strlen(result),
            sizeof(result) - strlen(result));

    /* libart */
    check_library_integrity(
            "libart.so",
            "/apex/com.android.art/lib64/libart.so",
            result + strlen(result),
            sizeof(result) - strlen(result));

    return (*env)->NewStringUTF(env, result);
}
