#设置版本编号
cmake_minimum_required(VERSION 3.25)

#定义项目的名称和使用的编程语言
project(Parabola CXX)

#添加头文件搜索路径
include_directories(/usr/include/rockchip)

#指定要生成的可执行文件和其源文件
add_executable(decoder_demo 
               ./src/decoder_demo.cpp
               ./src/ffmpeg/ffmpeg_getframe.cpp
               ./src/mpp/mpp_decoder.cpp)

add_executable(encoder_demo 
               ./src/encoder_demo.cpp
               ./src/ffmpeg/ffmpeg_getframe.cpp
               ./src/mpp/mpp_decoder.cpp
               ./src/mpp/mpp_encoder.cpp)
#设置值
set(COMMON_LIBS avcodec avformat avutil avdevice rockchip_mpp)
#链接目标文件与其他库：
target_link_libraries(encoder_demo ${COMMON_LIBS})
target_link_libraries(decoder_demo ${COMMON_LIBS})