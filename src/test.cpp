#include <iostream>
#include <thread>
#include <unistd.h>
#include "./ffmpeg/ffmpeg_getframe.h"
#include "./mpp/mpp_decoder.h"
#include "./vibe/vibe.h"
#include "./queue/queue.h"
#include "./liu/liu.h"
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

uint32_t full = 0;
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
    std::thread v(vibe, std::ref(Mpp_D), std::ref(Frame_queue), std::ref(Mat_queue));
    std::thread l(liu, std::ref(Mat_queue));
    MppFrame temp;
    while (1)
    {
        if (Mpp_D.packet_Init(f.getPacket()) == false)
        {
            usleep(1000); 
            f.packet_deinit();
            continue;
        }
        if (Mpp_D.Decode() == false)
        {
            Mpp_D.Frame_Deinit();
            f.packet_deinit();
            std::cout << "[Total] Decode false!\n";
            continue;
        }
        if (Frame_queue.queue_push(Mpp_D.dec_frame) == false)
        {
            Mpp_D.Frame_Deinit();
            f.packet_deinit();
            std::cout << full++ << std::endl;
            continue;
        }
        //需要av_packet_unref释放，否则会泄漏内存
        f.packet_deinit();
        Mpp_D.dec_frame = nullptr;
    }
    exit(EXIT_SUCCESS);
}
