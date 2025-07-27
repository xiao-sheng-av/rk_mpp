#ifndef MPP_DECODER_H_
#define MPP_DECODER_H_
//需要在Ctrl + Shift + P 打开命令面板，然后输入并选择 C/C++: Edit Configurations (UI)，然后输入/usr/include/rockchip
#include "rk_mpi.h"
#include "mpp_frame.h"
#include "rk_type.h"
#include "rk_venc_cmd.h"
extern "C"{
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
}
class MppDecoder
{
private:
	int ret = -1;
	// 解码上下文
    MppCtx dec_ctx = NULL;	
	//解码接口	
	MppApi *dec_mpi = NULL;
	//解码包		
	MppPacket dec_packet = NULL; 
	//解码cmd设置
	MpiCmd mpi_cmd = MPP_CMD_BASE; 
	//解码参数
	MppParam param = NULL;		   
	//解码模式
	RK_U32 need_split = 0;		   
	//宽
	RK_U32 width;	
	//高
	RK_U32 height; 
	//虚宽
	RK_U32 hor_stride = 0;
	//虚高
	RK_U32 ver_stride = 0;
	//视频格式MPP_VIDEO_CodingAVC为h264
	MppCodingType type = MPP_VIDEO_CodingAVC;
	//buffer池	
	MppBufferGroup frm_grp = NULL; 
	bool pkt_done;					
public:
	MppFrame dec_frame = nullptr;
	MppDecoder(int w = 1920, int h = 1080) : width(w), height(h) {};
	~MppDecoder();
	bool Init();
	bool packet_Init(const AVPacket * av_packet);
	bool Decode();
	bool Group_Init(const AVPacket * av_packet);
};

#endif