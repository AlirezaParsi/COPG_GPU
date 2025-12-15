
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <android/log.h>

#define LOG_TAG "GPU830Companion"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// اطلاعات Adreno 830
static const char* ADRENO_830_INFO = "Adreno 830";

// مسیرهای GPU
static const char* GPU_PATHS[] = {
    "/sys/class/kgsl/kgsl-3d0/gpu_model",
    "/sys/class/drm/card0/device/gpu_info",
    "/sys/kernel/gpu/gpu_model",
    "/proc/gpuinfo",
    "/sys/class/misc/mali0/device/gpu_name",
    nullptr
};

// مسیرهای EGL
static const char* EGL_PATHS[] = {
    "/system/lib/egl/egl.cfg",
    "/system/lib64/egl/egl.cfg",
    "/vendor/lib/egl/egl.cfg",
    "/vendor/lib64/egl/egl.cfg",
    nullptr
};

void createSpoofFiles() {
    LOGI("Creating GPU spoof files...");
    
    // ایجاد دایرکتوری موقت
    mkdir("/data/local/tmp/gpu_spoof", 0755);
    
    // ایجاد فایل‌های جعلی
    for (int i = 0; GPU_PATHS[i] != nullptr; i++) {
        char spoof_path[256];
        snprintf(spoof_path, sizeof(spoof_path), "/data/local/tmp/gpu_spoof/file%d", i);
        
        FILE* fp = fopen(spoof_path, "w");
        if (fp) {
            if (strstr(GPU_PATHS[i], "gpu_model") || strstr(GPU_PATHS[i], "gpu_name") || 
                strstr(GPU_PATHS[i], "gpuinfo")) {
                fprintf(fp, "%s\n", ADRENO_830_INFO);
            } else if (strstr(GPU_PATHS[i], "gpu_busy")) {
                fprintf(fp, "45\n");
            } else if (strstr(GPU_PATHS[i], "max_gpuclk")) {
                fprintf(fp, "900000000\n");
            } else {
                fprintf(fp, "Qualcomm Adreno 830\nVendor: Qualcomm\nDevice ID: 0x430514C1\n");
            }
            fclose(fp);
            chmod(spoof_path, 0444);
        }
    }
    
    // ایجاد egl.cfg جعلی
    FILE* fp = fopen("/data/local/tmp/gpu_spoof/egl.cfg", "w");
    if (fp) {
        fprintf(fp, "0 1 adreno\n");
        fclose(fp);
    }
    
    LOGI("Spoof files created");
}

void mountSpoofFiles() {
    LOGI("Mounting spoof files...");
    
    // Unmount اولیه
    for (int i = 0; GPU_PATHS[i] != nullptr; i++) {
        umount2(GPU_PATHS[i], MNT_DETACH);
    }
    
    // Mount کردن
    for (int i = 0; GPU_PATHS[i] != nullptr; i++) {
        char spoof_path[256];
        snprintf(spoof_path, sizeof(spoof_path), "/data/local/tmp/gpu_spoof/file%d", i);
        
        if (access(GPU_PATHS[i], F_OK) == 0 && access(spoof_path, F_OK) == 0) {
            if (mount(spoof_path, GPU_PATHS[i], nullptr, MS_BIND | MS_RDONLY, nullptr) == 0) {
                LOGI("Mounted: %s", GPU_PATHS[i]);
            }
        }
    }
    
    // Mount egl.cfg
    for (int i = 0; EGL_PATHS[i] != nullptr; i++) {
        if (access(EGL_PATHS[i], F_OK) == 0) {
            umount2(EGL_PATHS[i], MNT_DETACH);
            mount("/data/local/tmp/gpu_spoof/egl.cfg", EGL_PATHS[i], nullptr, MS_BIND | MS_RDONLY, nullptr);
        }
    }
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
    
    if (!resetprop) {
        LOGE("resetprop not found");
        return;
    }
    
    // تغییر properties
    char cmd[256];
    
    // GPU properties
    system((std::string(resetprop) + " ro.hardware.egl adreno").c_str());
    system((std::string(resetprop) + " ro.board.platform lahaina").c_str());
    system((std::string(resetprop) + " ro.chipname lahaina").c_str());
    system((std::string(resetprop) + " ro.vendor.gpu.model \"Adreno 830\"").c_str());
    system((std::string(resetprop) + " vendor.gpu.model \"Adreno 830\"").c_str());
    system((std::string(resetprop) + " ro.opengles.version 196608").c_str());
    
    LOGI("Build properties spoofed");
}

void cleanup() {
    LOGI("Cleaning up...");
    
    // Unmount
    for (int i = 0; GPU_PATHS[i] != nullptr; i++) {
        umount2(GPU_PATHS[i], MNT_DETACH);
    }
    
    for (int i = 0; EGL_PATHS[i] != nullptr; i++) {
        umount2(EGL_PATHS[i], MNT_DETACH);
    }
    
    // حذف فایل‌های موقت
    system("rm -rf /data/local/tmp/gpu_spoof");
}

void setupGPUSpoof() {
    LOGI("Setting up Adreno 830 GPU spoof...");
    
    createSpoofFiles();
    spoofBuildProps();
    mountSpoofFiles();
    
    LOGI("GPU spoof setup complete");
}

// تابع companion اصلی
void companion(int fd) {
    LOGI("Companion process started");
    
    char buffer[256];
    ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
    
    if (bytes > 0) {
        buffer[bytes] = '\0';
        std::string command = buffer;
        
        int result = -1;
        
        if (command == "setup_gpu_spoof") {
            setupGPUSpoof();
            result = 0;
        } else if (command == "cleanup") {
            cleanup();
            result = 0;
        } else if (command == "test") {
            result = 0;
        }
        
        write(fd, &result, sizeof(result));
    }
    
    close(fd);
}
