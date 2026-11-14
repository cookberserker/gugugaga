cmake_minimum_required(VERSION 3.20)
project(calib_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

add_executable(calib_demo main.cpp)
target_include_directories(calib_demo PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(calib_demo PRIVATE ${OpenCV_LIBS})

file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/shots")
