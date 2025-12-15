#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <android/log.h>

#define LOG_TAG "GPU830Companion"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// اطلاعات Adreno 830 برای اسپوف
static const char* ADRENO_830_INFO = 
    "Model: Adreno 830\n"
    "Vendor: Qualcomm\n"
    "Device ID: 0x430514C1\n"
    "Chip ID: 0x0999\n"
    "Revision: r5p0\n"
    "Clock Speed: 900 MHz\n"
    "Bus Interface: AXI\n"
    "Bus Width: 256-bit\n"
    "Memory Size: 12 GB\n"
    "Shaders: 3072\n"
    "Technology: 4 nm\n"
    "OpenGL ES: 3.2\n"
    "Vulkan: 1.3\n"
    "DirectX: 12\n";

// مسیرهای GPU در سیستم
static const char* GPU_PATHS[] = {
    "/sys/class/kgsl/kgsl-3d0/gpu_model",
    "/sys/class/kgsl/kgsl-3d0/gpu_busy",
    "/sys/class/kgsl/kgsl-3d0/gpu_clk",
    "/sys/class/kgsl/kgsl-3d0/max_gpuclk",
    "/sys/class/kgsl/kgsl-3d0/gpu_available_frequencies",
    "/sys/class/drm/card0/device/gpu_info",
    "/sys/kernel/gpu/gpu_model",
    "/proc/gpuinfo",
    "/sys/class/misc/mali0/device/gpu_name",
    "/sys/class/pvr/pvr/gpuinfo",
    "/sys/devices/soc/*/gpu/gpu_model",
    "/sys/devices/platform/*/gpu/gpu_model",
    nullptr
};

// مسیرهای EGL/Vulkan
static const char* EGL_PATHS[] = {
    "/system/lib/egl/egl.cfg",
    "/system/lib64/egl/egl.cfg",
    "/vendor/lib/egl/egl.cfg",
    "/vendor/lib64/egl/egl.cfg",
    "/system/vendor/lib/egl/egl.cfg",
    "/system/vendor/lib64/egl/egl.cfg",
    "/vendor/lib64/hw/gralloc.*.so",
    "/vendor/lib/hw/gralloc.*.so",
    nullptr
};

void createSpoofFiles() {
    LOGI("Creating GPU spoof files...");
    
    // 1. فایل‌های GPU اصلی
    for (int i = 0; GPU_PATHS[i] != nullptr; i++) {
        // بررسی وجود مسیر
        if (access(GPU_PATHS[i], F_OK) == 0) {
            char spoof_path[256];
            snprintf(spoof_path, sizeof(spoof_path), "/data/local/tmp/gpu_spoof_%d", i);
            
            // ایجاد فایل جعلی
            FILE* fp = fopen(spoof_path, "w");
            if (fp) {
                if (strstr(GPU_PATHS[i], "gpu_model") || strstr(GPU_PATHS[i], "gpu_name")) {
                    fprintf(fp, "Adreno 830\n");
                } else if (strstr(GPU_PATHS[i], "gpu_busy")) {
                    fprintf(fp, "45\n"); // 45% usage
                } else if (strstr(GPU_PATHS[i], "max_gpuclk")) {
                    fprintf(fp, "900000000\n"); // 900 MHz
                } else if (strstr(GPU_PATHS[i], "gpu_available_frequencies")) {
                    fprintf(fp, "200000000 400000000 600000000 800000000 900000000\n");
                } else if (strstr(GPU_PATHS[i], "gpu_clk")) {
                    fprintf(fp, "600000000\n"); // Current clock
                } else {
                    fprintf(fp, "%s", ADRENO_830_INFO);
                }
                fclose(fp);
                
                // تغییر مجوزها
                chmod(spoof_path, 0444);
                
                LOGI("Created spoof file: %s", spoof_path);
            }
        }
    }
    
    // 2. فایل egl.cfg جعلی
    const char* egl_cfg_path = "/data/local/tmp/egl_spoof.cfg";
    FILE* fp = fopen(egl_cfg_path, "w");
    if (fp) {
        fprintf(fp, "0 1 adreno\n");
        fprintf(fp, "0 0 android\n");
        fclose(fp);
    }
    
    // 3. فایل gralloc جعلی (اسمی)
    const char* gralloc_info = "/data/local/tmp/gralloc_info.txt";
    fp = fopen(gralloc_info, "w");
    if (fp) {
        fprintf(fp, "gralloc.adreno.version=1\n");
        fprintf(fp, "gralloc.adreno.id=0x430514C1\n");
        fprintf(fp, "gralloc.adreno.revision=r5p0\n");
        fclose(fp);
    }
}

