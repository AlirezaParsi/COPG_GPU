
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
        bool is_target = false;
        
        if (package_name) {
            LOGI("Checking package: %s", package_name);
            is_target = (strcmp(package_name, TARGET_PACKAGE) == 0);
            env->ReleaseStringUTFChars(args->nice_name, package_name);
        }
        
        if (is_target) {
            LOGI("Target package found! Setting up GPU 830 spoof...");
            
            // اجرای اسپوف از طریق companion
            if (executeCompanionCommand("setup_gpu_spoof")) {
                LOGI("GPU spoof setup successful");
                api->setOption(zygisk::Option::FORCE_DENYLIST_UNMOUNT);
            } else {
                LOGE("Failed to setup GPU spoof");
                api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            }
        } else {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs* args) override {
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
};

REGISTER_ZYGISK_MODULE(GPU830Module)
