#include <iostream>
#include "./ffmpeg/ffmpeg_getframe.h"
#include "./mpp/mpp_decoder.h"
#include "./mpp/mpp_encoder.h"
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

    MppEncoder Mpp_E(Mpp_D.get_hor(), Mpp_D.get_ver(), f.get_width(), f.get_heigh(), f.get_fps(), f.get_fps());
    if (Mpp_E.Init() == 0)
        std::cout << "Mpp_Encoder success\n";

    while (1)
    {
        Mpp_D.packet_Init(f.getPacket());
        Mpp_D.Decode();
        Mpp_E.Encode(Mpp_D.dec_frame);
        Mpp_E.Write_File(out_file);
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