#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <android/log.h>

#define LOG_TAG "GPU830Companion"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

void createAndMountSpoof() {
    LOGI("Creating GPU spoof files...");
    
    // ایجاد فایل موقت
    FILE* fp = fopen("/data/local/tmp/gpu_spoof.tmp", "w");
    if (fp) {
        fprintf(fp, "Adreno 830\n");
        fclose(fp);
        chmod("/data/local/tmp/gpu_spoof.tmp", 0444);
    }
    
    // مسیرهای احتمالی GPU
    const char* gpu_paths[] = {
        "/sys/class/kgsl/kgsl-3d0/gpu_model",
        "/sys/class/drm/card0/device/gpu_info",
        "/proc/gpuinfo",
        nullptr
    };
    
    // Mount کردن
    for (int i = 0; gpu_paths[i] != nullptr; i++) {
        if (access(gpu_paths[i], F_OK) == 0) {
            umount2(gpu_paths[i], MNT_DETACH);
            mount("/data/local/tmp/gpu_spoof.tmp", gpu_paths[i], nullptr, MS_BIND | MS_RDONLY, nullptr);
            LOGI("Mounted spoof to: %s", gpu_paths[i]);
        }
    }
    
    LOGI("GPU spoof setup complete");
}

void companion(int fd) {
    LOGI("Companion started");
    
    char buffer[256];
    ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
    
    if (bytes > 0) {
        buffer[bytes] = '\0';
        
        int result = -1;
        
        if (strcmp(buffer, "setup_gpu_spoof") == 0) {
            createAndMountSpoof();
            result = 0;
        }
        
        write(fd, &result, sizeof(result));
    }
    
    close(fd);
}
