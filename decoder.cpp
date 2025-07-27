// aarch64-linux-gnu-g++ -g decoder.cpp $(pkg-config --cflags --libs opencv4) -lavcodec -lavformat -lavutil -lavdevice -lavfilter -lrockchip_mpp -fopenmp -lrga -lpthread -fopenmp -L. -lFallDetector -O3 -o decoder

#include </usr/include/rockchip/rk_mpi.h>
#include </usr/include/rockchip/mpp_frame.h>
#include </usr/include/rockchip/rk_type.h>
#include </usr/include/rockchip/rk_venc_cmd.h>
extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}
#include </usr/include/rga/rga.h>
#include </usr/include/rga/im2d.hpp>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/time.h>
#include </usr/include/opencv4/opencv2/opencv.hpp>
#include <cstring>
#include "fallDetect.h"
#include <cmath>
using namespace cv;
using namespace std;

#define DEFAULT_NUM_SAMPLES 20
#define DEFAULT_MIN_MATCHES 2
#define DEFAULT_RADIUS 20
#define DEFAULT_RANDOM_SAMPLE 16
long arg_us = 0; // 平均时间
int ceshi_count = 0;
int Start_Flag = 0;
int rtsp = 0;
Mat FG_img; // vibe算法后得出的图像数据，将其放入到刘老师算法

#define QUEUE_SIZE 5 // 预定义队列容量

typedef struct
{
	MppFrame buffer[QUEUE_SIZE]; // 存储 MppFrame的数组
	uint32_t head;				 // 读取位置
	uint32_t tail;				 // 写入位置
} PointerQueue;
PointerQueue frame_queue;
// 初始化队列
void queue_init(PointerQueue *q)
{
	q->head = 0;
	q->tail = 0;
}

// 入队（非阻塞，队列满返回 false）
bool queue_push(PointerQueue *q, MppFrame ptr)
{
	uint32_t next_tail = (q->tail + 1) % QUEUE_SIZE;
	if (next_tail == q->head)
		return false;		  // 队列满
	q->buffer[q->tail] = ptr; // 存储指针
	q->tail = next_tail;	  // 更新尾指针
	return true;
}

// 出队（非阻塞，队列空返回 false）
bool queue_pop(PointerQueue *q, void **out_ptr)
{
	if (q->head == q->tail)
		return false;					  // 队列空
	*out_ptr = q->buffer[q->head];		  // 返回指针
	q->head = (q->head + 1) % QUEUE_SIZE; // 更新头指针
	return true;
}

pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
class ViBe
{

public:
	void Init(Mat First_Frame);

	void Run(Mat Frame);

	Mat GetFGModel();

	void DeleteSamples();

private:
	int num_sample = DEFAULT_NUM_SAMPLES;
	int min_match = DEFAULT_MIN_MATCHES;
	int radius = DEFAULT_RADIUS;
	int updata_probability = DEFAULT_RANDOM_SAMPLE;

	vector<Mat> samples;
	Mat FG_counts;
	Mat FG_model;
};

void ViBe::Init(Mat First_Frame)
{

	RNG rng;
	int random;
	int random_rows;
	int random_cols;
	int x_shift[9] = {-1, 0, 1, -1, 0, 1, -1, 0, 1};
	int y_shift[9] = {-1, 0, 1, -1, 0, 1, -1, 0, 1};

	FG_counts = Mat::zeros(First_Frame.rows, First_Frame.cols, CV_8UC1);

	FG_model = Mat::zeros(First_Frame.rows, First_Frame.cols, CV_8UC1);
#pragma omp parallel for num_threads(8)
	for (int k = 0; k < num_sample; k++)
	{

		Mat Temp = Mat::zeros(First_Frame.rows, First_Frame.cols, CV_8UC1);

		for (int i = 0; i < First_Frame.rows; i++)
		{
			for (int j = 0; j < First_Frame.cols; j++)
			{

				random = rng.uniform(0, 9);
				random_rows = i + y_shift[random];
				random = rng.uniform(0, 9);
				random_cols = j + x_shift[random];

				if (random_rows < 0)
					random_rows = 0;
				if (random_rows >= First_Frame.rows)
					random_rows = First_Frame.rows - 1;
				if (random_cols < 0)
					random_cols = 0;
				if (random_cols >= First_Frame.cols)
					random_cols = First_Frame.cols - 1;

				Temp.at<uchar>(i, j) = First_Frame.at<uchar>(random_rows, random_cols);
			}
		}
		samples.push_back(Temp);
	}
}

