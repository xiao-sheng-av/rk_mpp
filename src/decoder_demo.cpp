#include <iostream>
#include "./ffmpeg/ffmpeg_getframe.h"
#include "./mpp/mpp_decoder.h"
int main()
{
    avdevice_register_all();
    FFmpeg f;
    if (f.Init() == false)
        return -1;
    MppDecoder m;
    if (m.Init() == false)
        return -1;
    if (m.Group_Init(f.getFrame()) == false)
        return -1;
    
    return 0;
}