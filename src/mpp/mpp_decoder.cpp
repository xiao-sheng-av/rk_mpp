#include "mpp_decoder.h"
#include <iostream>
#include <unistd.h>
MppDecoder::~MppDecoder()
{
    if (dec_ctx)
    {
        mpp_destroy(dec_ctx);
        dec_ctx = nullptr;
    }
    if (frm_grp)
    {
        mpp_buffer_group_put(frm_grp);
        frm_grp = nullptr;
    }
    if (dec_frame)
    {
        mpp_frame_deinit(&dec_frame);
        dec_frame = nullptr;
    }
}

bool MppDecoder::Init()
{
    // 创建解码上下文和解码接口
    ret = mpp_create(&dec_ctx, &dec_mpi);
    if (MPP_OK != ret)
    {
        std::cout << "[Mpp_Init]" << "mpp_create failed\n";
        return false;
    }
    // 设置解码模式为自动拼接，原因：简单
    mpi_cmd = MPP_DEC_SET_PARSER_SPLIT_MODE;
    // 设置解码模式
    param = &need_split;
    // 设置解码模式
    ret = dec_mpi->control(dec_ctx, mpi_cmd, param);
    if (MPP_OK != ret)
    {
        std::cout << "[Mpp_Init]" << "mpi->control failed\n";
        return false;
    }
    // 设置解码输入为阻塞模式
    mpi_cmd = MPP_SET_INPUT_BLOCK;
    // 设置解码模式
    param = &need_split;
    // 设置解码输入为阻塞模式
    ret = dec_mpi->control(dec_ctx, mpi_cmd, param);
    if (MPP_OK != ret)
    {
        std::cout << "[Mpp_Init]" << "mpi->control failed\n";
        return false;
    }
    // 设置解码输入为阻塞模式
    mpi_cmd = MPP_SET_INPUT_BLOCK;
    // 设置解码模式
    param = &need_split;
    // 设置解码输入为阻塞模式
    ret = dec_mpi->control(dec_ctx, mpi_cmd, param);
    if (MPP_OK != ret)
    {
        std::cout << "[Mpp_Init]" << "mpi->control failed\n";
        return false;
    }
    // 初始化解码上下文
    ret = mpp_init(dec_ctx, MPP_CTX_DEC, type);
    if (MPP_OK != ret)
    {
        std::cout << "[Mpp_Init]" << "mpp_init failed\n";
        return false;
    }
    return true;
}

bool MppDecoder::packet_Init(AVPacket *av_packet)
{
    ret = mpp_packet_init(&dec_packet, av_packet->data, av_packet->size); // 将ffmpeg的packet变成mpp的pakect
    if (ret)
    {
        av_log(NULL, AV_LOG_ERROR, "mpp_packet_init failed\n");
        return false;
    }
    mpp_packet_set_pts(dec_packet, av_packet->pts); // 设置时间戳
    return true;
}