void ViBe::Run(Mat Frame)
{

	RNG rng;

	int distance = 0;
	int match_count = 0;
	int random = 0;
	int random_rows;
	int random_cols;
	int x_shift[9] = {-1, 0, 1, -1, 0, 1, -1, 0, 1};
	int y_shift[9] = {-1, 0, 1, -1, 0, 1, -1, 0, 1};
#pragma omp parallel for num_threads(12)
	for (int i = 0; i < Frame.rows; i++)
	{
		for (int j = 0; j < Frame.cols; j++)
		{
			for (int k = 0; k < num_sample; k++)
			{

				distance = abs(samples[k].at<uchar>(i, j) - Frame.at<uchar>(i, j));

				if (distance < radius)
					match_count++;
				if (match_count == min_match)
					break;
			}

			if (match_count >= min_match)
			{

				match_count = 0;

				FG_counts.at<uchar>(i, j) = 0;
				FG_model.at<uchar>(i, j) = 0;

				random = rng.uniform(0, updata_probability);

				if (random == 0)
				{

					random = rng.uniform(0, num_sample);
					samples[random].at<uchar>(i, j) = Frame.at<uchar>(i, j);

					random = rng.uniform(0, 9);
					random_rows = i + x_shift[random];
					random = rng.uniform(0, 9);
					random_cols = j + y_shift[random];

					if (random_rows < 0)
						random_rows = 0;
					if (random_rows >= Frame.rows)
						random_rows = Frame.rows - 1;
					if (random_cols < 0)
						random_cols = 0;
					if (random_cols >= Frame.cols)
						random_cols = Frame.cols - 1;

					random = rng.uniform(0, num_sample);
					samples[random].at<uchar>(random_rows, random_cols) = Frame.at<uchar>(i, j);
				}
			}

			else
			{
				match_count = 0;

				FG_model.at<uchar>(i, j) = 255;
				FG_counts.at<uchar>(i, j)++;

				if (FG_counts.at<uchar>(i, j) > 1)
				{
					random = rng.uniform(0, num_sample);
					samples[random].at<uchar>(i, j) = Frame.at<uchar>(i, j);
				}
			}
		}
	}
}

Mat ViBe::GetFGModel()
{

	return FG_model;
}

