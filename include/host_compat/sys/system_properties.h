#ifndef UNIVERSAL_STUB_SYSTEM_PROPERTIES_COMPAT_H
#define UNIVERSAL_STUB_SYSTEM_PROPERTIES_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PROP_VALUE_MAX
#define PROP_VALUE_MAX 92
#endif

int __system_property_get(const char *name, char *value);
int __system_property_set(const char *name, const char *value);

#ifdef __cplusplus
}
#endif

#endif
