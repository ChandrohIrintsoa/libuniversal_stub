#include "universal_stub.h"

static missing_lib_t g_libs[MAX_LIBS];
static int g_lib_count = 0;
static pthread_mutex_t g_lib_mutex = PTHREAD_MUTEX_INITIALIZER;

// Known problematic libraries - expanded and updated for 2025+
static const char *KNOWN_MISSING_LIBS[] = {
    // Pairip / Google Play Integrity
    "libpairipcore.so",
    "libpairipcore_shim.so",
    "libpairip.so",
    "libpairip_jni.so",
    "libpairip_security.so",
    "libpairip_verification.so",
    "libpairip_drm.so",
    "libpairip_integrity.so",
    "libpairip_attestation.so",
    "libpairip_protection.so",
    "libpairip_antitamper.so",
    "libpairip_anticheat.so",
    "libpairip_rootcheck.so",
    "libpairip_emulator.so",
    "libpairip_debug.so",
    "libpairip_hook.so",
    "libpairip_inject.so",
    "libpairip_monitor.so",
    "libpairip_analytics.so",
    "libpairip_telemetry.so",
    "libpairip_network.so",
    "libpairip_crypto.so",
    "libpairip_storage.so",
    "libpairip_utils.so",

    // Google SafetyNet / Play Services
    "libsafetynet.so",
    "libsafetynet_jni.so",
    "libplay_integrity.so",
    "libplay_integrity_jni.so",
    "libgms_security.so",
    "libgms_integrity.so",
    "libgms_attestation.so",
    "libgms_safetynet.so",
    "libgms_droidguard.so",
    "libfirebase_auth.so",
    "libfirebase_app.so",
    "libfirebase_analytics.so",
    "libfirebase_crashlytics.so",
    "libfirebase_performance.so",
    "libfirebase_messaging.so",
    "libfirebase_app_check.so",

    // DRM
    "libdrm.so",
    "libwidevine.so",
    "libwidevinecdm.so",
    "libwvdrmengine.so",
    "libwvm.so",
    "liboemcrypto.so",
    "libmediadrm.so",
    "libdrmclearkeyplugin.so",
    "libdrmwvmplugin.so",
    "libwvcrypto.so",

    // Security / Anti-tamper
    "libsecurity.so",
    "libantitamper.so",
    "libanticheat.so",
    "librootcheck.so",
    "libemulatorcheck.so",
    "libdebugger.so",
    "libhookdetector.so",
    "libinjectdetector.so",
    "libfridadetector.so",
    "libxposeddetector.so",
    "libmagiskdetector.so",
    "libsupersudetector.so",
    "libshamikoodetector.so",
    "librirdetector.so",
    "libkerneldetector.so",
    "libenvdetector.so",
    "libprotections.so",
    "libappdome.so",
    "libdexguard.so",
    "libarxan.so",
    "libirxo.so",
    "libguardso.so",
    "libvkey.so",
    "libsecsync.so",

    // Certificate / SSL pinning
    "libssl_pinning.so",
    "libcertificate.so",
    "libcert_pinning.so",
    "libssl_verify.so",
    "libtls_pinning.so",
    "libtrustkit.so",
    "libnsc.so",

    // Analytics / Tracking
    "libanalytics.so",
    "libtracking.so",
    "libtelemetry.so",
    "libcrashreport.so",
    "libbugreport.so",
    "libmetrics.so",
    "libperformance.so",
    "libadjust.so",
    "libappsflyer.so",
    "libbranch.so",

    // Licensing
    "liblicense.so",
    "liblicensing.so",
    "libactivation.so",
    "libregistration.so",
    "libsubscription.so",
    "libiap.so",
    "libbilling.so",
    "libplay_billing.so",

    // Social / Auth
    "libfacebook.so",
    "libfb.so",
    "libtwitter.so",
    "libgoogle_signin.so",
    "libgoogle_auth.so",
    "liboauth.so",
    "libsso.so",
    "libtiktok.so",
    "libsnapchat.so",

    // Game engines
    "libunity.so",
    "libunity_security.so",
    "libil2cpp.so",
    "libmono.so",
    "libmonobdwgc-2.0.so",
    "libunreal.so",
    "libcocos2d.so",
    "libcocos2djs.so",
    "libcocos_creator.so",

    // AI/ML
    "libtensorflow.so",
    "libtensorflowlite.so",
    "libpytorch.so",
    "libtorch.so",
    "libonnx.so",
    "libtflite.so",
    "libml.so",
    "libncnn.so",
    "libmnn.so",

    // Other common missing libs
    "libcurl.so",
    "libssl.so",
    "libcrypto.so",
    "libz.so",
    "libpng.so",
    "libjpeg.so",
    "libfreetype.so",
    "libharfbuzz.so",
    "libicu.so",
    "libsqlite.so",
    "libprotobuf.so",
    "libgrpc.so",
    "libopencv.so",
    "libavcodec.so",
    "libavformat.so",
    "libswresample.so",
    "libswscale.so",
    "libavutil.so",

    // 2025+ additions
    "libplay_integrity_v2.so",
    "libgms_integrity_v2.so",
    "libpairip_v2.so",
    "libpairipcore_v2.so",
    "libandroid_integrity.so",
    "libdevice_attestation.so",
    "libapp_attestation.so",
    "libkernelsu_detector.so",
    "libapatch_detector.so",
    "libmagisk_delta_detector.so",
    "libshamiko_detector.so",
    "libzksig.so",
    "libre_kotlin.so",
    "libre_coroutine.so",
    "libcompose.so",
    "libaccompanist.so",
    "libcamerax.so",
    "libbiometric.so",
    "libsecurity_crypto.so",
    "libidentity_credential.so",

    NULL
};

