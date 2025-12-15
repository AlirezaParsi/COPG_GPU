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

// فقط DevCheck
static const char* TARGET_PACKAGES[] = {
    "flar2.devcheck",
    "com.flar2.devcheck",
    nullptr
};

bool isTargetPackage(const char* package_name) {
    if (!package_name) return false;
    
    for (int i = 0; TARGET_PACKAGES[i] != nullptr; i++) {
        if (strcmp(package_name, TARGET_PACKAGES[i]) == 0) {
            return true;
        }
    }
    return false;
}

// ========== Companion ==========
void setupGPUSpoof() {
    LOGI("Setting up GPU 830 spoof...");
    
    // ایجاد فایل جعلی
    FILE* fp = fopen("/data/local/tmp/gpu_830_spoof", "w");
    if (fp) {
        fprintf(fp, "Adreno 830\n");
        fclose(fp);
        chmod("/data/local/tmp/gpu_830_spoof", 0444);
        LOGI("Spoof file created");
    }
    
    // فقط این مسیر (کمترین تداخل)
    const char* target_path = "/sys/class/kgsl/kgsl-3d0/gpu_model";
    
    if (access(target_path, F_OK) == 0) {
        umount2(target_path, MNT_DETACH);
        if (mount("/data/local/tmp/gpu_830_spoof", target_path, nullptr, MS_BIND | MS_RDONLY, nullptr) == 0) {
            LOGI("Mounted to %s", target_path);
        } else {
            LOGE("Failed to mount");
        }
    }
    
    LOGI("GPU spoof ready");
}

void cleanupGPUSpoof() {
    unlink("/data/local/tmp/gpu_830_spoof");
    umount2("/sys/class/kgsl/kgsl-3d0/gpu_model", MNT_DETACH);
}

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
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        if (!args || !args->nice_name) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        
        const char* package_name = env->GetStringUTFChars(args->nice_name, nullptr);
        bool is_target = false;
        
        if (package_name) {
            LOGD("App: %s", package_name);
            is_target = isTargetPackage(package_name);
            env->ReleaseStringUTFChars(args->nice_name, package_name);
        }
        
        if (is_target) {
            LOGI("🎯 Activating GPU spoof for DevCheck");
            
            int fd = api->connectCompanion();
            if (fd >= 0) {
                write(fd, "setup", 6);
                
                int result = -1;
                read(fd, &result, sizeof(result));
                close(fd);
                
                if (result == 0) {
                    LOGI("✅ GPU spoof activated");
                    // ماژول را نگه دار
                    return;
                }
            }
            
            LOGE("Failed to activate GPU spoof");
        }
        
        // برای سایر برنامه‌ها ماژول را ببند
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }
    
    void postAppSpecialize(const zygisk::AppSpecializeArgs* args) override {
        // کاری نکن
    }

private:
    zygisk::Api* api;
    JNIEnv* env;
};

REGISTER_ZYGISK_MODULE(GPU830Module)
REGISTER_ZYGISK_COMPANION(companion)
