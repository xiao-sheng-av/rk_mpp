#include <iostream>
#include "./ffmpeg/ffmpeg_getframe.h"
#include "./mpp/mpp_decoder.h"
bool Init(FFmpeg & f, MppDecoder & m);
int main(int argc, char ** argv)
{
    FFmpeg f;
    MppDecoder m;
    if (argc < 2)
        std::cout << "pleace enter out path\n";
    FILE * out_file = fopen(argv[1], "w+b");
    if (Init(f, m) == false)
        std::cout << "Init false\n";
    if (m.packet_Init(f.getFrame()) == false)
        std::cout << "mpp packet init false\n"; 
    if (m.Decode() == false)
        std::cout << "decode false\n";
    if (m.Write_Packet(out_file) == false)
        std::cout << "write false\n";
    return 0;
}

bool Init(FFmpeg &f, MppDecoder &m)
{
    avdevice_register_all();
    if (f.Init() == false)
        return false;
    if (m.Init() == false)
        return false;
    if (m.Group_Init(f.getFrame()) == false)
        return false;
    return true;
}