// Focused security symbol patterns (fixed: removed overly broad patterns)
// Updated for 2025+ detection frameworks
static const char *SECURITY_SYMBOL_PATTERNS[] = {
    "pairip",
    "safetynet",
    "integrity",
    "attestation",
    "antitamper",
    "anticheat",
    "rootcheck",
    "emulatorcheck",
    "hookdetect",
    "injectdetect",
    "fridadetect",
    "xposeddetect",
    "magiskdetect",
    "ssl_pinning",
    "cert_pinning",
    "trustkit",
    "dexguard",
    "arxan",
    "irxo",
    "vkey",
    "secsync",
    "shamiko",
    "kernelsu",
    "apatch",
    "zksig",
    "device_attest",
    "app_attest",
    NULL
};

int is_known_missing_lib(const char *name) {
    if (!name) return 0;
    for (int i = 0; KNOWN_MISSING_LIBS[i]; i++) {
        if (strstr(name, KNOWN_MISSING_LIBS[i]) != NULL) {
            return 1;
        }
    }
    return 0;
}

int is_security_symbol(const char *name) {
    if (!name) return 0;

    // Convert to lowercase for case-insensitive matching
    char lower_name[MAX_SYMBOL_LEN];
    int j = 0;
    for (const char *p = name; *p && j < MAX_SYMBOL_LEN - 1; p++) {
        lower_name[j++] = tolower(*p);
    }
    lower_name[j] = '\0';

    for (int i = 0; SECURITY_SYMBOL_PATTERNS[i]; i++) {
        if (strstr(lower_name, SECURITY_SYMBOL_PATTERNS[i]) != NULL) {
            return 1;
        }
    }
    return 0;
}

static void *create_fake_elf(const char *name) {
    // Create a minimal ELF header for the fake library
    size_t size = sizeof(Elf64_Ehdr) + sizeof(Elf64_Shdr) * 3 + 4096;
    void *mem = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (mem == MAP_FAILED) return NULL;

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)mem;
    memset(ehdr, 0, sizeof(Elf64_Ehdr));

    // ELF magic
    ehdr->e_ident[EI_MAG0] = ELFMAG0;
    ehdr->e_ident[EI_MAG1] = ELFMAG1;
    ehdr->e_ident[EI_MAG2] = ELFMAG2;
    ehdr->e_ident[EI_MAG3] = ELFMAG3;
    ehdr->e_ident[EI_CLASS] = ELFCLASS64;
    ehdr->e_ident[EI_DATA] = ELFDATA2LSB;
    ehdr->e_ident[EI_VERSION] = EV_CURRENT;
    ehdr->e_ident[EI_OSABI] = ELFOSABI_LINUX;

    ehdr->e_type = ET_DYN;
    ehdr->e_machine = EM_AARCH64;
    ehdr->e_version = EV_CURRENT;
    ehdr->e_entry = 0;
    ehdr->e_phoff = 0;
    ehdr->e_shoff = sizeof(Elf64_Ehdr);
    ehdr->e_flags = 0;
    ehdr->e_ehsize = sizeof(Elf64_Ehdr);
    ehdr->e_phentsize = sizeof(Elf64_Phdr);
    ehdr->e_phnum = 0;
    ehdr->e_shentsize = sizeof(Elf64_Shdr);
    ehdr->e_shnum = 3;
    ehdr->e_shstrndx = 2;

    // Section headers
    Elf64_Shdr *shdrs = (Elf64_Shdr *)((char *)mem + sizeof(Elf64_Ehdr));
    memset(shdrs, 0, sizeof(Elf64_Shdr) * 3);

    // .null section
    shdrs[0].sh_name = 0;
    shdrs[0].sh_type = SHT_NULL;

    // .symtab section
    shdrs[1].sh_name = 1;  // offset in string table
    shdrs[1].sh_type = SHT_SYMTAB;
    shdrs[1].sh_offset = sizeof(Elf64_Ehdr) + sizeof(Elf64_Shdr) * 3;
    shdrs[1].sh_size = 0;
    shdrs[1].sh_link = 2;  // string table
    shdrs[1].sh_entsize = sizeof(Elf64_Sym);

    // .strtab section
    shdrs[2].sh_name = 9;
    shdrs[2].sh_type = SHT_STRTAB;
    shdrs[2].sh_offset = shdrs[1].sh_offset;
    shdrs[2].sh_size = 256;

    // String table content
    char *strtab = (char *)mem + shdrs[2].sh_offset;
    strcpy(strtab, "\0.symtab\0.strtab\0");

    LOGI("[LIB] Created fake ELF for %s at %p", name, mem);
    return mem;
}