void ViBe::DeleteSamples()
{
	vector<Mat>().swap(samples);
}
void *liulaoshi(void *arg)
{
	FILE* pw_log = fopen("pw_log", "w");
	if (pw_log == NULL) {
        perror("Unable to open file");
        pthread_exit(NULL);  // 线程结束，返回空指针
    }
	time_t current_time;
    struct tm* time_info;
    char time_string[100];


	//==================步骤1、初始化参数（开始）=====================
	//------------------1、按实际情况填写以下9个参数-----------------------
	// 本例子以1920*1080分辨率、25帧每秒的视频为例，请按实际修改以下三个参数
	const int PRESSROWS = 1080 / 3; // 必须是除以3，若不能整除则取大（ceil）
	const int PRESSCOLUMNS = 1920 / 3;
	const int FRAMERATE = 25;	  // 帧率
	const int RECORDDURATION = 2; // 记录2秒	(足矣，一般不改)

	// 需计算参数：MePPi（米每像素），请按实际测量以下六个参数，具体方法看下文
	double x1 = 112 / 3;
	double x2 = 304 / 3;
	double y1 = 4;
	double x3 = 576 / 3;
	double x4 = 896 / 3;
	double y2 = 4;
	cout << "x1 = " << x1 << " x2 = " << x2 << " x3 = " << x3 << " x4 = " << x4 << endl;
	//---------------------------------------------------------------------
	const int RECORDFRAMES = FRAMERATE * RECORDDURATION;
	const long SIZE1 = PRESSROWS * PRESSCOLUMNS * RECORDFRAMES;
	const long SIZE2 = PRESSROWS * PRESSCOLUMNS;
	static int Record4DFrames[SIZE1];
	static int Record4DFramesNo2Filter[SIZE1];
	int LastPositionOfRecord4DFrames = RECORDFRAMES;
	static int SumOfRecord4DFrames[SIZE2];
	static int LastDFrame1[SIZE2];
	static int LastDFrame2[SIZE2];
	static int AdjustMaskPress[SIZE2];
	static int NumOfRecord4DFrames[SIZE2];
	double FrameRate = FRAMERATE;
	double MePPi[2];				 // 米每像素
	double TLTh = 1;				 // 抛物轨迹长度阈值（米）
	static int TrackR[RECORDFRAMES]; // 抛物轨迹（行号）
	static int TrackC[RECORDFRAMES]; // 抛物轨迹（列号）
	int IsDroped = 0;				 // 1:有抛物，0：没有抛物

	memset(&Record4DFrames[0], 0, SIZE1 * sizeof(int));
	memset(&Record4DFramesNo2Filter[0], 0, SIZE1 * sizeof(int));
	memset(&SumOfRecord4DFrames[0], 0, SIZE2 * sizeof(int));
	memset(&LastDFrame1[0], 0, SIZE2 * sizeof(int));
	memset(&LastDFrame2[0], 0, SIZE2 * sizeof(int));
	memset(&AdjustMaskPress[0], 0, SIZE2 * sizeof(int));
	memset(&NumOfRecord4DFrames[0], 0, SIZE2 * sizeof(int));
	memset(&TrackR[0], 0, RECORDFRAMES * sizeof(int));
	memset(&TrackC[0], 0, RECORDFRAMES * sizeof(int));
	// MePPi确定方法：
	// 1、取原图上方，列方向任意两点（距离大点），设其行号除以3为x1和x2（x2须大于x1，压缩图像行号），测量这两点的实际距离y1（米），则y1 = [(a*x1 + b) + (a*x2 + b)] / 2 * | x1 - x2 |
	// 2、再取原图下方两点，得x3、x4和y2，得另外一条方程y2 = [(a*x3 + b) + (a*x4 + b)] / 2 * | x3 - x4 | ，求解方程组得a和b，即MePPi[a,b]
	// 举例：测得建筑物的最顶一层位于原图的第46到120行，一层楼高度一般是3米
	// x1 = 46 / 3;
	// x2 = 120 / 3;
	// y1 = 3;
	// 再测得建筑物下面某层位于原图的第420到548行，该层高度也是3米
	// x3 = 420 / 3;
	// x4 = 548 / 3;
	// y2 = 3;
	// 解方程组得MePPi
	MePPi[0] = 2 * (y1 / (x2 - x1) - y2 / (x4 - x3)) / (x1 + x2 - x3 - x4);
	MePPi[1] = y1 / (x2 - x1) - MePPi[0] * (x1 + x2) / 2;
	//==================步骤1、初始化参数（结束）=====================

	//=================步骤2、循环计算各帧（开始）====================

	int NeedIdentify = 1;					   // 是否进行抛物判断，1：是，0：否
	int IdentifyTimer = 0;					   // 用于启动抛物判断的时间计算（实际上是帧计算）
	int IdentifyInterval = int(FRAMERATE / 2); // 启动抛物判断的帧间隔
	int DisableNextIdentify = 0;			   // 屏蔽下次的抛物判断，1：屏蔽，0：不屏蔽

	sleep(5);
	u_char *MaskNew = FG_img.data; // MaskNew 指向 FG_img 的数据区域
	int width = FG_img.cols;	   // 图像的宽度
	int height = FG_img.rows;	   // 图像的高度

	while (1)
	{
		pthread_mutex_lock(&mut);
		pthread_cond_wait(&cond, &mut);
		struct timeval start, end;
		gettimeofday(&start, nullptr);
		
		for (int ii = 1; ii < 1080 - 1; ii++)
		{
			for (int jj = 1; jj < 1920 - 1; jj++)
			{
				if (MaskNew[ii * width + jj] == 1)
				{
					if ((MaskNew[(ii - 1) * width + (jj - 1)] + MaskNew[(ii - 1) * width + jj] + MaskNew[(ii - 1) * width + (jj + 1)] +
						 MaskNew[ii * width + (jj - 1)] + MaskNew[ii * width + (jj + 1)] + MaskNew[(ii + 1) * width + (jj - 1)] +
						 MaskNew[(ii + 1) * width + jj] + MaskNew[(ii + 1) * width + (jj + 1)]) < 4)
					{
						// AdjustMask[ii][jj]=0;
						// ForeGroundSum[ii][jj]=0;
					}
					else
					{
						AdjustMaskPress[static_cast<int>(floor(double(ii) / 3) + floor(double(jj) / 3) * 1080)] = 255;
						// 修改：加了个static_cast<int>
						// 不然编译器报错error: invalid types 'int [230400][double]' for array subscript
						// AdjustMaskPress [ floor(double(ii)/3) + floor(double(jj)/3) * 1080]=255;
						//                 ^
					}
				}
			}
		}

		//=================步骤2.1、读取并处理一帧（结束）====================

		//=================步骤2.2、二次处理及判断（开始）====================
		// 由于fallDetect函数做抛物判断很耗时间，而实际需求并不需要每帧都做抛物判断
		// 因此，本步骤除fallDetect函数外的其它代码是优化代码，不是必须的
		// 优化内容：每半秒进行一次抛物判断，并且若判断有抛物，则不进行下次抛物判断
		IdentifyTimer = IdentifyTimer + 1;
		if (IdentifyTimer >= IdentifyInterval)
		{ // 每半秒启动一次
			IdentifyTimer = 0;
			if (DisableNextIdentify == 0)
			{ // 如果没有屏蔽，则进行抛物判断
				NeedIdentify = 1;
			}
			else
			{ // 如果屏蔽了抛物，即上次判断有抛物，则本次不进行抛物判断
				DisableNextIdentify = 0;
				NeedIdentify = 0;
			}
		}
		else
		{
			NeedIdentify = 0;
		}
		fallDetect(Record4DFrames, Record4DFramesNo2Filter, &LastPositionOfRecord4DFrames, SumOfRecord4DFrames, LastDFrame1, LastDFrame2,
				   AdjustMaskPress, NumOfRecord4DFrames, MePPi, FrameRate, NeedIdentify, TLTh, TrackR, TrackC, &IsDroped);
		if (IsDroped == 1)
		{ // 如果判断有抛物，则屏蔽下次的抛物判断
			cout << "有抛物" << endl;
			DisableNextIdentify = 1;
			 // 获取系统当前时间
			 time(&current_time);
    
			 // 将时间转换为本地时间
			 time_info = localtime(&current_time);
		 
			 // 格式化时间为字符串
			 strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", time_info);
		 
			 // 将时间写入文件
			 fprintf(pw_log, "Current time: %s\n", time_string);
		}
		//=================步骤2.2、二次处理及判断（结束）====================

		gettimeofday(&end, nullptr);
		ceshi_count++;
		long us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec); // 计算时间间隔，单位为微秒
		//av_log(NULL, AV_LOG_ERROR, "liulaoshi file time %ld us,count = %d\n", us, ceshi_count);
		pthread_mutex_unlock(&mut);
	}
	//=================步骤2、循环计算各帧（结束）====================

	return 0;
}
void *vibe(void *arg)
{
	ViBe Vibe_Model;
	unsigned char *vibe_buf;
	int frame_count = 0;
	vibe_buf = (unsigned char *)malloc(1920 * 1080 * 3 / 2); // buf用来存储解码后的图像数据，解码后是YUV420SP，所以需要开辟空间为1920*1080*3/2
	memset(vibe_buf, 128, 1920 * 1080 * 3 / 2);
	while (1)
	{
		MppFrame dec_frame;
		if (queue_pop(&frame_queue, &dec_frame))
		{
			struct timeval start, end;
			gettimeofday(&start, nullptr);
			//========rga的copy功能
			MppBuffer mpp_buffer = mpp_frame_get_buffer(dec_frame);										   // 获取frame的buf
			int buf_fd = mpp_buffer_get_fd(mpp_buffer);													   // 获取frame的fd，使用fd的话，驱动无需拷贝，可以直接读写fd对应区域的数据，通常fd用的是cma buffer，物理地址是连续的，RGA硬件操作效率会更高
			rga_buffer_t src = wrapbuffer_fd(buf_fd, 1920, 1080, RK_FORMAT_YCbCr_420_SP, 1984, 1088);			   // 获取输入rga_buffer_t
			rga_buffer_t dst = wrapbuffer_virtualaddr(vibe_buf, 1920, 1080, RK_FORMAT_YCbCr_420_SP); // 获取输出rga_buffer_t
			imcopy(src, dst);

			Mat gray(1080, 1920, CV_8UC1, vibe_buf);

			// bool result = cv::imwrite("output_image.png", gray);
			// sleep(10000);
			if (Start_Flag == 0)
			{
				Vibe_Model.Init(gray);
				Start_Flag += 1;
			}
			else
			{
				pthread_mutex_lock(&mut);
				Vibe_Model.Run(gray);
				FG_img = Vibe_Model.GetFGModel();
				pthread_cond_signal(&cond);
				pthread_mutex_unlock(&mut);
			}
			mpp_frame_deinit(&dec_frame);

			gettimeofday(&end, nullptr);
			long us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec); // 计算时间间隔，单位为微秒
			//av_log(NULL, AV_LOG_ERROR, "vibe file time %d\n", us);
		}
		else
		{
			usleep(100); // 队列空时短暂休眠
		}
	}
}
char *get_time_string()
{
	static char buffer[20];
	time_t rawtime;
	struct tm *timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(buffer, sizeof(buffer), "%H:%M:%S", timeinfo);
	return buffer;
}

