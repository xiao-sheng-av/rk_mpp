#include <iostream>
#include <thread>
#include <unistd.h>
#include "./ffmpeg/ffmpeg_getframe.h"
#include "./mpp/mpp_decoder.h"
#include "./vibe/vibe.h"
#include "./queue/queue.h"
bool Init(FFmpeg &f, MppDecoder &m)
{
    avdevice_register_all();
    if (f.Init() == false)
        return false;
    if (m.Init() == false)
        return false;
    if (m.Group_Init(f.getPacket()) == false)
        return false;
    return true;
}


int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cout << "pleace enter out path\n";
        exit(EXIT_FAILURE);
    }
    FFmpeg f;
    CircularQueue<MppFrame> Frame_queue;
    CircularQueue<Mat> Mat_queue;
    FILE *out_file = fopen(argv[1], "w + b");
    MppDecoder Mpp_D(f.get_width(), f.get_heigh());

    if (Init(f, Mpp_D) == false)
    {
        std::cout << "[Total] Init false!\n";
        exit(EXIT_FAILURE);
    }
    // 按引用传递
    std::thread t(vibe, std::ref(f), std::ref(Frame_queue), std::ref(Mat_queue));
   // usleep(2000000);
    while (1)
    {
        MppFrame temp;
        Mpp_D.packet_Init(f.getPacket());
        Mpp_D.Decode();
        if (Frame_queue.queue_push(Mpp_D.dec_frame) == false)
        {
            Frame_queue.queue_pop(&temp); // 队列满则出队
            mpp_frame_deinit(&temp);      // 释放旧帧资源
            Frame_queue.queue_push(Mpp_D.dec_frame); // 重新尝试入队
        }
        Mpp_D.dec_frame = nullptr;
    }
    exit(EXIT_SUCCESS);
}
