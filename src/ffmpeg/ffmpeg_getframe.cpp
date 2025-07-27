#include "ffmpeg_getframe.h"

FFmpeg::~FFmpeg()
{
    if (av_packet)
        av_packet_free(&av_packet);
	if (inFmtCtx)
		avformat_free_context(inFmtCtx);
}

bool FFmpeg::Init()
{
    std::string temp;
    // 查询并返回解复用器
    inFmt = av_find_input_format("v4l2"); 
	if (inFmt == NULL)
	{
		av_log(NULL, AV_LOG_ERROR, "find_input_v4l2 fial");
		return false;
	}

	options = NULL;
    // 设置输入格式
	av_dict_set(&options, "input_format", format.c_str(), 0);
    // 设置分辨率   分辨率根据需要修改
    temp = std::to_string(width) + "x" + std::to_string(height);
	av_dict_set(&options, "video_size", temp.c_str(), 0);	
    // 设置帧率   帧率也是根据需要修改			  
    temp = std::to_string(fps);
	av_dict_set(&options, "framerate", temp.c_str(), 0);

    // 创建一个上下文
	inFmtCtx = avformat_alloc_context();	
    // 打开输入流并绑定上下文，并设置参数  /dev/video0就是摄像头的结点，根据自己的结点改			  
	ret = avformat_open_input(&inFmtCtx, VideoDeviceNode, inFmt, &options); 
	if (ret != 0)
	{
		av_log(NULL, AV_LOG_ERROR, "open_input fial");
		return false;
	}
    // 通过上下文寻找流，并填充上下文相关信息
	ret = avformat_find_stream_info(inFmtCtx, NULL); 
	if (ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "find_stream fial");
		return false;
	}
    // 通过上下文返回流的索引
	videoIndex = av_find_best_stream(inFmtCtx, AVMEDIA_TYPE_VIDEO, false, false, NULL, 0);
	if (videoIndex < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "find_best_stream fial");
		return false;
	}
    // 创建avpacket
	av_packet = av_packet_alloc(); 
	if (!av_packet)
	{
		av_log(NULL, AV_LOG_ERROR, "av_packet_alloc fail\n");
        return false;
	}
    return true;
}

AVPacket *FFmpeg::getPacket()
{
	if (av_read_frame(inFmtCtx, av_packet) >= 0)
	{
		if (av_packet->stream_index == videoIndex) 
		{
			return av_packet;
		}
	}
	return nullptr;
}