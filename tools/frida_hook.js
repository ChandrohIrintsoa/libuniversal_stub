// UniversalStub Frida Script v5.0.0-RE
// Usage: frida -U -f com.example.app -l frida_hook.js --no-pause

console.log("[*] UniversalStub Frida Companion v5.0.0-RE loaded");

// ==================== UTILITY FUNCTIONS ====================

function hexDump(addr, size) {
    console.log("[*] Hex dump at " + addr + " (" + size + " bytes):");
    for (let i = 0; i < size; i += 16) {
        let hex = "";
        let ascii = "";
        for (let j = 0; j < 16 && i + j < size; j++) {
            let byte = Memory.readU8(addr.add(i + j));
            hex += byte.toString(16).padStart(2, '0') + " ";
            ascii += (byte >= 32 && byte < 127) ? String.fromCharCode(byte) : ".";
        }
        console.log("  " + addr.add(i) + "  " + hex.padEnd(48) + " |" + ascii + "|");
    }
}

function printBacktrace(context) {
    console.log("[*] Backtrace:");
    let bt = Thread.backtrace(context, Backtracer.ACCURATE);
    bt.forEach((addr, i) => {
        let sym = DebugSymbol.fromAddress(addr);
        console.log("  #" + i + " " + addr + " " + sym);
    });
}

function hookFunction(name, module, callback) {
    let addr = Module.findExportByName(module, name);
    if (!addr) {
        console.log("[!] " + name + " not found in " + module);
        return null;
    }

    Interceptor.attach(addr, {
        onEnter: function(args) {
            if (callback && callback.onEnter) {
                callback.onEnter.call(this, args);
            }
        },
        onLeave: function(retval) {
            if (callback && callback.onLeave) {
                callback.onLeave.call(this, retval);
            }
        }
    });

    console.log("[*] Hooked " + name + " at " + addr);
    return addr;
}

// ==================== DLOPEN/DLSYM HOOKING ====================

function hookDynamicLoader() {
    console.log("[*] Hooking dynamic loader...");

    // Hook android_dlopen_ext (Android 7+)
    let android_dlopen_ext = Module.findExportByName(null, "android_dlopen_ext");
    if (android_dlopen_ext) {
        Interceptor.attach(android_dlopen_ext, {
            onEnter: function(args) {
                let path = args[0].isNull() ? null : Memory.readCString(args[0]);
                let flags = args[1].toInt32();
                console.log("[DLOPEN-EXT] Loading: " + path + " (flags=0x" + flags.toString(16) + ")");
                this.path = path;
            },
            onLeave: function(retval) {
                if (retval.isNull()) {
                    console.log("[DLOPEN-EXT] Failed: " + this.path);
                } else {
                    console.log("[DLOPEN-EXT] Success: " + this.path + " -> " + retval);
                }
            }
        });
    }

    // Hook dlopen
    let dlopen = Module.findExportByName(null, "dlopen");
    if (dlopen) {
        Interceptor.attach(dlopen, {
            onEnter: function(args) {
                let path = args[0].isNull() ? null : Memory.readCString(args[0]);
                let flags = args[1].toInt32();
                console.log("[DLOPEN] Loading: " + path + " (flags=0x" + flags.toString(16) + ")");
                this.path = path;
            },
            onLeave: function(retval) {
                if (retval.isNull()) {
                    console.log("[DLOPEN] Failed: " + this.path);
                } else {
                    console.log("[DLOPEN] Success: " + this.path + " -> " + retval);
                }
            }
        });
    }

    // Hook dlsym
    let dlsym = Module.findExportByName(null, "dlsym");
    if (dlsym) {
        Interceptor.attach(dlsym, {
            onEnter: function(args) {
                let handle = args[0];
                let symbol = args[1].isNull() ? null : Memory.readCString(args[1]);
                console.log("[DLSYM] Looking up: " + symbol + " from " + handle);
                this.symbol = symbol;
            },
            onLeave: function(retval) {
                if (retval.isNull()) {
                    console.log("[DLSYM] Not found: " + this.symbol);
                }
            }
        });
    }
}

