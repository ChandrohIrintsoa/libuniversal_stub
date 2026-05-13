#include "universal_stub.h"

// ==================== PAIRIP STUBS ====================

JNIEXPORT jobject JNICALL Java_com_pairip_Pairip_checkIntegrity(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] Pairip.checkIntegrity() -> PASS");
    jclass bool_cls = (*env)->FindClass(env, "java/lang/Boolean");
    if (!bool_cls) return NULL;
    jmethodID value_of = (*env)->GetStaticMethodID(env, bool_cls, "valueOf", "(Z)Ljava/lang/Boolean;");
    if (!value_of) { (*env)->DeleteLocalRef(env, bool_cls); return NULL; }
    jobject result = (*env)->CallStaticObjectMethod(env, bool_cls, value_of, JNI_TRUE);
    (*env)->DeleteLocalRef(env, bool_cls);
    return result;
}

JNIEXPORT jboolean JNICALL Java_com_pairip_Pairip_isDeviceSecure(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] Pairip.isDeviceSecure() -> true");
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_pairip_Pairip_verifySignature(
    JNIEnv *env, jobject thiz, jstring signature) {
    LOGI("[JNI-STUB] Pairip.verifySignature() -> true");
    return JNI_TRUE;
}

JNIEXPORT jint JNICALL Java_com_pairip_Pairip_getSecurityLevel(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] Pairip.getSecurityLevel() -> L3 (0x3)");
    return 3; // L3
}

JNIEXPORT jboolean JNICALL Java_com_pairip_Pairip_checkRoot(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] Pairip.checkRoot() -> false (hidden)");
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_pairip_Pairip_checkEmulator(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] Pairip.checkEmulator() -> false (spoofed)");
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_pairip_Pairip_checkDebug(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] Pairip.checkDebug() -> false (hidden)");
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_pairip_Pairip_checkHook(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] Pairip.checkHook() -> false (hidden)");
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_pairip_Pairip_checkTamper(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] Pairip.checkTamper() -> false (patched)");
    return JNI_FALSE;
}

// ==================== SAFETYNET STUBS ====================

JNIEXPORT jstring JNICALL Java_com_google_android_gms_security_SafetyNetApi_getJwsResult(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] SafetyNetApi.getJwsResult() -> dummy JWS");
    const char *jws = "{\"ctsProfileMatch\":true,\"basicIntegrity\":true,\"evaluationType\":\"BASIC\"}";
    return (*env)->NewStringUTF(env, jws);
}

JNIEXPORT jobject JNICALL Java_com_google_android_gms_security_SafetyNetApi_attest(
    JNIEnv *env, jobject thiz, jbyteArray nonce, jstring apiKey) {
    LOGI("[JNI-STUB] SafetyNetApi.attest() -> dummy attestation");
    // Return a dummy Status object (simplified - avoid class-not-found crashes)
    jclass result_cls = (*env)->FindClass(env, "com/google/android/gms/common/api/Status");
    if (!result_cls) {
        LOGW("[JNI-STUB] Status class not found, returning null");
        return NULL;
    }
    jmethodID ctor = (*env)->GetMethodID(env, result_cls, "<init>", "(I)V");
    if (!ctor) {
        (*env)->DeleteLocalRef(env, result_cls);
        return NULL;
    }
    jobject status = (*env)->NewObject(env, result_cls, ctor, 0); // SUCCESS
    (*env)->DeleteLocalRef(env, result_cls);
    return status;
}

// ==================== PLAY INTEGRITY STUBS ====================

JNIEXPORT jobject JNICALL Java_com_google_android_play_core_integrity_IntegrityManager_requestIntegrityToken(
    JNIEnv *env, jobject thiz, jobject request) {
    LOGI("[JNI-STUB] IntegrityManager.requestIntegrityToken() -> dummy token");
    // Simplified: return null to avoid class-not-found crashes
    // The Java-side fallback should handle null gracefully
    return NULL;
}

// ==================== STANDARD INTEGRITY (2024+ API) ====================

JNIEXPORT jobject JNICALL Java_com_google_android_play_core_integrity_StandardIntegrityManager_requestIntegrityToken(
    JNIEnv *env, jobject thiz, jobject request) {
    LOGI("[JNI-STUB] StandardIntegrityManager.requestIntegrityToken() -> null (deferred to Java)");
    return NULL;
}