// 2.2 生成时间文本位图（RGBA8888）
void render_text_to_buffer(MppBuffer buffer, int width, int height)
{
	char *ptr = (char *)mpp_buffer_get_ptr(buffer); // 获取缓冲区地址
	const char *text = get_time_string();

	// 清空为透明背景
	memset(ptr, 0x00, width * height * 4);

	// 简单位图渲染（示例：白色文字）
	for (int y = 0; y < 32; y++)
	{ // 文字高度 32 像素
		for (int x = 0; x < strlen(text) * 16; x++)
		{ // 每个字符宽 16 像素
			int offset = (y * width + x) * 4;
			ptr[offset + 0] = 0xFF; // R
			ptr[offset + 1] = 0xFF; // G
			ptr[offset + 2] = 0xFF; // B
			ptr[offset + 3] = 0xFF; // A（不透明）
		}
	}
}

int main(int argc, char **argv)
{
	int ret = 0;
	FILE *fp_output = fopen(argv[1], "w+b"); // 打开输出文件
	if (NULL == fp_output)
	{
		av_log(NULL, AV_LOG_ERROR, "open output file failed\n");
	}
	/***********************rtsp*************************** */
	avformat_network_init();
	AVFormatContext *rtsp_ctx = NULL;
	rtsp_ctx = avformat_alloc_context();
	const char *STREAM_URL = "rtsp://192.168.137.234:8554/stream"; // rtsp推流地址
	if (avformat_alloc_output_context2(&rtsp_ctx, NULL, "rtsp", STREAM_URL) < 0)
	{
		fprintf(stderr, "Could not create output context\n");
		return -1;
	}
	AVStream *videoStream = avformat_new_stream(rtsp_ctx, NULL); // 创建视频流
	if (videoStream == NULL)
	{
		fprintf(stderr, "Could not create new stream\n");
		return -1;
	}
	videoStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO; // 视频流
	videoStream->codecpar->codec_id = AV_CODEC_ID_H264;		// 编码格式
	videoStream->codecpar->width = 1920;					// 视频宽
	videoStream->codecpar->height = 1080;					// 视频高
	videoStream->codecpar->format = AV_PIX_FMT_NV12;		// 像素格式
	videoStream->time_base = (AVRational){1, 25};			// 时间基
	videoStream->r_frame_rate = (AVRational){25, 1};		// 帧率
	videoStream->avg_frame_rate = (AVRational){25, 1};		// 平均帧率
	videoStream->codecpar->bit_rate = 1920 * 1080 * 3;		// 码率
	// 写入流头
	if (avformat_write_header(rtsp_ctx, NULL) < 0)
	{
		fprintf(stderr, "Error occurred when opening output URL\n");
		return -1;
	}
	/**************************rtsp************************* */
	/*********************************ffmpeg****************************************************/
	// 下面是打开摄像头的操作
	avdevice_register_all();
	const AVInputFormat *inFmt = av_find_input_format("v4l2"); // 查询并返回解复用器
	if (inFmt == NULL)
	{
		av_log(NULL, AV_LOG_ERROR, "find_input_v4l2 fial");
		return -1;
	}

	AVDictionary *options = NULL;
	av_dict_set(&options, "input_format", "h264", 0);					  // 设置输入格式
	av_dict_set(&options, "video_size", "1920x1080", 0);				  // 设置分辨率   分辨率根据需要修改
	av_dict_set(&options, "framerate", "25", 0);						  // 设置帧率       帧率也是根据需要修改
	AVFormatContext *inFmtCtx = avformat_alloc_context();				  // 创建一个上下文
	ret = avformat_open_input(&inFmtCtx, "/dev/video0", inFmt, &options); // 打开输入流并绑定上下文，并设置参数  /dev/video0就是摄像头的结点，根据自己的结点改
	if (ret != 0)
	{
		av_log(NULL, AV_LOG_ERROR, "open_input fial");
		return -1;
	}

	ret = avformat_find_stream_info(inFmtCtx, NULL); // 通过上下文寻找流，并填充上下文相关信息
	if (ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "find_stream fial");
		return -1;
	}

	int videoIndex = av_find_best_stream(inFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0); // 通过上下文返回流的索引
	if (videoIndex < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "find_best_stream fial");
		return -1;
	}
	AVPacket *av_packet = av_packet_alloc(); // 创建avpacket
	if (!av_packet)
	{
		av_log(NULL, AV_LOG_ERROR, "av_packet_alloc fail\n");
	}
	/*********************************ffmpeg****************************************************/

	MppBufferGroup frm_grp = NULL; // 创建帧组
	RK_U64 frame_count = 0;		   // 帧计数
	RK_U32 err_info = 0;		   // 错误信息
	/*********************************mpp解码****************************************************/
	MppCtx dec_ctx = NULL;		 // 解码上下文
	MppApi *dec_mpi = NULL;		 // 解码接口
	MppPacket dec_packet = NULL; // 解码包

	MpiCmd mpi_cmd = MPP_CMD_BASE; // 解码cmd设置
	MppParam param = NULL;		   // 解码参数
	RK_U32 need_split = 0;		   // 解码模式

	RK_U32 width = inFmtCtx->streams[videoIndex]->codecpar->width;	 // 视频宽度
	RK_U32 height = inFmtCtx->streams[videoIndex]->codecpar->height; // 视频高度
	MppCodingType type = MPP_VIDEO_CodingAVC;						 // #视频格式,MPP_VIDEO_CodingAVC就是h264

	RK_U32 pkt_done = 0;				  // 解码包完成标志
	ret = mpp_create(&dec_ctx, &dec_mpi); // 创建解码上下文和解码接口
	if (MPP_OK != ret)
	{
		av_log(NULL, AV_LOG_ERROR, "mpp_create failed\n");
	}
	mpi_cmd = MPP_DEC_SET_PARSER_SPLIT_MODE;		 // 设置解码模式为自动拼接，原因：简单
	param = &need_split;							 // 设置解码模式
	ret = dec_mpi->control(dec_ctx, mpi_cmd, param); // 设置解码模式
	if (MPP_OK != ret)
	{
		av_log(NULL, AV_LOG_ERROR, "mpi->control failed\n");
	}

	mpi_cmd = MPP_SET_INPUT_BLOCK;					 // 设置解码输入为阻塞模式
	param = &need_split;							 // 设置解码模式
	ret = dec_mpi->control(dec_ctx, mpi_cmd, param); // 设置解码输入为阻塞模式
	if (MPP_OK != ret)
	{
		av_log(NULL, AV_LOG_ERROR, "mpi->control failed\n");
	}
		mpi_cmd = MPP_SET_INPUT_BLOCK;					 // 设置解码输入为阻塞模式
	param = &need_split;							 // 设置解码模式
	ret = dec_mpi->control(dec_ctx, mpi_cmd, param); // 设置解码输入为阻塞模式
	if (MPP_OK != ret)
	{
		av_log(NULL, AV_LOG_ERROR, "mpi->control failed\n");
	}

	ret = mpp_init(dec_ctx, MPP_CTX_DEC, type); // 初始化解码上下文
	if (MPP_OK != ret)
	{
		av_log(NULL, AV_LOG_ERROR, "mpp_init failed\n");
	}
	/*********************************mpp解码****************************************************/

	/*********************************mpp编码***************************************************/
	MppCtx enc_ctx = NULL;				  // 编码上下文
	MppApi *enc_mpi = NULL;				  // 编码接口
	MppEncCfg enc_cfg = NULL;			  // 编码配置
	MppPacket enc_packet = NULL;		  // 编码包
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

	mpp_enc_cfg_set_s32(enc_cfg, "prep:width", width);				   // 设置编码宽度
	mpp_enc_cfg_set_s32(enc_cfg, "prep:height", height);			   // 设置编码高度
	mpp_enc_cfg_set_s32(enc_cfg, "prep:hor_stride", 1984);			   // 设置编码水平步幅
	mpp_enc_cfg_set_s32(enc_cfg, "prep:ver_stride", 1088);			   // 设置编码垂直步幅
	mpp_enc_cfg_set_s32(enc_cfg, "prep:format", MPP_FMT_YUV420SP);	   // 设置编码格式
	mpp_enc_cfg_set_s32(enc_cfg, "rc:mode", MPP_ENC_RC_MODE_CBR);	   // 设置码率模式  CBR:恒定码率  VBR:可变码率
	mpp_enc_cfg_set_s32(enc_cfg, "rc:fps_in_num", 25);				   // 设置输入帧率
	mpp_enc_cfg_set_s32(enc_cfg, "rc:fps_in_denorm", 1);			   // 设置输入帧率分母
	mpp_enc_cfg_set_s32(enc_cfg, "rc:fps_out_num", 25);				   // 设置输出帧率
	mpp_enc_cfg_set_s32(enc_cfg, "rc:fps_out_denorm", 1);			   // 设置输出帧率分母
	mpp_enc_cfg_set_s32(enc_cfg, "rc:gop", 15);						   // 设置gop
	mpp_enc_cfg_set_s32(enc_cfg, "rc:bps_target", width * height * 2); // 设置码率目标
	mpp_enc_cfg_set_s32(enc_cfg, "rc:bps_max", width * height * 3);	   // 设置码率最大值
	mpp_enc_cfg_set_s32(enc_cfg, "rc:bps_min", width * height * 1);	   // 设置码率最小值
	mpp_enc_cfg_set_s32(enc_cfg, "codec:type", MPP_VIDEO_CodingAVC);   // 设置编码类型  h264

	ret = enc_mpi->control(enc_ctx, MPP_ENC_SET_CFG, enc_cfg); // 设置编码配置
	if (ret)
	{

		return ret;
	}
	/*********************************mpp编码****************************************************/
	void *time = malloc(1984 * 1088 * 3 / 2);
	memset(time, 1984 * 1088 * 3 / 2, 0);
	queue_init(&frame_queue);
	pthread_t liu_pthread;
	pthread_create(&liu_pthread, NULL, liulaoshi, NULL); // 刘老师算法的线程
	pthread_t vibe_pthread;
	pthread_create(&vibe_pthread, NULL, vibe, NULL); // 师兄算法的线程

	/***********************字幕************************************* */
	MppBufferGroup time_bufgroup = NULL;
	ret = mpp_buffer_group_get_internal(&time_bufgroup, MPP_BUFFER_TYPE_DRM); // 创建一个buffer池
	if (ret)
	{
		av_log(NULL, AV_LOG_ERROR, "mpp_buffer_group_get_internal failed: %d\n", ret);
	}
	MppBuffer time_buffer = NULL;
	ret = mpp_buffer_get(time_bufgroup, &time_buffer, 1920 * 1080 * 4);
	if (ret != MPP_OK)
	{
		av_log(NULL, AV_LOG_ERROR, "mpp_buffer_get fail");
	}
	void *time_buf = malloc(1920 * 1080 * 3);
	/***********************字幕************************************* */
	while (1)
	{
		if (av_read_frame(inFmtCtx, av_packet) >= 0) // 读取摄像头的数据
		{
		
			if (av_packet->stream_index == videoIndex) // 判断读取到的packet是否为视频流数据
			{
				
				ret = mpp_packet_init(&dec_packet, av_packet->data, av_packet->size); // 将ffmpeg的packet变成mpp的pakect
				if (ret)
				{
					av_log(NULL, AV_LOG_ERROR, "mpp_packet_init failed\n");
					goto end;
				}
				mpp_packet_set_pts(dec_packet, av_packet->pts); // 设置时间戳
				do
				{
					
					pkt_done = 0;  // 该变量用来表示packet是否被送入到mpp的解码器中
					if (!pkt_done) // 1则表示已送入，0表示送入失败
					{
						ret = dec_mpi->decode_put_packet(dec_ctx, dec_packet); // 将pakcet送入到mpp解码器中
						if (MPP_OK == ret)									   // MPP_OK表示put成功
							pkt_done = 1;
						else if (ret == MPP_ERR_BUFFER_FULL) // MPP_ERR_BUFFER_FULL表示mpp解码器已满，则需先get_frame
						{
							av_log(NULL, AV_LOG_WARNING, "Buffer full, waiting...\n");
							pkt_done = 1;
						}
						else // 否则为失败，将再一次尝试
						{
							av_log(NULL, AV_LOG_ERROR, "decode_put_packet failed: %d\n", ret);
							break;
						}
					}

					do // 此时packet已经送到mpp的解码器
					{
						//RK_S32 get_frm = 0; // dec_freame为空则会为0而跳出while循环，为1则继续尝试get_frame

						MppFrame dec_frame = NULL;
						ret = dec_mpi->decode_get_frame(dec_ctx, &dec_frame); // put_packet后就get_frame得出解码的数据
						if (MPP_ERR_TIMEOUT == ret)							  // 超时则等待一下
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
							
							frame_count++;							  // 用来跳过开头的90帧，开头的一两秒视频不衔接，且用来当做视频的时间戳
							if (mpp_frame_get_info_change(dec_frame)) // 检测frame的信息是否变化（分辨率，帧率等），
							{
								RK_U32 width = mpp_frame_get_width(dec_frame);
								RK_U32 height = mpp_frame_get_height(dec_frame);
								RK_U32 hor_stride = mpp_frame_get_hor_stride(dec_frame);
								RK_U32 ver_stride = mpp_frame_get_ver_stride(dec_frame);
								RK_U32 buf_size = mpp_frame_get_buf_size(dec_frame);

								ret = mpp_buffer_group_get_internal(&frm_grp, MPP_BUFFER_TYPE_DRM); // 创建一个buffer池
								if (ret)
								{
									av_log(NULL, AV_LOG_ERROR, "mpp_buffer_group_get_internal failed: %d\n", ret);
									break;
								}
								dec_mpi->control(dec_ctx, MPP_DEC_SET_EXT_BUF_GROUP, frm_grp); // 设置buffer池到mpp解码器
								ret = mpp_buffer_group_limit_config(frm_grp, buf_size, 60);	   // 此函数用来限制buffer的数量
								if (ret)
								{
									av_log(NULL, AV_LOG_ERROR, "mpp_buffer_group_limit_config failed: %d\n", ret);
									break;
								}
								dec_mpi->control(dec_ctx, MPP_DEC_SET_INFO_CHANGE_READY, NULL); // 表示准备好了可以开始解码
							}
							else if (frame_count > 20) // 跳过前面90帧
							{

								err_info = mpp_frame_get_errinfo(dec_frame); // 获取帧错误信息

								if (!err_info) // 没有错则进行以下步骤
								{
									if (!queue_push(&frame_queue, dec_frame))
									{
										// 队列满时丢弃最旧帧
										MppFrame old_frame;
										if (queue_pop(&frame_queue, &old_frame))
										{
											mpp_frame_deinit(&old_frame); // 释放旧帧资源
										}
										queue_push(&frame_queue, dec_frame); // 重新尝试入队
									}

									MppBuffer dec_frame_buffer = NULL;
									dec_frame_buffer = mpp_frame_get_buffer(dec_frame);
									int dec_frame_fd = mpp_buffer_get_fd(dec_frame_buffer);

									rga_buffer_t src = wrapbuffer_fd(dec_frame_fd, 1920, 1080, RK_FORMAT_YCbCr_420_SP, 1984, 1088); // 获取输入rga_buffer_t
									rga_buffer_t dst = wrapbuffer_virtualaddr(time_buf, 1920, 1080, RK_FORMAT_RGB_888);				// 获取输出rga_buffer_t
									imcvtcolor(src, dst, RK_FORMAT_YCbCr_420_SP, RK_FORMAT_RGB_888);								// 耗时7ms
									cv::Mat time_date(1080, 1920, CV_8UC3, time_buf);

									// 获取当前本地时间
									auto now = std::chrono::system_clock::now();
									std::time_t now_time = std::chrono::system_clock::to_time_t(now);

									// 将时间格式化为字符串（例如 "2025-03-29 12:34:56"）
									std::stringstream time_stream;
									time_stream << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S");
									std::string current_time = time_stream.str();

									// 要添加的字幕内容是当前时间
									std::string text = current_time;

									// 字体、位置、字体大小、颜色等设置
									cv::Point text_position(50, 50);		  // 字幕起始位置
									double font_scale = 1.5;				  // 字体大小
									int thickness = 2;						  // 字体厚度
									cv::Scalar color(255, 255, 255);		  // 白色字体（BGR格式）
									int font_face = cv::FONT_HERSHEY_SIMPLEX; // 字体类型

									// 在图像上添加字幕
									cv::putText(time_date, text, text_position, font_face, font_scale, color, thickness, cv::LINE_AA);

									int dec_fd = mpp_buffer_get_fd(time_buffer);
									src = wrapbuffer_virtualaddr(time_buf, 1920, 1080, RK_FORMAT_RGB_888);		 // 获取输出rga_buffer_t
									dst = wrapbuffer_fd(dec_fd, 1920, 1080, RK_FORMAT_YCbCr_420_SP, 1984, 1088); // 获取输入rga_buffer_t
									imcvtcolor(src, dst, RK_FORMAT_RGB_888, RK_FORMAT_YCbCr_420_SP);			 // 耗时7ms
									MppFrame frame = NULL;
									ret = mpp_frame_init(&frame);
									if (ret)
									{
										av_log(NULL, AV_LOG_ERROR, "frame init %d\n", ret);
										return -1;
									}

									mpp_frame_set_width(frame, 1920);
									mpp_frame_set_height(frame, 1080);
									mpp_frame_set_hor_stride(frame, 1984);
									mpp_frame_set_ver_stride(frame, 1088);
									mpp_frame_set_fmt(frame, MPP_FMT_YUV420SP);
									mpp_frame_set_buffer(frame, time_buffer);
									ret = enc_mpi->encode_put_frame(enc_ctx, frame);
									if (ret)
									{
										av_log(NULL, AV_LOG_ERROR, "encode_put_frame failed: %d\n", ret);
										break;
									}

									ret = enc_mpi->encode_get_packet(enc_ctx, &enc_packet); // 得到编码后的结果
									if (ret)
									{
										av_log(NULL, AV_LOG_ERROR, "encode_get_packet failed: %d\n", ret);
										break;
									}

									if (enc_packet) // 编码结果是否为空
									{

										void *data = mpp_packet_get_pos(enc_packet);	// 获取编码后的地址
										size_t len = mpp_packet_get_length(enc_packet); // 获取编码后的图像长度
										fwrite(data, 1, len, fp_output);

										AVPacket *pkt = av_packet_alloc();
										if (!pkt)
										{
											av_log(NULL, AV_LOG_ERROR, "Could not allocate AVPacket\n");
											return -1;
										}

										pkt->data = (uint8_t *)data;
										pkt->size = len;
										pkt->pts = av_rescale_q_rnd(frame_count++,
																	(AVRational){1, 50},
																	videoStream->time_base,
																	AV_ROUND_NEAR_INF);
										pkt->dts = pkt->pts;
										pkt->flags = 0;
										pkt->stream_index = 0;

										ret = av_interleaved_write_frame(rtsp_ctx, pkt);
										if (ret < 0)
										{
											av_log(NULL, AV_LOG_ERROR, "Error writing packet\n");
										}

										mpp_packet_deinit(&enc_packet); // 反初始化编码包
									}
								}
							}
						}
						break;
					} while (1);

					if (pkt_done) // 等于1表示解码成功
						break;	  // 退出while继续获取下一帧

					usleep(1000); // 否则延时一会继续尝试put解码
				} while (1);
			}
			av_packet_unref(av_packet); // 释放ffmpeg的pakcet中的资源，但是不会销毁 AVPacket 结构本身。
		}
		av_packet_unref(av_packet);

	}
end:
	if (frm_grp)
	{
		mpp_buffer_group_put(frm_grp);
	}

	if (dec_ctx)
	{
		mpp_destroy(dec_ctx);
	}

	if (enc_ctx)
	{
		mpp_destroy(enc_ctx);
	}

	if (av_packet)
	{
		av_packet_free(&av_packet);
	}

	if (fp_output)
	{
		fclose(fp_output);
	}

	return ret;
}