// ==================== SECURITY CHECK BYPASS ====================

function bypassSecurityChecks() {
    console.log("[*] Installing security bypass hooks...");

    // Bypass root detection via access()
    let access = Module.findExportByName(null, "access");
    if (access) {
        Interceptor.attach(access, {
            onEnter: function(args) {
                let path = args[0].isNull() ? null : Memory.readCString(args[0]);
                if (path && (path.indexOf("/su") !== -1 ||
                    path.indexOf("magisk") !== -1 ||
                    path.indexOf("Superuser") !== -1 ||
                    path.indexOf("kernelsu") !== -1 ||
                    path.indexOf("apatch") !== -1)) {
                    console.log("[BYPASS] Blocking access to: " + path);
                    this.block = true;
                }
            },
            onLeave: function(retval) {
                if (this.block) {
                    retval.replace(-1);
                }
            }
        });
    }

    // Bypass root detection via stat()
    let stat = Module.findExportByName(null, "stat");
    if (stat) {
        Interceptor.attach(stat, {
            onEnter: function(args) {
                let path = args[0].isNull() ? null : Memory.readCString(args[0]);
                if (path && (path.indexOf("/su") !== -1 ||
                    path.indexOf("magisk") !== -1 ||
                    path.indexOf("kernelsu") !== -1)) {
                    console.log("[BYPASS] Blocking stat: " + path);
                    this.block = true;
                }
            },
            onLeave: function(retval) {
                if (this.block) {
                    retval.replace(-1);
                }
            }
        });
    }

    // Bypass ptrace anti-debug
    let ptrace = Module.findExportByName(null, "ptrace");
    if (ptrace) {
        Interceptor.attach(ptrace, {
            onEnter: function(args) {
                let request = args[0].toInt32();
                if (request === 0) { // PTRACE_TRACEME
                    console.log("[BYPASS] Blocking PTRACE_TRACEME");
                    this.block = true;
                }
            },
            onLeave: function(retval) {
                if (this.block) {
                    retval.replace(0);
                }
            }
        });
    }

    // Bypass fopen for root files
    let fopen = Module.findExportByName(null, "fopen");
    if (fopen) {
        Interceptor.attach(fopen, {
            onEnter: function(args) {
                let path = args[0].isNull() ? null : Memory.readCString(args[0]);
                if (path && (path.indexOf("/su") !== -1 ||
                    path.indexOf("magisk") !== -1 ||
                    path.indexOf("kernelsu") !== -1)) {
                    console.log("[BYPASS] Blocking fopen: " + path);
                    this.block = true;
                }
            },
            onLeave: function(retval) {
                if (this.block) {
                    retval.replace(0);
                }
            }
        });
    }
}

// ==================== JNI HOOKING ====================

function hookJNI() {
    console.log("[*] Hooking JNI...");

    let module = Process.findModuleByName("libart.so") ||
                 Process.findModuleByName("libdvm.so");

    if (!module) {
        console.log("[!] ART/DVM not found");
        return;
    }

    // Hook RegisterNatives
    let RegisterNatives = Module.findExportByName(module.name,
        "_ZN3art3JNI15RegisterNativeMethodsEP7_JNIEnvP7_jclassPK15JNINativeMethodi");

    if (RegisterNatives) {
        Interceptor.attach(RegisterNatives, {
            onEnter: function(args) {
                let env = args[0];
                let cls = args[1];
                let methods = args[2];
                let count = args[3].toInt32();

                console.log("[JNI] RegisterNatives called with " + count + " methods");

                for (let i = 0; i < count; i++) {
                    let method = methods.add(i * Process.pointerSize * 3);
                    let name = Memory.readPointer(method);
                    let sig = Memory.readPointer(method.add(Process.pointerSize));
                    let fn = Memory.readPointer(method.add(Process.pointerSize * 2));

                    try {
                        console.log("  [JNI] " + Memory.readCString(name) +
                                   Memory.readCString(sig) + " -> " + fn);
                    } catch(e) {
                        console.log("  [JNI] Error reading method info");
                    }
                }
            }
        });
    }
}

