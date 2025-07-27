#ifndef FFMPEG_GETFRAME_H_
#define FFMPEG_GETFRAME_H_
extern "C"{
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
}
#include <string>
#define VideoDeviceNode "/dev/video0"

class FFmpeg
{
private:
    int ret = -1;
    int width = -1, height = -1;
    int fps = -1;
    std::string format = "h264";
    const AVInputFormat *inFmt = nullptr;
    AVDictionary *options = nullptr;
    AVFormatContext *inFmtCtx = nullptr;
    int videoIndex = -1;
    AVPacket *av_packet = nullptr;
public:
    FFmpeg(int w = 1920, int h = 1080, int f = 25) : width(w), height(h), fps(f){}
    ~FFmpeg();
    bool Init();
    const AVPacket * getFrame();
    int get_fps() const {return fps;}
    int get_width() const {return width;}
    int get_heigh() const {return height;}
};


#endif