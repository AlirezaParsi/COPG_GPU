#include <jni.h>
#include <string>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <android/log.h>
#include "zygisk.hpp"

#define LOG_TAG "GPU830Spoof"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// فقط DevCheck - دقت کن!
static const char* TARGET_PACKAGES[] = {
    "flar2.devcheck",
    "com.flar2.devcheck",
    nullptr  // پایان لیست
};

// ========== Companion Functions ==========
bool isTargetPackage(const char* package_name) {
    if (!package_name) return false;
    
    for (int i = 0; TARGET_PACKAGES[i] != nullptr; i++) {
        if (strcmp(package_name, TARGET_PACKAGES[i]) == 0) {
            return true;
        }
    }
    return false;
}

void setupGPUSpoof() {
    LOGI("Setting up GPU spoof...");
    
    // فقط فایل‌های sys را mount کنیم (نه libraryها)
    const char* sys_paths[] = {
        "/sys/class/kgsl/kgsl-3d0/gpu_model",
        "/sys/class/drm/card0/device/gpu_info", 
        nullptr
    };
    
    // ایجاد فایل جعلی
    FILE* fp = fopen("/data/local/tmp/gpu_spoof_830", "w");
    if (fp) {
        fprintf(fp, "Adreno 830\n");
        fclose(fp);
        chmod("/data/local/tmp/gpu_spoof_830", 0444);
    }
    
    // mount فقط فایل‌های sys
    for (int i = 0; sys_paths[i] != nullptr; i++) {
        if (access(sys_paths[i], F_OK) == 0) {
            umount2(sys_paths[i], MNT_DETACH);
            mount("/data/local/tmp/gpu_spoof_830", sys_paths[i], nullptr, MS_BIND | MS_RDONLY, nullptr);
            LOGD("Mounted: %s", sys_paths[i]);
        }
    }
    
    LOGI("GPU spoof setup done");
}

void cleanupGPUSpoof() {
    LOGD("Cleaning up GPU spoof");
    unlink("/data/local/tmp/gpu_spoof_830");
    
    // unmount مسیرها
    const char* paths[] = {
        "/sys/class/kgsl/kgsl-3d0/gpu_model",
        "/sys/class/drm/card0/device/gpu_info",
        nullptr
    };
    
    for (int i = 0; paths[i] != nullptr; i++) {
        umount2(paths[i], MNT_DETACH);
    }
}

// ========== Companion ==========
void companion(int fd) {
    LOGD("Companion started");
    
    char cmd[32];
    int bytes = read(fd, cmd, sizeof(cmd) - 1);
    
    if (bytes > 0) {
        cmd[bytes] = '\0';
        
        int result = -1;
        
        if (strcmp(cmd, "setup") == 0) {
            setupGPUSpoof();
            result = 0;
        } else if (strcmp(cmd, "cleanup") == 0) {
            cleanupGPUSpoof();
            result = 0;
        } else {
            LOGE("Unknown command: %s", cmd);
        }
        
        write(fd, &result, sizeof(result));
    }
    
    close(fd);
}

// ========== Zygisk Module ==========
class GPU830Module : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
        LOGD("Module loaded");
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        if (!args || !args->nice_name) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        
        const char* package_name = env->GetStringUTFChars(args->nice_name, nullptr);
        bool target_found = false;
        
        if (package_name) {
            LOGD("Package: %s", package_name);
            target_found = isTargetPackage(package_name);
            env->ReleaseStringUTFChars(args->nice_name, package_name);
        }
        
        if (target_found) {
            LOGI("🎯 Target found! Setting up GPU 830 spoof");
            
            // فعال کردن companion
            int fd = api->connectCompanion();
            if (fd >= 0) {
                write(fd, "setup", 6);
                
                int result = -1;
                read(fd, &result, sizeof(result));
                close(fd);
                
                if (result == 0) {
                    LOGI("✅ GPU spoof activated for DevCheck");
                    // ماژول را نگه دار
                    return;
                }
            }
            
            LOGE("❌ Failed to setup GPU spoof");
        }
        
        // برای سایر برنامه‌ها، ماژول را unload کن
        LOGD("Not target app, unloading module");
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs* args) override {
        // کاری نکن
    }
    
    void onUnload() override {
        LOGD("Module unloading");
    }

private:
    zygisk::Api* api;
    JNIEnv* env;
};

// ========== Registration ==========
REGISTER_ZYGISK_MODULE(GPU830Module)
REGISTER_ZYGISK_COMPANION(companion)
