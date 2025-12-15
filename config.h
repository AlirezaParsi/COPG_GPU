#ifndef GPU_SPOOF_CONFIG_H
#define GPU_SPOOF_CONFIG_H

// تنظیمات Adreno 830
#define GPU_MODEL "Adreno 830"
#define GPU_VENDOR "Qualcomm"
#define GPU_RENDERER "Adreno (TM) 830"
#define OPENGL_VERSION "OpenGL ES 3.2"
#define VULKAN_VERSION "1.3"
#define CHIP_NAME "lahaina"
#define PLATFORM "lahaina"
#define SOC_MODEL "SM8550"
#define DEVICE_ID "0x430514C1"

// پکیج‌های هدف
static const char* TARGET_PACKAGES[] = {
    "flar2.devcheck",
    "com.flar2.devcheck",
    "dev.flar2.check",
    nullptr
};

// مسیرهای اضافی برای GPU detection
static const char* ADDITIONAL_PATHS[] = {
    // Vendor properties
    "/vendor/etc/vintf/manifest.xml",
    "/vendor/build.prop",
    "/odm/etc/vintf/manifest.xml",
    
    // HW info
    "/proc/device-tree/model",
    "/proc/cpuinfo",
    
    // DRM paths
    "/sys/class/drm/card0/device/vendor",
    "/sys/class/drm/card0/device/device",
    "/sys/class/drm/card0/device/revision",
    
    // PowerVR
    "/sys/devices/platform/pvrsrvkm.0/sgx_clk",
    
    // Mali
    "/sys/class/misc/mali0/device/utilisation",
    "/sys/class/misc/mali0/device/core_mask",
    
    nullptr
};

#endif // GPU_SPOOF_CONFIG_H
