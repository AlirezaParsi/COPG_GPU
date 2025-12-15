#include <jni.h>
#include <string>
#include <unistd.h>
#include <string.h>
#include <android/log.h>
#include "zygisk.hpp"

#define LOG_TAG "GPU830Spoof"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

static const char* TARGET_PACKAGE = "flar2.devcheck";

// ============================================
// JNI Hook برای کلاس‌های DevCheck
// ============================================

// ذخیره متدهای اصلی
static jmethodID original_getHardwareInfo = nullptr;
static jmethodID original_getGpuInfo = nullptr;
static jobject original_hardware_instance = nullptr;

// تابع جایگزین برای getHardwareInfo
static jstring hook_getHardwareInfo(JNIEnv* env, jobject thiz) {
    LOGI("hook_getHardwareInfo called - returning Adreno 830");
    
    // برگرداندن رشته جعلی
    const char* fake_info = 
        "Adreno 830\n"
        "Vendor: Qualcomm\n"
        "Device ID: 0x430514C1\n"
        "Revision: r5p0\n"
        "Clock Speed: 900 MHz\n"
        "Memory: 12 GB\n"
        "Shaders: 3072\n"
        "Process: 4 nm";
    
    return env->NewStringUTF(fake_info);
}

// تابع جایگزین برای getGpuInfo
static jstring hook_getGpuInfo(JNIEnv* env, jobject thiz) {
    LOGI("hook_getGpuInfo called - returning Adreno 830 info");
    
    const char* fake_gpu = 
        "GPU: Adreno 830\n"
        "Vendor: Qualcomm Technologies, Inc.\n"
        "Renderer: Adreno (TM) 830\n"
        "OpenGL ES: 3.2\n"
        "Vulkan: 1.3\n"
        "Max Texture Size: 16384\n"
        "Max Cubemap Size: 16384\n"
        "Max Renderbuffer Size: 16384";
    
    return env->NewStringUTF(fake_gpu);
}

// ============================================
// Zygisk Module
// ============================================

class GPU830Module : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
        LOGI("GPU 830 Spoof Module loaded");
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        if (!args || !args->nice_name) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        
        const char* package_name = env->GetStringUTFChars(args->nice_name, nullptr);
        bool is_devcheck = false;
        
        if (package_name) {
            LOGD("Checking package: %s", package_name);
            is_devcheck = (strcmp(package_name, TARGET_PACKAGE) == 0);
            env->ReleaseStringUTFChars(args->nice_name, package_name);
        }
        
        if (is_devcheck) {
            LOGI("🎯 DevCheck detected! Setting up GPU spoof hooks");
            setupJNIHooks();
            api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);
        } else {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
        }
    }
    
    void postAppSpecialize(const zygisk::AppSpecializeArgs* args) override {
        // Hook کردن JNI بعد از لود شدن برنامه
        hookDevCheckClasses();
    }

private:
    zygisk::Api* api;
    JNIEnv* env;
    
    void setupJNIHooks() {
        LOGI("Setting up JNI hooks for GPU spoof");
        // اینجا می‌توانیم بعداً inline hooking اضافه کنیم
    }
    
    void hookDevCheckClasses() {
        LOGI("Attempting to hook DevCheck classes...");
        
        // صبر کن تا کلاس‌ها لود شوند
        sleep(1);
        
        JNIEnv* current_env = nullptr;
        JavaVM* vm = nullptr;
        
        if (env->GetJavaVM(&vm) == JNI_OK) {
            vm->AttachCurrentThread(&current_env, nullptr);
            
            if (current_env) {
                tryHookClasses(current_env);
                vm->DetachCurrentThread();
            }
        }
    }
    
    void tryHookClasses(JNIEnv* jni_env) {
        // یافتن کلاس‌های DevCheck
        jclass hardware_class = jni_env->FindClass("flar2/devcheck/modules/hardware/HardwareInfo");
        if (hardware_class) {
            LOGI("Found HardwareInfo class");
            
            // پیدا کردن متد getHardwareInfo
            jmethodID getHardwareInfo = jni_env->GetMethodID(hardware_class, "getHardwareInfo", "()Ljava/lang/String;");
            if (getHardwareInfo) {
                LOGI("Found getHardwareInfo method - would hook here");
                // در اینجا باید JNI method hooking انجام شود
            }
            
            jni_env->DeleteLocalRef(hardware_class);
        }
        
        jclass gpu_class = jni_env->FindClass("flar2/devcheck/modules/gpu/GPUInfo");
        if (gpu_class) {
            LOGI("Found GPUInfo class");
            jni_env->DeleteLocalRef(gpu_class);
        }
    }
};

REGISTER_ZYGISK_MODULE(GPU830Module)
