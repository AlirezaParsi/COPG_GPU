#include <jni.h>
#include <string>
#include "zygisk.hpp"
#include <android/log.h>

#define LOG_TAG "GPU830Spoof"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static const char* TARGET_PACKAGE = "flar2.devcheck";

class GPU830Module : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        if (!args || !args->nice_name) {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        
        const char* package_name = env->GetStringUTFChars(args->nice_name, nullptr);
        
        if (package_name && strcmp(package_name, TARGET_PACKAGE) == 0) {
            LOGI("GPU 830 spoof activated for: %s", package_name);
            
            // اجرای اسپوف از طریق companion
            int fd = api->connectCompanion();
            if (fd >= 0) {
                const char* cmd = "setup_gpu_spoof";
                write(fd, cmd, strlen(cmd) + 1);
                
                int result = -1;
                read(fd, &result, sizeof(result));
                close(fd);
                
                if (result == 0) {
                    LOGI("GPU spoof setup successful");
                    api->setOption(zygisk::Option::FORCE_DENYLIST_UNMOUNT);
                } else {
                    api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
                }
            }
        } else {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
        
        if (package_name) {
            env->ReleaseStringUTFChars(args->nice_name, package_name);
        }
    }
    
    void postAppSpecialize(const zygisk::AppSpecializeArgs* args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api* api;
    JNIEnv* env;
};

REGISTER_ZYGISK_MODULE(GPU830Module)
