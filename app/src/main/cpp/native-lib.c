// native-lib.c
#include <jni.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

#include <openssl/sha.h> // requires linking against -lcrypto

// Helper: read a whole file into a malloc'd buffer. Returns pointer and sets out_size.
// Caller must free returned pointer.
static unsigned char* read_whole_file(const char* path, size_t* out_size, char** out_err) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        if (out_err) {
            size_t n = strlen(path) + 128;
            *out_err = (char*)malloc(n);
            snprintf(*out_err, n, "fopen failed for %s: %s", path, strerror(errno));
        }
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        if (out_err) {
            *out_err = strdup("fseek failed");
        }
        fclose(f);
        return NULL;
    }
    long len = ftell(f);
    if (len < 0) {
        if (out_err) *out_err = strdup("ftell failed");
        fclose(f);
        return NULL;
    }
    rewind(f);
    unsigned char* buf = (unsigned char*)malloc(len + 1);
    if (!buf) {
        if (out_err) *out_err = strdup("malloc failed");
        fclose(f);
        return NULL;
    }
    size_t read = fread(buf, 1, len, f);
    fclose(f);
    if (read != (size_t)len) {
        // partial read is OK, but note it
    }
    buf[read] = 0;
    *out_size = read;
    return buf;
}

JNIEXPORT jstring JNICALL
Java_com_example_procmapsreader_MainActivity_readProcStatusNative(JNIEnv *env, jobject thiz) {
    const char* path = "/proc/self/status";
    char* err = NULL;
    size_t sz = 0;
    unsigned char* data = read_whole_file(path, &sz, &err);
    if (!data) {
        jstring jerr = (*env)->NewStringUTF(env, err ? err : "read failed");
        if (err) free(err);
        return jerr;
    }
    jstring result = (*env)->NewStringUTF(env, (const char*)data);
    free(data);
    return result;
}

JNIEXPORT jstring JNICALL
Java_com_example_procmapsreader_MainActivity_exportProcStatusNative(JNIEnv *env, jobject thiz) {
    // For simplicity we return the same as readProcStatusNative.
    // The Java side will write it to a file and share.
    return Java_com_example_procmapsreader_MainActivity_readProcStatusNative(env, thiz);
}

// compute hex output for SHA256 digest
static void hexify(const unsigned char* in, size_t inlen, char* out) {
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < inlen; ++i) {
        out[i*2] = hex[(in[i] >> 4) & 0xF];
        out[i*2 + 1] = hex[in[i] & 0xF];
    }
    out[inlen * 2] = 0;
}

JNIEXPORT jstring JNICALL
Java_com_example_procmapsreader_MainActivity_computeLibartSha256Native(JNIEnv *env, jobject thiz) {
    // Common locations of libart
    const char* candidates[] = {
        "/apex/com.android.art/lib64/libart.so",
        "/apex/com.android.art/lib/libart.so",
        "/system/lib64/libart.so",
        "/system/lib/libart.so",
        NULL
    };

    char* last_err = NULL;
    for (int i = 0; candidates[i] != NULL; ++i) {
        const char* path = candidates[i];
        char* err = NULL;
        size_t size = 0;
        unsigned char* data = read_whole_file(path, &size, &err);
        if (!data) {
            // remember last error but try next
            if (last_err) free(last_err);
            last_err = err;
            continue;
        }

        unsigned char digest[SHA256_DIGEST_LENGTH];
        SHA256(data, size, digest);
        free(data);

        char* hex = (char*)malloc(SHA256_DIGEST_LENGTH * 2 + 1);
        hexify(digest, SHA256_DIGEST_LENGTH, hex);

        // Build response: path + newline + hex
        size_t msglen = strlen(path) + 1 + strlen(hex) + 1;
        char* outmsg = (char*)malloc(msglen);
        snprintf(outmsg, msglen, "%s\n%s", path, hex);
        free(hex);
        if (last_err) free(last_err);
        jstring jres = (*env)->NewStringUTF(env, outmsg);
        free(outmsg);
        return jres;
    }

    // none found: return last error or generic message
    const char* fallback = last_err ? last_err : "libart not found in common locations";
    jstring jerr = (*env)->NewStringUTF(env, fallback);
    if (last_err) free(last_err);
    return jerr;
}
