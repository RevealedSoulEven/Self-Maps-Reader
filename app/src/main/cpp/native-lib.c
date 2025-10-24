\
    #include <jni.h>
    #include <string.h>
    #include <stdlib.h>
    #include <stdio.h>

    // JNI function: read /proc/self/maps and return as Java String
    JNIEXPORT jstring JNICALL
    Java_com_example_procmapsreader_MainActivity_readMaps(JNIEnv* env, jobject thiz) {
        const char *path = "/proc/self/maps";
        FILE *f = fopen(path, "r");
        if (!f) {
            const char *err = "Unable to open /proc/self/maps";
            return (*env)->NewStringUTF(env, err);
        }

        // read file into buffer dynamically
        size_t cap = 4096;
        size_t len = 0;
        char *buf = (char*)malloc(cap);
        if (!buf) {
            fclose(f);
            return (*env)->NewStringUTF(env, "Out of memory");
        }

        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            size_t l = strlen(line);
            if (len + l + 1 >= cap) {
                cap *= 2;
                char *nb = (char*)realloc(buf, cap);
                if (!nb) break;
                buf = nb;
            }
            memcpy(buf + len, line, l);
            len += l;
            buf[len] = '\\0';
        }

        fclose(f);
        jstring result = (*env)->NewStringUTF(env, buf ? buf : "");
        free(buf);
        return result;
    }