// ==================== SECURITY CHECKER STUBS ====================

JNIEXPORT jboolean JNICALL Java_com_security_SecurityChecker_isRooted(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] SecurityChecker.isRooted() -> false");
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_security_SecurityChecker_isEmulator(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] SecurityChecker.isEmulator() -> false");
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_security_SecurityChecker_isDebugged(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] SecurityChecker.isDebugged() -> false");
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_security_SecurityChecker_isTampered(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] SecurityChecker.isTampered() -> false");
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_security_SecurityChecker_isHooked(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] SecurityChecker.isHooked() -> false");
    return JNI_FALSE;
}

// ==================== ROOT CHECKER STUBS ====================

JNIEXPORT jboolean JNICALL Java_com_root_RootChecker_isRooted(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] RootChecker.isRooted() -> false");
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_root_RootChecker_checkSuBinary(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] RootChecker.checkSuBinary() -> false");
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_root_RootChecker_checkTestKeys(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] RootChecker.checkTestKeys() -> false");
    return JNI_FALSE;
}

// ==================== DEBUG DETECTOR STUBS ====================

JNIEXPORT jboolean JNICALL Java_com_debug_DebugDetector_isDebuggerAttached(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] DebugDetector.isDebuggerAttached() -> false");
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_debug_DebugDetector_isDebugBuild(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] DebugDetector.isDebugBuild() -> false");
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_debug_DebugDetector_isBeingDebugged(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] DebugDetector.isBeingDebugged() -> false");
    return JNI_FALSE;
}

// ==================== DRM STUBS ====================

JNIEXPORT jint JNICALL Java_com_drm_DrmManager_initialize(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] DrmManager.initialize() -> DRM_SUCCESS");
    return 0; // DRM_SUCCESS
}

JNIEXPORT jint JNICALL Java_com_drm_DrmManager_acquireLicense(
    JNIEnv *env, jobject thiz, jobject licenseRequest) {
    LOGI("[JNI-STUB] DrmManager.acquireLicense() -> DRM_SUCCESS");
    return 0;
}

JNIEXPORT jboolean JNICALL Java_com_drm_DrmManager_isProvisioned(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] DrmManager.isProvisioned() -> true");
    return JNI_TRUE;
}

// ==================== LICENSE STUBS ====================

JNIEXPORT jboolean JNICALL Java_com_license_LicenseManager_verifyLicense(
    JNIEnv *env, jobject thiz, jstring license) {
    LOGI("[JNI-STUB] LicenseManager.verifyLicense() -> true");
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_license_LicenseManager_isLicensed(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] LicenseManager.isLicensed() -> true");
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_license_LicenseManager_isPremium(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] LicenseManager.isPremium() -> true");
    return JNI_TRUE;
}

JNIEXPORT jstring JNICALL Java_com_license_LicenseManager_getLicenseType(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] LicenseManager.getLicenseType() -> 'premium'");
    return (*env)->NewStringUTF(env, "premium");
}

JNIEXPORT jlong JNICALL Java_com_license_LicenseManager_getExpiryDate(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] LicenseManager.getExpiryDate() -> 9999999999");
    return 9999999999LL;
}

// ==================== SSL PINNING BYPASS STUBS ====================

JNIEXPORT void JNICALL Java_com_ssl_SslPinning_disablePinning(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] SslPinning.disablePinning() -> done");
}

JNIEXPORT jboolean JNICALL Java_com_ssl_SslPinning_isPinningEnabled(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] SslPinning.isPinningEnabled() -> false");
    return JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_ssl_SslPinning_addTrustedHost(
    JNIEnv *env, jobject thiz, jstring host) {
    LOGI("[JNI-STUB] SslPinning.addTrustedHost() -> done");
}

// ==================== CERTIFICATE STUBS ====================

