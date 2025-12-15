cmake_minimum_required(VERSION 3.21)
project(GPU830Spoof)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# تنظیمات بهینه‌سازی
add_compile_options(
    -Os 
    -flto 
    -ffunction-sections 
    -fdata-sections 
    -fvisibility=hidden
    -fPIC
    -Wall
    -Wno-unused-parameter
    -Wno-unused-variable
    -DZYGISK_ENABLED
)

add_link_options(
    -flto 
    -Wl,--gc-sections 
    -Wl,--exclude-libs,ALL
    -Wl,--no-undefined
)

if(NOT DEBUG)
    add_compile_options(-g0 -DNDEBUG)
    add_link_options(-s)
endif()

# فایل منبع - فقط یک فایل!
set(SOURCES main.cpp)

# کتابخانه اصلی
add_library(gpu_spoof SHARED ${SOURCES})

# مسیرهای include
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/zygisk)

# پیدا کردن کتابخانه‌های مورد نیاز
find_library(log-lib log)
find_library(android-lib android)

# لینک کتابخانه‌ها
target_link_libraries(gpu_spoof
    ${log-lib}
    ${android-lib}
)

# تنظیم نام خروجی
set_target_properties(gpu_spoof PROPERTIES
    PREFIX ""
    OUTPUT_NAME "gpu_spoof"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# پیام اطلاعات
message(STATUS "Building GPU 830 Spoof Module")
message(STATUS "Target: ${PROJECT_NAME}")
message(STATUS "Output: ${CMAKE_CURRENT_BINARY_DIR}/libgpu_spoof.so")
