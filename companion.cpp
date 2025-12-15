#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <android/log.h>

#define LOG_TAG "GPU830Companion"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

void setupGPUSpoof() {
    LOGI("Setting up GPU 830 spoof...");
    
    // ایجاد فایل جعلی
    const char* spoof_content = "Adreno 830\nVendor: Qualcomm\nDevice ID: 0x430514C1\n";
    
    FILE* fp = fopen("/data/local/tmp/gpu_spoof.tmp", "w");
    if (fp) {
        fputs(spoof_content, fp);
        fclose(fp);
        chmod("/data/local/tmp/gpu_spoof.tmp", 0444);
        LOGI("Spoof file created");
    } else {
        LOGE("Failed to create spoof file");
        return;
    }
    
    // مسیرهای GPU
    const char* gpu_paths[] = {
        "/sys/class/kgsl/kgsl-3d0/gpu_model",
        "/sys/class/drm/card0/device/gpu_info",
        "/sys/kernel/gpu/gpu_model",
        "/proc/gpuinfo",
        nullptr
    };
    
    // Mount کردن
    int mounted = 0;
    for (int i = 0; gpu_paths[i] != nullptr; i++) {
        if (access(gpu_paths[i], F_OK) == 0) {
            // Unmount اولیه
            umount2(gpu_paths[i], MNT_DETACH);
            
            // Mount فایل جعلی
            if (mount("/data/local/tmp/gpu_spoof.tmp", gpu_paths[i], 
                     nullptr, MS_BIND | MS_RDONLY, nullptr) == 0) {
                LOGI("Mounted to: %s", gpu_paths[i]);
                mounted++;
            } else {
                LOGE("Failed to mount: %s", gpu_paths[i]);
            }
        }
    }
    
    // تغییر build properties
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
        snprintf(cmd, sizeof(cmd), "%s ro.hardware.egl adreno", resetprop);
        system(cmd);
        
        snprintf(cmd, sizeof(cmd), "%s ro.vendor.gpu.model \"Adreno 830\"", resetprop);
        system(cmd);
        
        LOGI("Build properties updated");
    }
    
    LOGI("GPU spoof setup complete. Mounted to %d locations", mounted);
}

void companion(int fd) {
    LOGI("GPU Spoof Companion started");
    
    char buffer[256];
    ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
    
    if (bytes > 0) {
        buffer[bytes] = '\0';
        
        int result = -1;
        
        if (strcmp(buffer, "setup_gpu_spoof") == 0) {
            setupGPUSpoof();
            result = 0;
        } else if (strcmp(buffer, "cleanup") == 0) {
            // Clean up
            unlink("/data/local/tmp/gpu_spoof.tmp");
            result = 0;
        }
        
        write(fd, &result, sizeof(result));
    }
    
    close(fd);
}