bool MppDecoder::Group_Init(AVPacket *av_packet)
{
    if (packet_Init(av_packet) == false)
        return false;
    pkt_done = false;
    if (!pkt_done) // 1则表示已送入，0表示送入失败
    {
        ret = dec_mpi->decode_put_packet(dec_ctx, dec_packet); // 将pakcet送入到mpp解码器中
        if (MPP_OK == ret)                                     // MPP_OK表示put成功
            pkt_done = true;
        else if (ret == MPP_ERR_BUFFER_FULL) // MPP_ERR_BUFFER_FULL表示mpp解码器已满，则需先get_frame
        {
            std::cout << "[MPP Decode]" << "Buffer full, waiting...\n";
            pkt_done = true;
        }
        else // 否则为失败，将再一次尝试
        {
            std::cout << "[MPP Decode]" << "decode_put_packet failed: %d\n", ret;
        }
    }
    do
    {
        pkt_done = false;
        mpp_frame_deinit(&dec_frame);
        ret = dec_mpi->decode_get_frame(dec_ctx, &dec_frame); // put_packet后就get_frame得出解码的数据
        if (MPP_ERR_TIMEOUT == ret)                           // 超时则等待一下
        {
            usleep(1000);
            continue;
        }
        if (MPP_OK != ret) // 错误
        {
            av_log(NULL, AV_LOG_ERROR, "decode_get_frame failed: %d\n", ret);
            break;
        }
        if (dec_frame) // 判断得到的frame是否为空
        {
            if (mpp_frame_get_info_change(dec_frame)) // 检测frame的信息是否变化（分辨率，帧率等），
            {
                width = mpp_frame_get_width(dec_frame);
                height = mpp_frame_get_height(dec_frame);
                hor_stride = mpp_frame_get_hor_stride(dec_frame);
                ver_stride = mpp_frame_get_ver_stride(dec_frame);
                RK_U32 buf_size = mpp_frame_get_buf_size(dec_frame);

                ret = mpp_buffer_group_get_internal(&frm_grp, MPP_BUFFER_TYPE_DRM); // 创建一个buffer池
                if (ret)
                {
                    av_log(NULL, AV_LOG_ERROR, "mpp_buffer_group_get_internal failed: %d\n", ret);
                    pkt_done = false;
                    break;
                }
                dec_mpi->control(dec_ctx, MPP_DEC_SET_EXT_BUF_GROUP, frm_grp); // 设置buffer池到mpp解码器
                ret = mpp_buffer_group_limit_config(frm_grp, buf_size, 60);    // 此函数用来限制buffer的数量
                if (ret)
                {
                    av_log(NULL, AV_LOG_ERROR, "mpp_buffer_group_limit_config failed: %d\n", ret);
                    pkt_done = false;
                    break;
                }
                dec_mpi->control(dec_ctx, MPP_DEC_SET_INFO_CHANGE_READY, NULL); // 表示准备好了可以开始解码
                pkt_done = true;
            }
        }
        if (pkt_done == true)
            break;
    } while (1);
    std::cout << "[MPP INFO]  " << " width: " << width << " height: " << height << "  hor_stride: " << hor_stride
              << "  ver_stride: " << ver_stride << std::endl;
    return pkt_done;
}

bool MppDecoder::Decode()
{
    pkt_done = false; // 该变量用来表示packet是否被送入到mpp的解码器中
    if (!pkt_done)    // 1则表示已送入，0表示送入失败
    {
        ret = dec_mpi->decode_put_packet(dec_ctx, dec_packet); // 将pakcet送入到mpp解码器中
        if (MPP_OK == ret)                                     // MPP_OK表示put成功
            pkt_done = true;
        else if (ret == MPP_ERR_BUFFER_FULL) // MPP_ERR_BUFFER_FULL表示mpp解码器已满，则需先get_frame
        {
            std::cout << "[MPP Decode]" << "Buffer full, waiting...\n";
            pkt_done = true;
        }
        else // 否则为失败，将再一次尝试
        {
            std::cout << "[MPP Decode]" << "decode_put_packet failed: %d\n", ret;
        }
    }
    do
    {
        pkt_done = false;
        mpp_frame_deinit(&dec_frame);
        ret = dec_mpi->decode_get_frame(dec_ctx, &dec_frame); // put_packet后就get_frame得出解码的数据
        if (MPP_ERR_TIMEOUT == ret)                           // 超时则等待一下
        {
            usleep(1000);
            continue;
        }
        if (MPP_OK != ret) // 错误
        {
            av_log(NULL, AV_LOG_ERROR, "decode_get_frame failed: %d\n", ret);
            break;
        }

        if (dec_frame) // 判断得到的frame是否为空
        {
            pkt_done = true;
        }
        if (pkt_done == true)
            break;
    } while (1);
    return pkt_done;
}

bool MppDecoder::Write_File(FILE *f)
{
    MppFrameFormat fmt = MPP_FMT_YUV420SP;
    RK_U8 *base = NULL;
    MppBuffer buffer = NULL;
    fmt = mpp_frame_get_fmt(dec_frame);
    buffer = mpp_frame_get_buffer(dec_frame);
    if (NULL == buffer)
        return false;
    base = (RK_U8 *)mpp_buffer_get_ptr(buffer);

    RK_U32 i;
    RK_U8 *base_y = base;                       // 亮度分量从base开始
    RK_U8 *base_c = base + hor_stride * ver_stride; // 颜色分量从base + h_stride * v_stride开始

    for (i = 0; i < height; i++, base_y = base_y + hor_stride)
    { // 写入亮度分量Y
        fwrite(base_y, 1, width, f);
    }
    for (i = 0; i < height / 2; i++, base_c += hor_stride)
    { // 写入颜色分量uv
        fwrite(base_c, 1, width, f);
    }
    return true;
}