JNIEXPORT jboolean JNICALL Java_com_cert_CertificateValidator_validate(
    JNIEnv *env, jobject thiz, jobject cert) {
    LOGI("[JNI-STUB] CertificateValidator.validate() -> true");
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_cert_CertificateValidator_verifyChain(
    JNIEnv *env, jobject thiz, jobjectArray chain) {
    LOGI("[JNI-STUB] CertificateValidator.verifyChain() -> true");
    return JNI_TRUE;
}

// ==================== ANTI-CHEAT STUBS ====================

JNIEXPORT jboolean JNICALL Java_com_anticheat_AntiCheat_detectCheat(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] AntiCheat.detectCheat() -> false");
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_anticheat_AntiCheat_detectMod(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] AntiCheat.detectMod() -> false");
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_anticheat_AntiCheat_detectSpeedHack(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] AntiCheat.detectSpeedHack() -> false");
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_anticheat_AntiCheat_detectMemoryHack(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] AntiCheat.detectMemoryHack() -> false");
    return JNI_FALSE;
}

// ==================== ANALYTICS STUBS ====================

JNIEXPORT void JNICALL Java_com_analytics_Analytics_trackEvent(
    JNIEnv *env, jobject thiz, jstring event, jstring params) {
    LOGI("[JNI-STUB] Analytics.trackEvent() -> skipped");
}

JNIEXPORT void JNICALL Java_com_analytics_Analytics_trackScreen(
    JNIEnv *env, jobject thiz, jstring screen) {
    LOGI("[JNI-STUB] Analytics.trackScreen() -> skipped");
}

JNIEXPORT void JNICALL Java_com_analytics_Analytics_setUserProperty(
    JNIEnv *env, jobject thiz, jstring key, jstring value) {
    LOGI("[JNI-STUB] Analytics.setUserProperty() -> skipped");
}

// ==================== KERNELSU / APATCH STUBS (2024+) ====================

JNIEXPORT jboolean JNICALL Java_com_kernelsu_KernelSU_isKernelSUAvaliable(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] KernelSU.isKernelSUAvaliable() -> false");
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_kernelsu_KernelSU_isSafeMode(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] KernelSU.isSafeMode() -> false");
    return JNI_FALSE;
}

// ==================== ANDROID 15+ / IDENTITY CREDENTIAL STUBS ====================

JNIEXPORT jboolean JNICALL Java_com_android_identity_credential_CredentialStore_isHardwareBacked(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] CredentialStore.isHardwareBacked() -> true");
    return JNI_TRUE;
}

JNIEXPORT jobject JNICALL Java_com_android_identity_credential_CredentialStore_createCredential(
    JNIEnv *env, jobject thiz, jstring credentialId) {
    LOGI("[JNI-STUB] CredentialStore.createCredential() -> null (deferred)");
    return NULL;
}

// ==================== PLAY INTEGRITY V2 STUBS (2025+) ====================

JNIEXPORT jobject JNICALL Java_com_google_android_play_core_integrity_IntegrityTokenResponse_showDialog(
    JNIEnv *env, jobject thiz, jstring dialogType) {
    LOGI("[JNI-STUB] IntegrityTokenResponse.showDialog() -> null");
    return NULL;
}

// ==================== SHAMIKO / HIDE MY APP LIST STUBS ====================

JNIEXPORT jboolean JNICALL Java_com_tsng_hidemyapplist_MyApplication_isHidden(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] HideMyAppList.isHidden() -> false");
    return JNI_FALSE;
}

// ==================== BIOMETRIC PROMPT STUB ====================

JNIEXPORT jboolean JNICALL Java_android_hardware_biometrics_BiometricPrompt_canAuthenticate(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] BiometricPrompt.canAuthenticate() -> true");
    return JNI_TRUE;
}

// ==================== MAGISK DELTA / KITSUNE STUBS ====================

JNIEXPORT jboolean JNICALL Java_com_topjohnwu_magisk_CoreRootService_isRootAvailable(
    JNIEnv *env, jobject thiz) {
    LOGI("[JNI-STUB] CoreRootService.isRootAvailable() -> false");
    return JNI_FALSE;
}

// ==================== GENERIC JNI STUB ====================

// This is a catch-all for any JNI method that isn't explicitly stubbed
JNIEXPORT jobject JNICALL Java_com_universalstub_GenericStub_stubMethod(
    JNIEnv *env, jobject thiz, jobjectArray args) {
    LOGI("[JNI-STUB] GenericStub.stubMethod() -> null");
    return NULL;
}
