#include <jni.h>
#include <string>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <android/log.h>
#include "zygisk.hpp"

#define LOG_TAG "GPUSmartSpoof"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static const char* TARGET_PACKAGE = "flar2.devcheck";

// ============================================
// سیستم fake کردن فایل‌خوانی
// ============================================

typedef ssize_t (*read_func_t)(int, void*, size_t);
typedef FILE* (*fopen_func_t)(const char*, const char*);
typedef char* (*fgets_func_t)(char*, int, FILE*);

static read_func_t original_read = nullptr;
static fopen_func_t original_fopen = nullptr;
static fgets_func_t original_fgets = nullptr;

// hook برای read
ssize_t hooked_read(int fd, void* buf, size_t count) {
    // بررسی آیا در حال خواندن فایل GPU هستیم
    char path[256];
    snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
    
    char real_path[256];
    ssize_t len = readlink(path, real_path, sizeof(real_path)-1);
    if (len > 0) {
        real_path[len] = '\0';
        
        // اگر فایل GPU است، fake data برگردان
        if (strstr(real_path, "gpu_model") || strstr(real_path, "gpuinfo") || 
            strstr(real_path, "gpu_info")) {
            
            LOGI("Intercepting read for: %s", real_path);
            
            const char* fake_gpu = "Adreno 830\n";
            size_t fake_len = strlen(fake_gpu);
            
            if (count >= fake_len) {
                memcpy(buf, fake_gpu, fake_len);
                return fake_len;
            }
        }
    }
    
    // در غیر این صورت read اصلی
    return original_read(fd, buf, count);
}

// hook برای fopen
FILE* hooked_fopen(const char* pathname, const char* mode) {
    if (pathname && (strstr(pathname, "gpu_model") || strstr(pathname, "gpuinfo"))) {
        LOGI("Intercepting fopen for: %s", pathname);
        
        // ایجاد فایل موقت با اطلاعات جعلی
        static FILE* fake_file = nullptr;
        if (!fake_file) {
            fake_file = tmpfile();
            if (fake_file) {
                fputs("Adreno 830\n", fake_file);
                fputs("Vendor: Qualcomm\n", fake_file);
                fputs("Device ID: 0x430514C1\n", fake_file);
                rewind(fake_file);
            }
        }
        return fake_file;
    }
    
    return original_fopen(pathname, mode);
}

// hook برای fgets
char* hooked_fgets(char* str, int num, FILE* stream) {
    // اگر فایل GPU fake ماست
    if (stream && ftell(stream) == 0) {
        static bool first_call = true;
        if (first_call) {
            first_call = false;
            const char* fake_line = "Adreno 830\n";
            strncpy(str, fake_line, num);
            return str;
        }
    }
    
    return original_fgets(str, num, stream);
}

// ============================================
// Zygisk Module
// ============================================

class SmartGPUSpoof : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
        LOGI("Smart GPU Spoof loaded");
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        if (!args || !args->nice_name) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        
        const char* package_name = env->GetStringUTFChars(args->nice_name, nullptr);
        bool is_target = false;
        
        if (package_name) {
            is_target = (strcmp(package_name, TARGET_PACKAGE) == 0);
            env->ReleaseStringUTFChars(args->nice_name, package_name);
        }
        
        if (is_target) {
            LOGI("🎯 Setting up smart GPU spoof for DevCheck");
            setupHooks();
            api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);
        } else {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
        }
    }
    
    void postAppSpecialize(const zygisk::AppSpecializeArgs* args) override {
        LOGI("Smart GPU spoof active");
    }

private:
    zygisk::Api* api;
    JNIEnv* env;
    
    void setupHooks() {
        LOGI("Setting up function hooks...");
        
        // ذخیره توابع اصلی
        original_read = (read_func_t)dlsym(RTLD_NEXT, "read");
        original_fopen = (fopen_func_t)dlsym(RTLD_NEXT, "fopen");
        original_fgets = (fgets_func_t)dlsym(RTLD_NEXT, "fgets");
        
        // در اینجا باید inline hooking انجام شود
        // برای سادگی فعلاً فقط log می‌کنیم
        LOGI("Hooks would be installed here");
    }
};

REGISTER_ZYGISK_MODULE(SmartGPUSpoof)