// ==================== SSL PINNING BYPASS ====================

function bypassSSLPinning() {
    console.log("[*] Installing SSL pinning bypass...");

    // Hook SSL_CTX_set_custom_verify (BoringSSL)
    let SSL_CTX_set_custom_verify = Module.findExportByName(null,
        "SSL_CTX_set_custom_verify");

    if (SSL_CTX_set_custom_verify) {
        Interceptor.replace(SSL_CTX_set_custom_verify, new NativeCallback(function(ctx,
            mode, callbacks) {
            console.log("[SSL] Bypassing custom verify");
            return 0;
        }, 'void', ['pointer', 'int', 'pointer']));
    }

    // Hook SSL_CTX_set_verify
    let SSL_CTX_set_verify = Module.findExportByName(null, "SSL_CTX_set_verify");
    if (SSL_CTX_set_verify) {
        Interceptor.replace(SSL_CTX_set_verify, new NativeCallback(function(ctx,
            mode, callback) {
            console.log("[SSL] Bypassing SSL_CTX_set_verify");
        }, 'void', ['pointer', 'int', 'pointer']));
    }

    // Hook cert_verify_callback
    let cert_verify_callback = Module.findExportByName(null,
        "cert_verify_callback");

    if (cert_verify_callback) {
        Interceptor.replace(cert_verify_callback, new NativeCallback(function(
            transport, callback) {
            console.log("[SSL] Bypassing cert verify callback");
            return 1; // Success
        }, 'int', ['pointer', 'pointer']));
    }

    // Hook X509_verify_cert
    let X509_verify_cert = Module.findExportByName(null, "X509_verify_cert");
    if (X509_verify_cert) {
        Interceptor.replace(X509_verify_cert, new NativeCallback(function(ctx) {
            console.log("[SSL] Bypassing X509_verify_cert");
            return 1; // Success
        }, 'int', ['pointer']));
    }
}

// ==================== MEMORY SCANNING ====================

function scanMemory(pattern, moduleName) {
    console.log("[*] Scanning memory for: " + pattern);

    let ranges = Process.enumerateRanges('r--');
    let matches = 0;

    ranges.forEach(function(range) {
        if (moduleName && !range.file ||
            (range.file && range.file.path.indexOf(moduleName) === -1)) {
            return;
        }

        try {
            Memory.scan(range.base, range.size, pattern, {
                onMatch: function(addr, size) {
                    console.log("  [MATCH] " + addr + " in " +
                        (range.file ? range.file.path : "anonymous"));
                    matches++;
                },
                onComplete: function() {}
            });
        } catch(e) {
            // Skip unreadable ranges
        }
    });

    console.log("[*] Found " + matches + " matches");
}

// ==================== MODULE ENUMERATION ====================

function listModules() {
    console.log("[*] Loaded modules:");
    Process.enumerateModules().forEach(function(mod) {
        console.log("  " + mod.name + " @ " + mod.base + " (" + mod.size + " bytes)");
    });
}

function listExports(moduleName) {
    let mod = Process.findModuleByName(moduleName);
    if (!mod) {
        console.log("[!] Module not found: " + moduleName);
        return;
    }

    console.log("[*] Exports from " + moduleName + ":");
    mod.enumerateExports().forEach(function(exp) {
        console.log("  " + exp.name + " @ " + exp.address);
    });
}

// ==================== INTERACTIVE COMMANDS ====================

