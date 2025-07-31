#include <iostream>
#include <thread>
#include <unistd.h>
#include "./ffmpeg/ffmpeg_getframe.h"
#include "./mpp/mpp_decoder.h"
#include "./vibe/vibe.h"
#include "./queue/queue.h"
#include </usr/include/rga/rga.h>
#include </usr/include/rga/im2d.hpp>
#include <sys/time.h>
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

void vibe(FFmpeg &f,CircularQueue<MppFrame> &Frame_queue, CircularQueue<Mat> &Mat_queue)
{
    ViBe Vibe_Model;
    unsigned char *vibe_buf;
    int Start_Flag = 0;
    vibe_buf = (unsigned char *)malloc(f.get_width() * f.get_heigh() * 3 / 2); // buf用来存储解码后的图像数据，解码后是YUV420SP，所以需要开辟空间为1920*1080*3/2
    memset(vibe_buf, 128, f.get_width() * f.get_heigh() * 3 / 2);
    while (1)
    {
        MppFrame dec_frame;
        struct timeval start, end;
		gettimeofday(&start, nullptr);
        Mat FG_img;
        FG_img = Mat::zeros(f.get_heigh(), f.get_width(), CV_8UC1);
        gettimeofday(&end, nullptr);
		long us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec); // 计算时间间隔，单位为微秒
		av_log(NULL, AV_LOG_ERROR, "vibe file time %d\n", us);
        if (Frame_queue.queue_pop(&dec_frame))
        {
            //========rga的copy功能=====4ms
            MppBuffer mpp_buffer = mpp_frame_get_buffer(dec_frame);                                   // 获取frame的buf
            int buf_fd = mpp_buffer_get_fd(mpp_buffer);  
            if (buf_fd < 0 )
            {
                std::cout << "< 0 " << endl;
                continue;
            }                                        // 获取frame的fd，使用fd的话，驱动无需拷贝，可以直接读写fd对应区域的数据，通常fd用的是cma buffer，物理地址是连续的，RGA硬件操作效率会更高
            rga_buffer_t src = wrapbuffer_fd(buf_fd, 1920, 1080, RK_FORMAT_YCbCr_420_SP, 1984, 1088); // 获取输入rga_buffer_t
            rga_buffer_t dst = wrapbuffer_virtualaddr(vibe_buf, 1920, 1080, RK_FORMAT_YCbCr_420_SP);  // 获取输出rga_buffer_t
            imcopy(src, dst);
            //==============4ms
            Mat gray(1080, 1920, CV_8UC1, vibe_buf);
            if (Start_Flag == 0)
            {
                Vibe_Model.Init(gray);
                Start_Flag += 1;
            }
            else
            {
                Vibe_Model.Run(gray);
                FG_img = Vibe_Model.GetFGModel();
            }
            mpp_frame_deinit(&dec_frame);
        }
        else usleep(100); // 队列空时短暂休眠
    }
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
    //按引用传递
    std::thread t(vibe, std::ref(f), std::ref(Frame_queue), std::ref(Mat_queue));
    while (1)
    {
        MppFrame temp;
        Mpp_D.packet_Init(f.getPacket());
        Mpp_D.Decode();
        if (Frame_queue.queue_push(Mpp_D.dec_frame) == false)
        {
            mpp_frame_deinit(&Mpp_D.dec_frame);
        }
        Mpp_D.dec_frame = nullptr;
    }
    exit(EXIT_SUCCESS);
}
