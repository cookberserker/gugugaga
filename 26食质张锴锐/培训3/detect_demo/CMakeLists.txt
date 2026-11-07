cmake_minimum_required(VERSION 3.20)

# vcpkg.cmake 在 project() 时读 VCPKG_TARGET_TRIPLET；本机默认 x64-windows
# 与已装的 x64-mingw-dynamic 不符，须提前修正。显式指定过的环境不覆盖。
if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg[.]cmake$")
    if(NOT DEFINED VCPKG_TARGET_TRIPLET OR VCPKG_TARGET_TRIPLET STREQUAL "x64-windows")
        set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "vcpkg triplet" FORCE)
    endif()
endif()

project(detect_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

add_executable(detect_demo
        main.cpp
        src/armor_common.hpp)

target_include_directories(detect_demo PRIVATE ${OpenCV_INCLUDE_DIRS} src)
target_link_libraries(detect_demo PRIVATE ${OpenCV_LIBS})

# 把源码根路径注入，保证从任意工作目录运行都能兜底找到 assets/
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" DETECT_ROOT_CMAKE)
target_compile_definitions(detect_demo PRIVATE ARMOR_PROJECT_ROOT="${DETECT_ROOT_CMAKE}")

# 测试视频拷到构建目录，这样在 build/ 里运行也读得到 assets/armor.mp4
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets/armor.mp4"
     DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/assets")
