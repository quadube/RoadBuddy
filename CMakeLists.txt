cmake_minimum_required(VERSION 3.15.5)

set(CMAKE_CC_COMPILER "/home/joao22/buildroot/output/host/bin/arm-buildroot-linux-uclibcgnueabihf-gcc")
set(CMAKE_CXX_COMPILER "/home/joao22/buildroot/output/host/bin/arm-buildroot-linux-uclibcgnueabihf-g++")
project(roadbuddy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(HAVE_FFMPEG 1)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)

include_directories(${OpenCV_INCLUDE_DIRS}, ${PROJECT_SOURCE_DIR}/inc)

file(GLOB all_SRCS
	"${PROJECT_SOURCE_DIR}/src/*.cpp"
	"${PROJECT_SOURCE_DIR}/inc/*.h"
)


find_package(Threads REQUIRED)
find_package(OpenCV REQUIRED PATHS /home/joao22/buildroot/output/host/arm-buildroot-linux-uclibcgnueabihf/sysroot/usr/share/OpenCV)

add_executable(roadbuddy main.cpp ${all_SRCS})
target_link_libraries(roadbuddy ${OpenCV_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} -lrt -DWITH_FFMPEG=ON)
