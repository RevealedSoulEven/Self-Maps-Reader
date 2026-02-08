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
#include <link.h>
#include <android/log.h>

#define LOG_TAG "native_integrity"

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
 * PROC FILE HELPERS
 * ============================================================ */
static jstring read_proc_file(JNIEnv* env, const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return (*env)->NewStringUTF(env, "Failed to open file");

    const size_t CHUNK = 64 * 1024;
    size_t cap = CHUNK;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) { close(fd); return (*env)->NewStringUTF(env, "OOM"); }

    while (1) {
        if (len + CHUNK > cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); close(fd); return (*env)->NewStringUTF(env, "OOM"); }
            buf = nb;
        }
        ssize_t r = read(fd, buf + len, CHUNK);
        if (r <= 0) break;
        len += r;
    }
    close(fd);
    buf[len] = '\0';
    jstring res = (*env)->NewStringUTF(env, buf);
    free(buf);
    return res;
}

JNIEXPORT jstring JNICALL Java_com_example_selfmapsreader_MainActivity_readProcSelfStatus(JNIEnv* env, jobject thiz) {
    return read_proc_file(env, "/proc/self/status");
}

JNIEXPORT jstring JNICALL Java_com_example_selfmapsreader_MainActivity_readProcSelfMaps(JNIEnv* env, jobject thiz) {
    return read_proc_file(env, "/proc/self/maps");
}

JNIEXPORT jstring JNICALL Java_com_example_selfmapsreader_MainActivity_readProcSelfSmaps(JNIEnv* env, jobject thiz) {
    return read_proc_file(env, "/proc/self/smaps");
}

/* ============================================================
 * LINKER & MAPS HELPERS
 * ============================================================ */
struct dl_ctx { const char *target; uintptr_t base; int found; };

static int dl_callback(struct dl_phdr_info *info, size_t size, void *data) {
    struct dl_ctx *ctx = (struct dl_ctx *)data;
    if (info->dlpi_name && strstr(info->dlpi_name, ctx->target)) {
        ctx->base = (uintptr_t)info->dlpi_addr;
        ctx->found = 1;
        return 1;
    }
    return 0;
}

static uintptr_t get_linker_base(const char *soname) {
    struct dl_ctx ctx = { soname, 0, 0 };
    dl_iterate_phdr(dl_callback, &ctx);
    return ctx.found ? ctx.base : 0;
}

typedef struct { uintptr_t start; uintptr_t end; uintptr_t file_offset; int found; } map_entry_t;

static void find_mapping_for_segment(const char *soname, uintptr_t seg_off, map_entry_t *out) {
    out->found = 0;
    FILE *fp = fopen("/proc/self/maps", "r");
    if (!fp) return;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        if (!strstr(line, soname)) continue;
        uintptr_t s, e, off; char p[8];
        if (sscanf(line, "%lx-%lx %4s %lx", &s, &e, p, &off) != 4) continue;
        if (!strchr(p, 'x')) continue;
        if (seg_off >= off && seg_off < off + (e - s)) {
            out->start = s; out->end = e; out->file_offset = off; out->found = 1;
            break;
        }
    }
    fclose(fp);
}

/* ============================================================
 * INTEGRITY CHECK
 * ============================================================ */
static void check_library(const char *soname, const char *disk_path, char *out, size_t out_sz) {
    char *p = out;
    size_t left = out_sz;
    
    p += snprintf(p, left, "\n=== CHECK: %s ===\n", soname);
    left = out_sz - (p - out);

    uintptr_t linker_base = get_linker_base(soname);
    int fd = open(disk_path, O_RDONLY);
    if (fd < 0 || !linker_base) {
        p += snprintf(p, left, "[-] Error: FD=%d LinkerBase=%lx\n", fd, linker_base);
        if(fd >= 0) close(fd);
        return;
    }

    struct stat st; fstat(fd, &st);
    uint8_t *disk = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (disk == MAP_FAILED) return;

    Elf64_Ehdr *eh = (Elf64_Ehdr *)disk;
    Elf64_Phdr *ph = (Elf64_Phdr *)(disk + eh->e_phoff);

    for (int i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD || !(ph[i].p_flags & PF_X)) continue;

        // 1. Disk
        uint32_t disk_crc = crc32_calc(disk + ph[i].p_offset, ph[i].p_filesz);
        
        // 2. Maps (Kernel)
        map_entry_t map;
        find_mapping_for_segment(soname, ph[i].p_offset, &map);
        uintptr_t map_addr = map.found ? map.start + (ph[i].p_offset - map.file_offset) : 0;
        uint32_t map_crc = map_addr ? crc32_calc((uint8_t *)map_addr, ph[i].p_filesz) : 0;

        // 3. Linker (Real)
        uintptr_t link_addr = linker_base + ph[i].p_vaddr;
        uint32_t link_crc = crc32_calc((uint8_t *)link_addr, ph[i].p_filesz);

        p += snprintf(p, left, 
            "Seg %d (Off: %lx, Sz: %lu)\n"
            "  DISK: CRC=%08x\n"
            "  MAPS: Addr=%lx CRC=%08x %s\n"
            "  LINK: Addr=%lx CRC=%08x %s\n",
            i, (unsigned long)ph[i].p_offset, (unsigned long)ph[i].p_filesz,
            disk_crc,
            map_addr, map_crc, (map_crc == disk_crc) ? "OK" : "BAD",
            link_addr, link_crc, (link_crc == disk_crc) ? "OK" : "BAD");
        
        left = out_sz - (p - out);

        if (map_addr != link_addr) {
            p += snprintf(p, left, "  [!] DETECTED: Maps/Linker Address Mismatch\n");
            left = out_sz - (p - out);
        }
    }
    munmap(disk, st.st_size);
}

JNIEXPORT jstring JNICALL Java_com_example_selfmapsreader_MainActivity_getLibArtHash(JNIEnv *env, jobject thiz) {
    char res[8192] = {0};
    check_library("libc.so", "/apex/com.android.runtime/lib64/bionic/libc.so", res + strlen(res), sizeof(res) - strlen(res));
    check_library("libart.so", "/apex/com.android.art/lib64/libart.so", res + strlen(res), sizeof(res) - strlen(res));
    return (*env)->NewStringUTF(env, res);
}
