#include <iostream>
#include "./ffmpeg/ffmpeg_getframe.h"
#include "./mpp/mpp_decoder.h"
#include <unistd.h>
bool Init(FFmpeg &f, MppDecoder &m);
int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cout << "pleace enter out path\n";
        exit(EXIT_FAILURE);
    }
    FILE * out_file = fopen(argv[1], "w + b");
    FFmpeg f;
    MppDecoder Mpp_D(f.get_width(), f.get_heigh());
    if (Init(f, Mpp_D) == false)
    {
        std::cout << "[Total] Init false!\n";
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        if (Mpp_D.packet_Init(f.getPacket()) == false)
        {
            usleep(1000);
            goto deinit;
        }
        if (Mpp_D.Decode() == false)
        {
            std::cout << "[Total] Decode false!\n";
            goto deinit;
            
        }
        // if (Mpp_D.Write_File(out_file) == false)
        // {
        //     std::cout << "[Total] Write File false!\n";
        //     goto deinit;
        // }
deinit:
        f.packet_deinit();
        Mpp_D.Frame_Deinit();
    }
    exit(EXIT_SUCCESS);
}

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