void mountSpoofFiles() {
    LOGI("Mounting spoof files...");
    
    // 1. Unmount اولیه (اگر mount شده)
    for (int i = 0; GPU_PATHS[i] != nullptr; i++) {
        if (access(GPU_PATHS[i], F_OK) == 0) {
            umount2(GPU_PATHS[i], MNT_DETACH);
        }
    }
    
    // 2. Mount کردن فایل‌های GPU
    for (int i = 0; GPU_PATHS[i] != nullptr; i++) {
        char spoof_path[256];
        snprintf(spoof_path, sizeof(spoof_path), "/data/local/tmp/gpu_spoof_%d", i);
        
        if (access(spoof_path, F_OK) == 0 && access(GPU_PATHS[i], F_OK) == 0) {
            if (mount(spoof_path, GPU_PATHS[i], nullptr, MS_BIND | MS_RDONLY, nullptr) == 0) {
                LOGI("Successfully mounted: %s -> %s", spoof_path, GPU_PATHS[i]);
            } else {
                LOGE("Failed to mount %s to %s", spoof_path, GPU_PATHS[i]);
            }
        }
    }
    
    // 3. Mount کردن egl.cfg
    for (int i = 0; EGL_PATHS[i] != nullptr; i++) {
        if (access(EGL_PATHS[i], F_OK) == 0) {
            umount2(EGL_PATHS[i], MNT_DETACH);
            
            if (mount("/data/local/tmp/egl_spoof.cfg", EGL_PATHS[i], 
                     nullptr, MS_BIND | MS_RDONLY, nullptr) == 0) {
                LOGI("Mounted egl.cfg to: %s", EGL_PATHS[i]);
            }
        }
    }
}

void spoofBuildProps() {
    LOGI("Spoofing build properties...");
    
    // یافتن resetprop
    const char* resetprop_paths[] = {
        "/data/adb/magisk/resetprop",
        "/data/adb/ksu/bin/resetprop",
        "/system/bin/resetprop",
        "/debug_ramdisk/resetprop",
        nullptr
    };
    
    const char* resetprop = nullptr;
    for (int i = 0; resetprop_paths[i] != nullptr; i++) {
        if (access(resetprop_paths[i], X_OK) == 0) {
            resetprop = resetprop_paths[i];
            break;
        }
    }
    
    if (!resetprop) {
        LOGE("resetprop not found!");
        return;
    }
    
    // تغییر properties برای Adreno 830
    char cmd[512];
    
    // properties اصلی GPU
    const char* props[] = {
        "ro.hardware.egl adreno",
        "ro.board.platform lahaina",
        "ro.chipname lahaina",
        "ro.mediatek.platform mt6983",
        "ro.product.board lahaina",
        "ro.soc.model SM8550",
        "ro.hardware.vulkan adreno",
        "ro.opengles.version 196608", // OpenGL ES 3.0
        "ro.vendor.gpu.model Adreno 830",
        "ro.vendor.gpu.vendor Qualcomm",
        "ro.vendor.gpu.driver 512",
        nullptr
    };
    
    for (int i = 0; props[i] != nullptr; i += 2) {
        snprintf(cmd, sizeof(cmd), "%s %s %s", resetprop, props[i], props[i+1]);
        system(cmd);
    }
    
    // تغییر vendor properties
    snprintf(cmd, sizeof(cmd), "%s ro.vendor.gpu.model \"Adreno 830\"", resetprop);
    system(cmd);
    
    snprintf(cmd, sizeof(cmd), "%s vendor.opengles.version 196608", resetprop);
    system(cmd);
    
    LOGI("Build properties spoofed");
}

void cleanup() {
    LOGI("Cleaning up...");
    
    // Unmount کردن همه چیز
    for (int i = 0; GPU_PATHS[i] != nullptr; i++) {
        umount2(GPU_PATHS[i], MNT_DETACH);
    }
    
    for (int i = 0; EGL_PATHS[i] != nullptr; i++) {
        umount2(EGL_PATHS[i], MNT_DETACH);
    }
    
    // حذف فایل‌های موقت
    system("rm -f /data/local/tmp/gpu_spoof_*");
    system("rm -f /data/local/tmp/egl_spoof.cfg");
    system("rm -f /data/local/tmp/gralloc_info.txt");
}

void setupGPUSpoof() {
    LOGI("=== Starting Adreno 830 GPU Spoof ===");
    
    // 1. ایجاد فایل‌های جعلی
    createSpoofFiles();
    
    // 2. تغییر build properties
    spoofBuildProps();
    
    // 3. Mount کردن فایل‌ها
    mountSpoofFiles();
    
    // 4. تغییر مجوزها برای برخی فایل‌ها
    system("chmod 444 /sys/class/kgsl/kgsl-3d0/* 2>/dev/null");
    system("chmod 444 /sys/class/drm/card0/device/* 2>/dev/null");
    
    LOGI("=== GPU Spoof Setup Complete ===");
}

void restoreOriginal() {
    LOGI("Restoring original GPU info...");
    
    cleanup();
    
    // بازنشانی properties به حالت اصلی
    const char* resetprop = "/data/adb/magisk/resetprop";
    if (access(resetprop, X_OK) == 0) {
        // خواندن properties اصلی از build.prop
        system("resetprop -v ro.hardware.egl");
        system("resetprop -v ro.board.platform");
        system("resetprop -v ro.chipname");
        system("resetprop -v vendor.gpu.model");
    }
}

// تابع companion اصلی
void companion(int fd) {
    LOGI("GPU Spoof Companion started");
    
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
        } else if (command == "restore") {
            restoreOriginal();
            result = 0;
        } else if (command == "test") {
            // تست وجود مسیرها
            for (int i = 0; GPU_PATHS[i] != nullptr; i++) {
                if (access(GPU_PATHS[i], F_OK) == 0) {
                    LOGI("Found: %s", GPU_PATHS[i]);
                }
            }
            result = 0;
        }
        
        write(fd, &result, sizeof(result));
    }
    
    close(fd);
}
