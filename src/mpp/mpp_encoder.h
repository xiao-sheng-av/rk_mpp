#ifndef MPP_ENCODER_H
#define MPP_ENCODER_H
#include "rk_type.h"
#include "rk_mpi.h"
#include <cstdio> 
class MppEncoder
{
private:
	int ret = -1;
    MppCtx enc_ctx = NULL;				  // 编码上下文
	MppApi *enc_mpi = NULL;				  // 编码接口
	MppEncCfg enc_cfg = NULL;			  // 编码配置
	MppPacket enc_packet = NULL;		  // 编码包

	RK_U32 width = 1920;
	RK_U32 height = 1080;
	RK_U32 hor_stride = -1;
	RK_U32 ver_stride = -1;
	MppFrameFormat fmt = MPP_FMT_YUV420SP;
	int fps = -1;
	int gop;
public:
	MppEncoder(RK_U32 hor, RK_U32 ver, RK_U32 w = 1920, RK_U32 h = 1080, int f =  25, int g = 25);
	~MppEncoder();
	//返回0为正确
	int Init();
	bool Encode(MppFrame frame);
	bool Write_File(FILE * out_file);
};


#endif