#include "mpp_encoder.h"
MppEncoder::MppEncoder(RK_U32 hor, RK_U32 ver,
                       RK_U32 w, RK_U32 h, int f, int g) : hor_stride(hor),
                                                           ver_stride(ver), width(w), height(h), fps(f), gop(g)
{
}

MppEncoder::~MppEncoder()
{
    if (enc_ctx)
    {
        mpp_destroy(enc_ctx);
        enc_ctx = nullptr;
    }
    if (enc_cfg)
    {
        mpp_enc_cfg_deinit(enc_cfg);
        enc_cfg = nullptr;
    }
    if (enc_packet)
    {
        mpp_packet_deinit(&enc_packet);
    }
}

// 返回0为正确
int MppEncoder::Init()
{
    ret = mpp_create(&enc_ctx, &enc_mpi); // 创建编码上下文和编码接口
    if (ret)
    {
        return ret;
    }

    ret = mpp_init(enc_ctx, MPP_CTX_ENC, MPP_VIDEO_CodingAVC); // 初始化编码上下文
    if (ret)
    {
        return ret;
    }

    ret = mpp_enc_cfg_init(&enc_cfg); // 初始化编码配置
    if (ret)
    {
        return ret;
    }
    
    mpp_enc_cfg_set_s32(enc_cfg, "prep:width", width);                 // 设置编码宽度
    mpp_enc_cfg_set_s32(enc_cfg, "prep:height", height);               // 设置编码高度
    mpp_enc_cfg_set_s32(enc_cfg, "prep:hor_stride", hor_stride);       // 设置编码水平步幅
    mpp_enc_cfg_set_s32(enc_cfg, "prep:ver_stride", ver_stride);       // 设置编码垂直步幅
    mpp_enc_cfg_set_s32(enc_cfg, "prep:format", fmt);                  // 设置编码格式
    mpp_enc_cfg_set_s32(enc_cfg, "rc:mode", MPP_ENC_RC_MODE_CBR);      // 设置码率模式  CBR:恒定码率  VBR:可变码率
    mpp_enc_cfg_set_s32(enc_cfg, "rc:fps_in_num", fps);                // 设置输入帧率
    mpp_enc_cfg_set_s32(enc_cfg, "rc:fps_in_denorm", 1);               // 设置输入帧率分母
    mpp_enc_cfg_set_s32(enc_cfg, "rc:fps_out_num", fps);               // 设置输出帧率
    mpp_enc_cfg_set_s32(enc_cfg, "rc:fps_out_denorm", 1);              // 设置输出帧率分母
    mpp_enc_cfg_set_s32(enc_cfg, "rc:gop", gop);                       // 设置gop
    mpp_enc_cfg_set_s32(enc_cfg, "rc:bps_target", width * height * 2); // 设置码率目标
    mpp_enc_cfg_set_s32(enc_cfg, "rc:bps_max", width * height * 3);    // 设置码率最大值
    mpp_enc_cfg_set_s32(enc_cfg, "rc:bps_min", width * height * 1);    // 设置码率最小值
    mpp_enc_cfg_set_s32(enc_cfg, "codec:type", MPP_VIDEO_CodingAVC);   // 设置编码类型  h264

    ret = enc_mpi->control(enc_ctx, MPP_ENC_SET_CFG, enc_cfg); // 设置编码配置
    if (ret)
    {
        return ret;
    }
    return 0;
}