missing_lib_t *find_or_create_stub_lib(const char *name) {
    if (!name) return NULL;

    pthread_mutex_lock(&g_lib_mutex);

    // Check if already exists
    for (int i = 0; i < g_lib_count; i++) {
        if (strcmp(g_libs[i].name, name) == 0) {
            g_libs[i].ref_count++;
            pthread_mutex_unlock(&g_lib_mutex);
            return &g_libs[i];
        }
    }

    if (g_lib_count >= MAX_LIBS) {
        LOGE("[LIB] Library table full!");
        pthread_mutex_unlock(&g_lib_mutex);
        return NULL;
    }

    missing_lib_t *lib = &g_libs[g_lib_count];
    memset(lib, 0, sizeof(missing_lib_t));

    strncpy(lib->name, name, MAX_LIB_NAME - 1);
    snprintf(lib->replacement, MAX_LIB_NAME - 1, "stub://%s", name);
    lib->is_stubbed = 1;
    lib->is_fake = 1;
    lib->ref_count = 1;
    lib->load_time = time(NULL);

    // Create fake ELF
    lib->base_addr = create_fake_elf(name);
    if (lib->base_addr) {
        lib->elf_header = (Elf64_Ehdr *)lib->base_addr;
        lib->section_headers = (Elf64_Shdr *)((char *)lib->base_addr + sizeof(Elf64_Ehdr));
        lib->size = sizeof(Elf64_Ehdr) + sizeof(Elf64_Shdr) * 3 + 4096;
    }

    // The handle is the pointer to the library entry itself
    lib->handle = lib;

    g_lib_count++;

    LOGI("[LIB] Created stub library #%d: %s (handle=%p, base=%p)",
         g_lib_count, name, lib, lib->base_addr);

    pthread_mutex_unlock(&g_lib_mutex);
    return lib;
}

void *get_lib_base(const char *name) {
    pthread_mutex_lock(&g_lib_mutex);

    for (int i = 0; i < g_lib_count; i++) {
        if (strcmp(g_libs[i].name, name) == 0) {
            void *base = g_libs[i].base_addr;
            pthread_mutex_unlock(&g_lib_mutex);
            return base;
        }
    }

    pthread_mutex_unlock(&g_lib_mutex);
    return NULL;
}

int enumerate_loaded_libs(void (*callback)(const char*, void*, size_t)) {
    if (!callback) return 0;

    pthread_mutex_lock(&g_lib_mutex);

    int count = 0;
    for (int i = 0; i < g_lib_count; i++) {
        callback(g_libs[i].name, g_libs[i].base_addr, g_libs[i].size);
        count++;
    }

    pthread_mutex_unlock(&g_lib_mutex);
    return count;
}

int replace_lib_reference(const char *old_name, const char *new_name) {
    pthread_mutex_lock(&g_lib_mutex);

    for (int i = 0; i < g_lib_count; i++) {
        if (strcmp(g_libs[i].name, old_name) == 0) {
            LOGI("[LIB] Replacing reference %s -> %s", old_name, new_name);
            strncpy(g_libs[i].replacement, new_name, MAX_LIB_NAME - 1);
            pthread_mutex_unlock(&g_lib_mutex);
            return 1;
        }
    }

    pthread_mutex_unlock(&g_lib_mutex);
    return 0;
}

void *create_virtual_lib(const char *name) {
    missing_lib_t *lib = find_or_create_stub_lib(name);
    return lib ? lib->handle : NULL;
}

void dump_stubbed_libs(void) {
    pthread_mutex_lock(&g_lib_mutex);

    LOGI("[DUMP] === Stubbed Libraries (%d total) ===", g_lib_count);
    for (int i = 0; i < g_lib_count; i++) {
        LOGI("[DUMP] #%d: '%s' -> '%s' refs=%d fake=%d base=%p",
             i, g_libs[i].name, g_libs[i].replacement,
             g_libs[i].ref_count, g_libs[i].is_fake, g_libs[i].base_addr);
    }

    pthread_mutex_unlock(&g_lib_mutex);
}

int is_stub_handle(void *handle) {
    pthread_mutex_lock(&g_lib_mutex);

    for (int i = 0; i < g_lib_count; i++) {
        if (g_libs[i].handle == handle || &g_libs[i] == handle) {
            pthread_mutex_unlock(&g_lib_mutex);
            return 1;
        }
    }

    pthread_mutex_unlock(&g_lib_mutex);
    return 0;
}

missing_lib_t *get_lib_by_handle(void *handle) {
    if (!handle) return NULL;

    pthread_mutex_lock(&g_lib_mutex);

    for (int i = 0; i < g_lib_count; i++) {
        if (g_libs[i].handle == handle || &g_libs[i] == handle) {
            pthread_mutex_unlock(&g_lib_mutex);
            return &g_libs[i];
        }
    }

    pthread_mutex_unlock(&g_lib_mutex);
    return NULL;
}