rpc.exports = {
    // Get library info
    getInfo: function() {
        return {
            version: "5.0.0-RE",
            pid: Process.id,
            arch: Process.arch,
            platform: Process.platform,
            pointerSize: Process.pointerSize,
            pageSize: Process.pageSize,
            modules: Process.enumerateModules().length
        };
    },

    // List modules
    listModules: function() {
        return Process.enumerateModules().map(function(m) {
            return {
                name: m.name,
                base: m.base.toString(),
                size: m.size,
                path: m.path
            };
        });
    },

    // Find symbol
    findSymbol: function(name) {
        let addr = Module.findExportByName(null, name);
        return addr ? addr.toString() : null;
    },

    // Read memory
    readMemory: function(addr, size) {
        return Memory.readByteArray(ptr(addr), size);
    },

    // Write memory
    writeMemory: function(addr, data) {
        Memory.writeByteArray(ptr(addr), data);
        return true;
    },

    // Scan memory
    scanMemory: function(pattern) {
        let results = [];
        Process.enumerateRanges('r--').forEach(function(range) {
            try {
                Memory.scanSync(range.base, range.size, pattern).forEach(function(match) {
                    results.push({
                        address: match.address.toString(),
                        size: match.size
                    });
                });
            } catch(e) {
                // Skip unreadable ranges
            }
        });
        return results;
    },

    // Hook function
    hookFunction: function(name, module) {
        hookFunction(name, module, {
            onEnter: function(args) {
                console.log("[*] " + name + " called");
            }
        });
        return true;
    },

    // Dump module
    dumpModule: function(name) {
        let mod = Process.findModuleByName(name);
        if (!mod) return null;

        return {
            name: mod.name,
            base: mod.base.toString(),
            size: mod.size,
            path: mod.path,
            exports: mod.enumerateExports().slice(0, 100).map(function(e) {
                return { name: e.name, address: e.address.toString() };
            })
        };
    },

    // Get backtrace
    getBacktrace: function() {
        return Thread.backtrace(this.context, Backtracer.ACCURATE).map(function(addr) {
            return {
                address: addr.toString(),
                symbol: DebugSymbol.fromAddress(addr).toString()
            };
        });
    }
};

// ==================== KERNELSU / APATCH BYPASS (2025+) ====================

function bypassKernelSU() {
    console.log("[*] Installing KernelSU bypass...");

    // Block access to KernelSU paths
    let access = Module.findExportByName(null, "access");
    if (access) {
        // Already hooked in bypassSecurityChecks, add KernelSU-specific patterns
    }

    // Hook ksud binary detection
    let open = Module.findExportByName(null, "open");
    if (open) {
        Interceptor.attach(open, {
            onEnter: function(args) {
                let path = args[0].isNull() ? null : Memory.readCString(args[0]);
                if (path && (path.indexOf("ksud") !== -1 ||
                    path.indexOf("kernelsu") !== -1 ||
                    path.indexOf("/data/adb/ksu") !== -1)) {
                    console.log("[BYPASS-KSU] Blocking open: " + path);
                    this.block = true;
                }
            },
            onLeave: function(retval) {
                if (this.block) {
                    retval.replace(-1);
                }
            }
        });
    }
}

function bypassAPatch() {
    console.log("[*] Installing APatch bypass...");

    // Block access to APatch paths
    let open = Module.findExportByName(null, "open");
    if (open) {
        Interceptor.attach(open, {
            onEnter: function(args) {
                let path = args[0].isNull() ? null : Memory.readCString(args[0]);
                if (path && (path.indexOf("apatch") !== -1 ||
                    path.indexOf("/data/adb/apatch") !== -1)) {
                    console.log("[BYPASS-AP] Blocking open: " + path);
                    this.block = true;
                }
            },
            onLeave: function(retval) {
                if (this.block) {
                    retval.replace(-1);
                }
            }
        });
    }
}

// ==================== MAIN ====================

function main() {
    console.log("[*] UniversalStub Frida Companion v5.0.0-RE");
    console.log("[*] PID: " + Process.id);
    console.log("[*] Arch: " + Process.arch);
    console.log("[*] Platform: " + Process.platform);

    // Install hooks
    hookDynamicLoader();
    bypassSecurityChecks();
    hookJNI();
    bypassSSLPinning();

    // 2025+: Additional bypasses for newer detection methods
    bypassKernelSU();
    bypassAPatch();

    console.log("[*] All hooks installed");
    console.log("[*] Use rpc.exports.* for interactive control");
}

main();
