#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>
#include <sys/stat.h>

// Read /proc/self/status and return contents
JNIEXPORT jstring JNICALL
Java_com_example_selfmapsreader_MainActivity_readProcSelfStatus(JNIEnv* env, jobject thiz) {
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) return (*env)->NewStringUTF(env, "Failed to open /proc/self/status");

    char* buf = NULL;
    size_t len = 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        size_t l = strlen(line);
        buf = realloc(buf, len + l + 1);
        memcpy(buf + len, line, l);
        len += l;
        buf[len] = 0;
    }
    fclose(f);

    jstring result = (*env)->NewStringUTF(env, buf ? buf : "");
    free(buf);
    return result;
}

// Export /proc/self/status to given path
JNIEXPORT jboolean JNICALL
Java_com_example_selfmapsreader_MainActivity_exportProcSelfStatus(JNIEnv* env, jobject thiz, jstring jpath) {
    const char* path = (*env)->GetStringUTFChars(env, jpath, 0);
    FILE* in = fopen("/proc/self/status", "r");
    FILE* out = fopen(path, "w");
    if (!in || !out) {
        if (in) fclose(in);
        if (out) fclose(out);
        (*env)->ReleaseStringUTFChars(env, jpath, path);
        return JNI_FALSE;
    }

    char buf[512];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, n, out);

    fclose(in);
    fclose(out);
    (*env)->ReleaseStringUTFChars(env, jpath, path);
    return JNI_TRUE;
}

// Compute SHA256 of /apex/com.android.art/lib64/libart.so
JNIEXPORT jstring JNICALL
Java_com_example_selfmapsreader_MainActivity_getLibArtHash(JNIEnv* env, jobject thiz) {
    const char* path = "/apex/com.android.art/lib64/libart.so";
    FILE* f = fopen(path, "rb");
    if (!f) return (*env)->NewStringUTF(env, "Failed to open libart.so");

    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    unsigned char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        SHA256_Update(&ctx, buf, n);

    fclose(f);

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &ctx);

    char hex[SHA256_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(hex + i * 2, "%02x", hash[i]);
    hex[SHA256_DIGEST_LENGTH * 2] = 0;

    return (*env)->NewStringUTF(env, hex);
}
