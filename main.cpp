#include <jni.h>
#include <string>
#include <zygisk.hpp>
#include <android/log.h>
#include <unordered_set>
#include <dlfcn.h>

#define LOG_TAG "GPU830Spoof"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// پکیج هدف
static const char* TARGET_PACKAGE = "flar2.devcheck";

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
        if (!package_name) {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        
        LOGI("Checking package: %s", package_name);
        
        // فقط برای DevCheck
        if (strcmp(package_name, TARGET_PACKAGE) == 0) {
            LOGI("Target package found! Setting up GPU 830 spoof...");
            
            // اجرای اسپوف از طریق companion
            executeCompanionCommand("setup_gpu_spoof");
            
            // Hook کردن کتابخانه‌های گرافیکی
            setupGLHooks();
            
            // نگه داشتن ماژول در حافظه
            api->setOption(zygisk::Option::FORCE_DENYLIST_UNMOUNT);
        } else {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
        
        env->ReleaseStringUTFChars(args->nice_name, package_name);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs* args) override {
        // بعد از اجرای برنامه، کتابخانه را unload کن
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api* api;
    JNIEnv* env;
    
    bool executeCompanionCommand(const std::string& command) {
        int fd = api->connectCompanion();
        if (fd < 0) {
            LOGE("Failed to connect to companion");
            return false;
        }
        
        write(fd, command.c_str(), command.length() + 1);
        
        int result = -1;
        read(fd, &result, sizeof(result));
        close(fd);
        
        return result == 0;
    }
    
    void setupGLHooks() {
        LOGI("Setting up OpenGL/Vulkan hooks...");
        
        // Hook کردن OpenGL ES
        hookFunction("libGLESv2.so", "glGetString", (void*)hooked_glGetString);
        hookFunction("libGLESv1_CM.so", "glGetString", (void*)hooked_glGetString);
        hookFunction("libGLESv3.so", "glGetString", (void*)hooked_glGetString);
        
        // Hook کردن EGL
        hookFunction("libEGL.so", "eglQueryString", (void*)hooked_eglQueryString);
        
        // Hook کردن Vulkan (اگر موجود بود)
        hookFunction("libvulkan.so", "vkGetPhysicalDeviceProperties", 
                    (void*)hooked_vkGetPhysicalDeviceProperties);
    }
    
    void hookFunction(const char* libName, const char* funcName, void* newFunc) {
        void* handle = dlopen(libName, RTLD_LAZY);
        if (!handle) {
            LOGI("Library %s not found", libName);
            return;
        }
        
        void* original = dlsym(handle, funcName);
        if (original) {
            LOGI("Found %s in %s", funcName, libName);
            // در اینجا باید از روش‌های hooking مانند PLT/GOT استفاده کنی
            // برای سادگی، inline hooking با استفاده از zygisk inline hooking API
        }
        
        dlclose(handle);
    }
    
    // توابع hook شده
    static const char* hooked_glGetString(unsigned int name) {
        LOGI("glGetString called with: 0x%X", name);
        
        // مقادیر جعلی برای Adreno 830
        switch (name) {
            case 0x1F00: // GL_VENDOR
                return "Qualcomm";
            case 0x1F01: // GL_RENDERER
                return "Adreno (TM) 830";
            case 0x1F02: // GL_VERSION
                return "OpenGL ES 3.2";
            case 0x1F03: // GL_EXTENSIONS
                return "GL_EXT_debug_marker GL_OES_EGL_image GL_OES_EGL_sync "
                       "GL_OES_vertex_half_float GL_OES_framebuffer_object "
                       "GL_OES_rgb8_rgba8 GL_OES_compressed_ETC1_RGB8_texture "
                       "GL_EXT_compressed_ETC1_RGB8_sub_texture GL_OES_standard_derivatives "
                       "GL_OES_depth24 GL_OES_depth_texture GL_OES_packed_depth_stencil";
            default:
                // برای سایر موارد، تابع اصلی را فراخوانی کن
                // باید تابع اصلی را ذخیره و استفاده کنیم
                return "Adreno 830";
        }
    }
    
    static const char* hooked_eglQueryString(void* display, int name) {
        LOGI("eglQueryString called with: %d", name);
        
        // مقادیر جعلی
        switch (name) {
            case 0x3053: // EGL_VENDOR
                return "Qualcomm";
            case 0x3054: // EGL_VERSION
                return "1.5";
            case 0x3055: // EGL_EXTENSIONS
                return "EGL_KHR_image EGL_KHR_image_base EGL_KHR_gl_texture_2D_image "
                       "EGL_KHR_gl_texture_cubemap_image EGL_KHR_gl_renderbuffer_image "
                       "EGL_KHR_reusable_sync EGL_KHR_fence_sync EGL_EXT_create_context_robustness "
                       "EGL_KHR_wait_sync EGL_ANDROID_native_fence_sync EGL_ANDROID_front_buffer_auto_refresh";
            case 0x3056: // EGL_CLIENT_APIS
                return "OpenGL_ES";
            default:
                return "Adreno";
        }
    }
    
    static void hooked_vkGetPhysicalDeviceProperties(void* physicalDevice, void* properties) {
        LOGI("vkGetPhysicalDeviceProperties called");
        
        // باید ساختار را پر کنیم
        // برای سادگی فقط لاگ می‌کنیم
    }
};

REGISTER_ZYGISK_MODULE(GPU830Module)
