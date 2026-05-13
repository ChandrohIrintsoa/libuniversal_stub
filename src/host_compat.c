#include "universal_stub.h"

#include <stdarg.h>

int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
    (void)prio;

    FILE *out = stderr;
    fprintf(out, "[%s] ", tag ? tag : "android-log");

    va_list ap;
    va_start(ap, fmt);
    int ret = vfprintf(out, fmt ? fmt : "", ap);
    va_end(ap);

    fputc('\n', out);
    return ret;
}

int __system_property_get(const char *name, char *value) {
    (void)name;
    if (value) value[0] = '\0';
    return 0;
}

int __system_property_set(const char *name, const char *value) {
    (void)name;
    (void)value;
    return 0;
}
