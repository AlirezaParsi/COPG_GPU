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

static const char* TARGET_PACKAGE = "flar2.devcheck";

// ========== Companion Functions ==========
void createSpoofFile() {
    LOGI("Creating GPU spoof file...");
    
    FILE* fp = fopen("/data/local/tmp/gpu_830_spoof", "w");
    if (fp) {
        fprintf(fp, "Adreno 830\n");
        fprintf(fp, "Vendor: Qualcomm\n");
        fprintf(fp, "Device ID: 0x430514C1\n");
        fprintf(fp, "Revision: r5p0\n");
        fprintf(fp, "Clock: 900 MHz\n");
        fclose(fp);
        chmod("/data/local/tmp/gpu_830_spoof", 0444);
        LOGI("Spoof file created");
    } else {
        LOGE("Failed to create spoof file");
    }
}

void mountGPUSpoof() {
    LOGI("Mounting GPU spoof...");
    
    // لیست مسیرهای GPU
    const char* gpu_paths[] = {
        "/sys/class/kgsl/kgsl-3d0/gpu_model",
        "/sys/class/drm/card0/device/gpu_info",
        "/sys/kernel/gpu/gpu_model",
        "/proc/gpuinfo",
        nullptr
    };
    
    int mounted = 0;
    for (int i = 0; gpu_paths[i] != nullptr; i++) {
        // بررسی وجود مسیر
        if (access(gpu_paths[i], F_OK) == 0) {
            LOGI("Found GPU path: %s", gpu_paths[i]);
            
            // ابتدا unmount کن (اگر mount شده)
            umount2(gpu_paths[i], MNT_DETACH);
            
            // mount فایل جعلی
            if (mount("/data/local/tmp/gpu_830_spoof", gpu_paths[i], 
                     nullptr, MS_BIND | MS_RDONLY, nullptr) == 0) {
                LOGI("Successfully mounted to: %s", gpu_paths[i]);
                mounted++;
            } else {
                LOGE("Failed to mount: %s", gpu_paths[i]);
            }
        }
    }
    
    LOGI("Mounted to %d GPU paths", mounted);
}

void spoofBuildProps() {
    LOGI("Spoofing build properties...");
    
    // یافتن resetprop
    const char* resetprop = nullptr;
    const char* paths[] = {
        "/data/adb/magisk/resetprop",
        "/data/adb/ksu/bin/resetprop",
        "/system/bin/resetprop",
        nullptr
    };
    
    for (int i = 0; paths[i] != nullptr; i++) {
        if (access(paths[i], X_OK) == 0) {
            resetprop = paths[i];
            break;
        }
    }
    
    if (resetprop) {
        char cmd[256];
        
        // GPU properties
        snprintf(cmd, sizeof(cmd), "%s ro.hardware.egl adreno", resetprop);
        system(cmd);
        
        snprintf(cmd, sizeof(cmd), "%s ro.board.platform lahaina", resetprop);
        system(cmd);
        
        snprintf(cmd, sizeof(cmd), "%s ro.chipname lahaina", resetprop);
        system(cmd);
        
        snprintf(cmd, sizeof(cmd), "%s ro.vendor.gpu.model \"Adreno 830\"", resetprop);
        system(cmd);
        
        snprintf(cmd, sizeof(cmd), "%s vendor.gpu.model \"Adreno 830\"", resetprop);
        system(cmd);
        
        snprintf(cmd, sizeof(cmd), "%s ro.opengles.version 196608", resetprop);
        system(cmd);
        
        LOGI("Build properties spoofed");
    } else {
        LOGE("resetprop not found");
    }
}

void cleanupSpoof() {
    LOGI("Cleaning up...");
    unlink("/data/local/tmp/gpu_830_spoof");
}

// ========== Companion Entry Point ==========
void companion(int fd) {
    LOGI("=== GPU Spoof Companion Started ===");
    
    char buffer[64];
    ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
    
    if (bytes > 0) {
        buffer[bytes] = '\0';
        LOGI("Received command: %s", buffer);
        
        int result = -1;
        
        if (strcmp(buffer, "setup") == 0) {
            // ایجاد و mount فایل‌ها
            createSpoofFile();
            mountGPUSpoof();
            spoofBuildProps();
            result = 0;
        } else if (strcmp(buffer, "cleanup") == 0) {
            cleanupSpoof();
            result = 0;
        } else if (strcmp(buffer, "test") == 0) {
            LOGI("Test command received");
            result = 0;
        }
        
        // ارسال نتیجه
        write(fd, &result, sizeof(result));
        LOGI("Command result: %d", result);
    } else {
        LOGE("No command received");
    }
    
    close(fd);
    LOGI("=== Companion Finished ===");
}

// ========== Zygisk Module ==========
class GPU830Module : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
        LOGI("GPU 830 Spoof Module loaded");
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        if (!args || !args->nice_name) {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        
        const char* package_name = env->GetStringUTFChars(args->nice_name, nullptr);
        bool should_spoof = false;
        
        if (package_name) {
            LOGI("Checking package: %s", package_name);
            
            // فقط برای DevCheck
            if (strstr(package_name, "devcheck") != nullptr || 
                strcmp(package_name, TARGET_PACKAGE) == 0) {
                should_spoof = true;
                LOGI("Target package identified!");
            }
        }
        
        if (should_spoof) {
            LOGI("Activating GPU 830 spoof...");
            
            // اتصال به companion
            int fd = api->connectCompanion();
            if (fd >= 0) {
                LOGI("Connected to companion");
                
                // ارسال دستور setup
                const char* cmd = "setup";
                write(fd, cmd, strlen(cmd) + 1);
                
                // دریافت نتیجه
                int result = -1;
                read(fd, &result, sizeof(result));
                close(fd);
                
                if (result == 0) {
                    LOGI("✅ GPU spoof setup successful!");
                    api->setOption(zygisk::Option::FORCE_DENYLIST_UNMOUNT);
                } else {
                    LOGE("❌ GPU spoof setup failed");
                    api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
                }
            } else {
                LOGE("❌ Failed to connect to companion");
                api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            }
        } else {
            LOGI("Not target package, unloading module");
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
        
        if (package_name) {
            env->ReleaseStringUTFChars(args->nice_name, package_name);
        }
    }
    
    void postAppSpecialize(const zygisk::AppSpecializeArgs* args) override {
        LOGI("postAppSpecialize - keeping module loaded");
        // ماژول را در حافظه نگه می‌داریم
    }

private:
    zygisk::Api* api;
    JNIEnv* env;
};

// ========== Registration ==========
// ثبت همزمان ماژول و companion
REGISTER_ZYGISK_MODULE(GPU830Module)
REGISTER_ZYGISK_COMPANION(companion)
