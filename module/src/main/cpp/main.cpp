//
// Created by canyie on 2020/8/12.
//

#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <dlfcn.h>
#include <stdio.h>
#include <android/log.h>
#include <fcntl.h>
#include "main.h"
#include "log.h"

NativeBridgeCallbacks NativeBridgeItf;
void* real_nb_handle_ = nullptr;

static bool fake_initialize(const struct NativeBridgeRuntimeCallbacks* runtime_cbs,
        const char* private_dir, const char* instruction_set) {
    return false;
}

static void* fake_loadLibrary(const char* libpath, int flag) {
    return nullptr;
}

static void* fake_getTrampoline(void* handle, const char* name, const char* shorty, uint32_t len) {
    return nullptr;
}

static bool fake_isSupported(const char* libpath) {
    return false;
}

static const struct NativeBridgeRuntimeValues* fake_getAppEnv(const char* instruction_set) {
    return nullptr;
}

static bool fake_isCompatibleWith(uint32_t bridge_version) {
    return bridge_version <= NativeBridgeImplementationVersion::NAMESPACE_VERSION;
}

static NativeBridgeSignalHandlerFn fake_getSignalHandler(int signal) {
    return nullptr;
}

static int fake_unloadLibrary(void* handle) {
    return 1;
}

static const char* fake_getError() {
    return "Not supported for NbInjection without real native bridge";
}

static bool fake_isPathSupported(const char* library_path) {
    return false;
}

static bool fake_initAnonymousNamespace(const char* public_ns_sonames, const char* anon_ns_library_path) {
    return false;
}

static struct native_bridge_namespace_t* fake_createNamespace(const char* name,
        const char* ld_library_path, const char* default_library_path, uint64_t type,
        const char* permitted_when_isolated_path, struct native_bridge_namespace_t* parent_ns) {
    return nullptr;
}

static bool fake_linkNamespaces(struct native_bridge_namespace_t* from,
        struct native_bridge_namespace_t* to, const char* shared_libs_sonames) {
    return false;
}

static void* fake_loadLibraryExt(const char* libpath, int flag, struct native_bridge_namespace_t* ns) {
    return nullptr;
}

static struct native_bridge_namespace_t* fake_getVendorNamespace() {
    return nullptr;
}

static struct native_bridge_namespace_t* fake_getExportedNamespace(const char* name) {
    return nullptr;
}

static void fake_preZygoteFork() {
}

static bool read_real_nb_filename(char* buf, uint32_t len) {
    FILE* fp = fopen("/data/misc/nbinjection/real_nb_filename.txt", "re");
    if (!fp) {
        LOGE("Could not open real_nb_filename.txt: %s(%d)", strerror(errno), errno);
        return false;
    }
    if (!fgets(buf, len, fp)) {
        if (feof(fp)) {
            LOGW("Unexpected EOF when reading real_nb_filename.txt. empty file?");
        } else {
            LOGE("Could not read real_nb_filename.txt: %s(%d)", strerror(errno), errno);
        }
        fclose(fp);
        return false;
    }
    fclose(fp);
    return true;
}

void setup_nb_itf() {
    char real_nb_filename[PROP_VALUE_MAX + 1];
    if (read_real_nb_filename(real_nb_filename, PROP_VALUE_MAX)) {
        if (real_nb_filename[0] == '\0') {
            LOGW("ro.dalvik.vm.native.bridge is not expected to be empty");
        } else if (strcmp(real_nb_filename, "0") != 0) {
            LOGI("The system has real native bridge support, libname %s", real_nb_filename);
            real_nb_handle_ = dlopen(real_nb_filename, RTLD_LAZY);
            const char* error;
            if (real_nb_handle_) {
                void* real_nb_itf = dlsym(real_nb_handle_, "NativeBridgeItf");
                if (real_nb_itf) {
                    // sizeof(NativeBridgeCallbacks) maybe changed in other android version
                    memcpy(&NativeBridgeItf, real_nb_itf, sizeof(NativeBridgeCallbacks));
                    return;
                }
                error = dlerror();
                dlclose(real_nb_handle_);
                real_nb_handle_ = nullptr;
            } else {
                error = dlerror();
            }
            LOGE("Could not setup NativeBridgeItf for real lib %s: %s", real_nb_filename, error);
        }
    }

    // The system has no native bridge support, replace callbacks to fake implementations
    // If you just use nbinjection as a starter(just load a other library), then its work is now complete;
    // you can set version to an invalid value to make the system close us.
    // Even if we don't do this, we can only reside in the memory of the zygote process;
    // for processes that do not require native bridge, we will be uninstalled in Runtime::InitNonZygoteOrPostFork.
    LOGI("Setup NativeBridgeItf to the fake implementations");
    NativeBridgeItf.version = NativeBridgeImplementationVersion::NAMESPACE_VERSION;
    NativeBridgeItf.initialize = fake_initialize;
    NativeBridgeItf.loadLibrary = fake_loadLibrary;
    NativeBridgeItf.getTrampoline = fake_getTrampoline;
    NativeBridgeItf.isSupported = fake_isSupported;
    NativeBridgeItf.getAppEnv = fake_getAppEnv;
    NativeBridgeItf.isCompatibleWith = fake_isCompatibleWith;
    NativeBridgeItf.getSignalHandler = fake_getSignalHandler;
    NativeBridgeItf.unloadLibrary = fake_unloadLibrary;
    NativeBridgeItf.getError = fake_getError;
    NativeBridgeItf.isPathSupported = fake_isPathSupported;
    NativeBridgeItf.initAnonymousNamespace = fake_initAnonymousNamespace;
    NativeBridgeItf.createNamespace = fake_createNamespace;
    NativeBridgeItf.linkNamespaces = fake_linkNamespaces;
    NativeBridgeItf.loadLibraryExt = fake_loadLibraryExt;
    NativeBridgeItf.getVendorNamespace = fake_getVendorNamespace;
    NativeBridgeItf.getExportedNamespace = fake_getExportedNamespace;
    NativeBridgeItf.preZygoteFork = fake_preZygoteFork;
}

bool is_zygote() {
    if (getuid() != 0) return false; // Zygote always running in root user

    // Read current process name and check it is whether zygote64(for 64-bit) or zygote (32-bit)
    // We can't read cmdline to get start arguments here,
    // because it has been overrode before we were loaded. See app_main.cpp
    int fd = open("/proc/self/cmdline", O_RDONLY | O_CLOEXEC);
    if (fd == -1) {
        LOGE("Failed to open /proc/self/cmdline: %s(%d)", strerror(errno), errno);
        return false;
    }

    char buf[NAME_MAX + 1];
    ssize_t size = read(fd, buf, NAME_MAX);
    if (size <= 0) {
        LOGE("Failed to read /proc/self/cmdline: %s(%d)", strerror(errno), errno);
        close(fd);
        return false;
    }
    close(fd);

    return strcmp(buf, kZygoteNiceName) == 0;
}

__attribute__((constructor)) __attribute__((used)) void OnLoad() {
    // Do what you want here
    LOGI("NbInjection loaded in %s process, pid=%d uid=%d", is_zygote() ? "zygote" : "non-zygote", getpid(), getuid());
    setup_nb_itf();
}

__attribute__((destructor)) __attribute__((used)) void OnUnload() {
    if (real_nb_handle_) {
        dlclose(real_nb_handle_);
    }
}