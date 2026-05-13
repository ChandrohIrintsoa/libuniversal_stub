#ifndef UNIVERSAL_STUB_JNI_COMPAT_H
#define UNIVERSAL_STUB_JNI_COMPAT_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JNIEXPORT __attribute__((visibility("default")))
#define JNICALL
#define JNI_OK 0
#define JNI_TRUE 1
#define JNI_FALSE 0
#define JNI_VERSION_1_6 0x00010006

typedef int jint;
typedef int jsize;
typedef unsigned char jboolean;
typedef signed char jbyte;
typedef unsigned short jchar;
typedef short jshort;
typedef long long jlong;
typedef float jfloat;
typedef double jdouble;
typedef void *jobject;
typedef jobject jclass;
typedef jobject jstring;
typedef jobject jarray;
typedef jarray jbyteArray;
typedef jobject jobjectArray;
typedef jobject jthrowable;
typedef void *jmethodID;
typedef void *jfieldID;

typedef struct JNINativeMethod {
    char *name;
    char *signature;
    void *fnPtr;
} JNINativeMethod;

struct JNINativeInterface;
typedef const struct JNINativeInterface *JNIEnv;

struct JNINativeInterface {
    void *reserved0;
    void *reserved1;
    void *reserved2;
    void *reserved3;
    jclass (*FindClass)(JNIEnv *env, const char *name);
    jmethodID (*FromReflectedMethod)(JNIEnv *env, jobject method);
    jfieldID (*FromReflectedField)(JNIEnv *env, jobject field);
    jobject (*ToReflectedMethod)(JNIEnv *env, jclass cls, jmethodID methodID, jboolean isStatic);
    jclass (*GetSuperclass)(JNIEnv *env, jclass sub);
    jboolean (*IsAssignableFrom)(JNIEnv *env, jclass sub, jclass sup);
    jobject (*ToReflectedField)(JNIEnv *env, jclass cls, jfieldID fieldID, jboolean isStatic);
    jint (*Throw)(JNIEnv *env, jthrowable obj);
    jint (*ThrowNew)(JNIEnv *env, jclass clazz, const char *msg);
    jthrowable (*ExceptionOccurred)(JNIEnv *env);
    void (*ExceptionDescribe)(JNIEnv *env);
    void (*ExceptionClear)(JNIEnv *env);
    void (*FatalError)(JNIEnv *env, const char *msg);
    jint (*PushLocalFrame)(JNIEnv *env, jint capacity);
    jobject (*PopLocalFrame)(JNIEnv *env, jobject result);
    jobject (*NewGlobalRef)(JNIEnv *env, jobject lobj);
    void (*DeleteGlobalRef)(JNIEnv *env, jobject gref);
    void (*DeleteLocalRef)(JNIEnv *env, jobject obj);
    jboolean (*IsSameObject)(JNIEnv *env, jobject obj1, jobject obj2);
    jobject (*NewLocalRef)(JNIEnv *env, jobject ref);
    jint (*EnsureLocalCapacity)(JNIEnv *env, jint capacity);
    jobject (*AllocObject)(JNIEnv *env, jclass clazz);
    jobject (*NewObject)(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
    jobject (*NewObjectV)(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
    jobject (*NewObjectA)(JNIEnv *env, jclass clazz, jmethodID methodID, const void *args);
    jclass (*GetObjectClass)(JNIEnv *env, jobject obj);
    jboolean (*IsInstanceOf)(JNIEnv *env, jobject obj, jclass clazz);
    jmethodID (*GetMethodID)(JNIEnv *env, jclass clazz, const char *name, const char *sig);
    jobject (*CallObjectMethod)(JNIEnv *env, jobject obj, jmethodID methodID, ...);
    jobject (*CallObjectMethodV)(JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
    jobject (*CallObjectMethodA)(JNIEnv *env, jobject obj, jmethodID methodID, const void *args);
    void *reserved_call_methods[96];
    jmethodID (*GetStaticMethodID)(JNIEnv *env, jclass clazz, const char *name, const char *sig);
    jobject (*CallStaticObjectMethod)(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
    jobject (*CallStaticObjectMethodV)(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
    jobject (*CallStaticObjectMethodA)(JNIEnv *env, jclass clazz, jmethodID methodID, const void *args);
    void *reserved_static_methods[32];
    jstring (*NewString)(JNIEnv *env, const jchar *unicode, jsize len);
    jsize (*GetStringLength)(JNIEnv *env, jstring string);
    const jchar *(*GetStringChars)(JNIEnv *env, jstring string, jboolean *isCopy);
    void (*ReleaseStringChars)(JNIEnv *env, jstring string, const jchar *chars);
    jstring (*NewStringUTF)(JNIEnv *env, const char *utf);
    jsize (*GetStringUTFLength)(JNIEnv *env, jstring string);
    const char *(*GetStringUTFChars)(JNIEnv *env, jstring string, jboolean *isCopy);
    void (*ReleaseStringUTFChars)(JNIEnv *env, jstring string, const char *utf);
    void *reserved_arrays[64];
    jint (*RegisterNatives)(JNIEnv *env, jclass clazz, const JNINativeMethod *methods, jint nMethods);
};

struct JNIInvokeInterface;
typedef const struct JNIInvokeInterface *JavaVM;

typedef struct JavaVMAttachArgs {
    jint version;
    char *name;
    jobject group;
} JavaVMAttachArgs;

struct JNIInvokeInterface {
    void *reserved0;
    void *reserved1;
    void *reserved2;
    jint (*DestroyJavaVM)(JavaVM *vm);
    jint (*AttachCurrentThread)(JavaVM *vm, JNIEnv **p_env, void *thr_args);
    jint (*DetachCurrentThread)(JavaVM *vm);
    jint (*GetEnv)(JavaVM *vm, void **env, jint version);
};

#ifdef __cplusplus
}
#endif

#endif
