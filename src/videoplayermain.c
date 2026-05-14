#include<stdio.h>
#include<libavformat/avformat.h>
#include<libavcodec/avcodec.h>
#include<libswscale/swscale.h>
#include<SDL2/SDL.h>
#include<libswresample/swresample.h>

//视频显示的窗口大小
//窗口的宽
#define window_width 640
//窗口的长
#define window_high 360

//音频缓冲区结构体
static struct 
{
    uint8_t *data;
    //设置缓冲区的大小
    int size;
    //说明进行到了那一步
    int pos;    
    //设置一个寻找音频ID的变量
    SDL_AudioDeviceID device_id;
    //设置一个互斥锁，防止两个线程同时运行
    SDL_mutex *mutex;
    //设置一个等待机制
    SDL_cond *cond;
    int waiting;

}
//初始化前面这个结构体内的所有变量
audio_buf ={0};

//audio_callback:回调函数
void audio_callback(void *userdata ,Uint8 *stream,int len)
{
    //暂时没有用到这个参数，先忽略
    (void)userdata;
    //重置缓冲区
    SDL_memset(stream, 0 ,len);
    //将互斥锁使用
    SDL_LockMutex(audio_buf.mutex );
    //设置变量还需要存放的字节
    int remaining = len;
    //设置变量存放每次的字节数
    int copy_len = 0;
    //设置一个空指针存放上次输入的位置，从而保证下次输入的时候直接从上次的位置开始
    uint8_t *stream_pos = stream;
    // 
    while (remaining > 0) 
    {
        //当输入的字节数为0或者现在已有的字节数大于放置字节的容器
        if (audio_buf.data == NULL || audio_buf.pos >= audio_buf.size)
        {
            //让回调函数处于等待状态
            audio_buf.waiting = 1;
            //打开互斥锁，让主线程可以进入查看，睡眠100ms
            SDL_CondWaitTimeout(audio_buf.cond, audio_buf.mutex, 100);
            //如果超时后还是没有填充，就直接退出
            if (audio_buf.data == NULL || audio_buf.pos >= audio_buf.size)
            {
                break;
            }
            continue;   
        }
    //设置一个变量让他等于存放容器的数量减去已经存放的数据就等于还能存放的数据
    int available = audio_buf.size  - audio_buf.pos ;
    //每次存放是数量是缓存区剩余的数量和SDL还需要的数量中较小的呢个
    copy_len = (remaining < available) ? remaining : available;
    //从音频缓冲区复制到输出的缓冲区
    SDL_memcpy(stream_pos, audio_buf.data + audio_buf.pos ,copy_len);
    
    //更新音频输出的位置
    audio_buf.pos +=copy_len;
    //更新新的还需要的字节
    remaining -= copy_len;
    //跟新新的输出位置
    stream_pos += copy_len;
    
    //检查音频缓冲区的数据是否已经全部播放完了
    if(audio_buf.pos  >= audio_buf.size)
    {   
        //释放内存
        av_freep(&audio_buf.data);
        //初始化容器
        audio_buf.size = 0;
        //初始化
        audio_buf.pos = 0;
        //初始化
        audio_buf.waiting = 0;
        //告诉主线程可以继续提供数据
        SDL_CondSignal(audio_buf.cond);
    }

    }
    //解开互斥锁
    SDL_UnlockMutex(audio_buf.mutex);
}

int main(int argc, char *argv[]){
    if(argc < 2){
        printf("缺少输入内容，需要输入./myplayer和文件的地址名称\n");
        return -1;
    }

    //1.初始化ffmpeg的网络模块
    avformat_network_init();

    //2.打开视频
    AVFormatContext*ctx = NULL;
    if(avformat_open_input(&ctx ,argv[1],NULL,NULL)<0){
        printf("打开文件失败\n");
        return -1;
    }

    //3.查找视频文件中的所以流

    avformat_find_stream_info(ctx ,NULL);

    //4.找到视频流后进行索引
    int video_stream = -1;
    int audio_stream = -1;
    for (int  i = 0; i < ctx->nb_streams; i++)
    {
        if (ctx-> streams[i]->codecpar ->codec_type == AVMEDIA_TYPE_VIDEO)
        //codecpar:编码的参数，cpdec_type：流的类型
        {
            video_stream = i;
           
        }
        else if(ctx-> streams[i]->codecpar ->codec_type == AVMEDIA_TYPE_AUDIO)
        //codecpar:编码的参数，cpdec_type：流的类型
        {
            audio_stream = i;
           
        } 
    }
    if (video_stream == -1)
    //0是未知，1是视频，2是音频，
    {
        printf("没有找到视频流\n");
        return -1;
    }
    if (audio_stream == -1)
    {
        printf("没有找到音频流\n");
       
    }
    
    //获取时间基
    AVRational time_base = ctx ->streams[video_stream]->time_base;
    //获取平均帧率
    AVRational frame_rate = ctx->streams[video_stream]->avg_frame_rate;
    //当平均帧率无法使用时切换成真实帧率
    if (frame_rate.num == 0 || frame_rate.den == 0)
    {
        frame_rate = ctx->streams[video_stream]->r_frame_rate;
    }
    //计算每帧的间隔时间
    //av_q2d是把他转换成浮点数，av_inv_q取倒数
    double frame_delay = av_q2d(av_inv_q(frame_rate));

    printf("视频帧率：%.2f fps,每秒延迟： %.3f 毫秒\n",
            av_q2d(frame_rate), frame_delay * 1000);
 
    //初始化音频同步的两个变量
    //记录上一帧的时间戳
    double last_pts = 0;
    //维护一个视频播放的绝对时间轴
    double video_clock = 0;


    //5.获得解码器
    AVCodecParameters *codec_par = ctx ->streams[video_stream]->codecpar;
    //把存入的流放到ctx中，从而获取这个些流的信息，方便告诉编码器
    AVCodec *codec = avcodec_find_decoder(codec_par->codec_id);
    //寻找合适的编码器
        if (!codec)
    {
        printf("找不到解码器\n");
        return-1;
    }

    AVCodecContext *codec_cxt =avcodec_alloc_context3(codec);
    //空白一个解码器让流的信息有地方储存
        if (!codec_cxt)
    {
        printf("分配视频上下文失败\n");
        return-1;
    }
    avcodec_parameters_to_context(codec_cxt,codec_par);
    //把之前的流的信息都储存到编码器中
    if (avcodec_open2(codec_cxt, codec, NULL) < 0)
     {
    printf("打开视频解码器失败\n");
    return -1;
     }

    avcodec_open2(codec_cxt,codec ,NULL);
    //打开编码器

    // 初始化音频解码器和采样
    AVCodecContext *audio_ctx = NULL;
    SwrContext *swr_ctx = NULL;
    //判断音频流是否存在
    if (audio_stream != -1)
    {
        //设置一个结构体储存文件的所有信息
        AVCodecParameters *audio_par = ctx -> streams[audio_stream]->codecpar;
        //根据编码器ID 找到对应的编码器
        AVCodec *audio_codec =avcodec_find_decoder(audio_par->codec_id);
        //
        if (audio_codec)
        {
            //为解码器创建并初始化一个结构体
            audio_ctx = avcodec_alloc_context3(audio_codec);
            //把音频中的参数复制到解码器中
            avcodec_parameters_to_context(audio_ctx,audio_par);
            //打开编码器
            avcodec_open2(audio_ctx,audio_codec,NULL);

            //设置一个空的音频转换器
            swr_ctx = swr_alloc();
            //  设置采样器的输入声道
            av_opt_set_int(swr_ctx, "in_channel_layout",  audio_ctx->channel_layout ? audio_ctx->channel_layout : av_get_default_channel_layout(audio_ctx->channels), 0);
            //设置采样器的输出声道
            av_opt_set_int(swr_ctx, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
            //设置采样器的输入采样率
            av_opt_set_int(swr_ctx, "in_sample_rate",     audio_ctx->sample_rate, 0);
            //设置采样器的输出采样率
            av_opt_set_int(swr_ctx, "out_sample_rate",    48000, 0);
            //设置采样器的输入格式
            av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt",   audio_ctx->sample_fmt, 0);
            //设置采样器的输出格式
            av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt",  AV_SAMPLE_FMT_S16, 0);
            //根据前面的设置初始化采样器
            swr_init(swr_ctx);
        }
    }
    
    //6.初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
    {
        printf("SDL 初始化失败：%s\n", SDL_GetError());
        return -1;
    }
    //初始化SDL，并且告诉SDL我接下来是要用视频和音频相关的功能
    //创建互斥锁
    audio_buf.mutex = SDL_CreateMutex();
    //创建一个环境变量用于实现唤醒
    audio_buf.cond = SDL_CreateCond();
    //初始化等待
    audio_buf.waiting = 0;
    //创建一个窗口用来播放视频
    SDL_Window *window = SDL_CreateWindow(
        "FFmpeg 播放器",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        window_width,window_high,
        SDL_WINDOW_SHOWN
    );
    if (!window)
    {
        printf("创建窗口失败： %s\n",SDL_GetError());
        return -1;
    }
    
    //创建渲染器
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_YV12,
        SDL_TEXTUREACCESS_STREAMING,
        codec_cxt->width,
        codec_cxt ->height
    );
    
    if (audio_ctx && swr_ctx)
    {
        SDL_AudioSpec want = {0};
        want .freq = 48000;
        want.format = AUDIO_S16SYS;
        want.channels = 2;
        want .samples = 1024;
        want.callback = audio_callback;
        audio_buf.device_id = SDL_OpenAudioDevice(NULL , 0 , &want , NULL , 0);
        if (audio_buf.device_id)
        {
            SDL_PauseAudioDevice(audio_buf.device_id,0);
            printf("音频设备打开成功\n");
        }
        else
        {
            printf("音频设备打开失败\n",SDL_GetError());
        }
    }

    //7. 开始读取帧和解码和显示
    AVPacket pkt ;
    //数据包
    AVFrame *frame = av_frame_alloc();
    AVFrame *audio_frame = av_frame_alloc();
    //帧
    SDL_Event event;

    //读取每一帧的数据
    while (av_read_frame(ctx ,&pkt )>=0)
    {
        //判断是否有音频流且包内的是不是音频流且采样器是否初始化且音频设备是否打开
        if (audio_stream != -1 && pkt.stream_index == audio_stream && swr_ctx &&audio_buf.device_id)
        {
            //这个包是发送的音频文件
            avcodec_send_packet(audio_ctx, &pkt);
            //把音频解码器中的所有 帧都接收一边
            while (avcodec_receive_frame(audio_ctx,audio_frame) == 0)
            {
                //重采样后输出的数量
                int out_samples = av_rescale_rnd(
                    //缓存的加上当前输入的
                    swr_get_delay(swr_ctx, audio_ctx->sample_rate) + audio_frame->nb_samples,
                    48000,audio_ctx->sample_rate,AV_ROUND_UP);

                //把一个指向字节的变量初始化
                uint8_t *out =NULL;
                //为输入的音频分配缓冲区的内存
                av_samples_alloc(&out, NULL, 2, out_samples, AV_SAMPLE_FMT_S16, 0);

                //转换音频的格式存放到samples
                int samples = swr_convert(swr_ctx, &out, out_samples,
                                            (const uint8_t**)audio_frame->data, audio_frame->nb_samples);

                
                if (samples > 0)
                {
                    //计算缓存区的大小存放到size
                    int size = av_samples_get_buffer_size(NULL, 2, samples, AV_SAMPLE_FMT_S16, 1 );

                    //锁住互斥锁
                    SDL_LockMutex(audio_buf.mutex);
                    //有数据，上一层数据还没执行完
                    while(audio_buf.data != NULL)
                    {
                    //等待，最多10ms
                    SDL_CondWaitTimeout(audio_buf.cond, audio_buf.mutex, 10);
                    }

                    //进行初始化
                    audio_buf.data = out;
                    audio_buf.size = size;
                    audio_buf.pos = 0;
                    audio_buf.waiting = 0;

                    //等待线程唤醒
                    SDL_CondSignal(audio_buf.cond);
                    //解锁互斥锁
                    SDL_UnlockMutex(audio_buf.mutex);
                }
                else
                {
                    //释放内存
                    av_free(&out);
                }
                
            }
            
        }
        
        //看当前帧是不是视频帧  
        if (pkt.stream_index == video_stream)
        {
            //把包里的数据发送到解码器中
            avcodec_send_packet(codec_cxt,&pkt);
            //遍历解码器中的帧
            while (avcodec_receive_frame(codec_cxt,frame )==0)
            {
                //设置变量pts储存时间戳
                double pts = 0;
                //获得有效的时间戳
                //pts是显示时间戳，dts是解码时间戳
                //显示时间戳的优先级大于解码时间传
                if (frame->pts != AV_NOPTS_VALUE)
                {
                    pts = frame->pts * av_q2d(time_base);
                }
                else if (frame->pkt_dts != AV_NOPTS_VALUE)
                {
                    pts = frame->pkt_dts * av_q2d(time_base);
                }
                
                //设置变量存放两帧的等待时间
                double delay = frame_delay;
                if (last_pts != 0 && pts > 0)
                {
                    delay = pts - last_pts;
                    //间隔不合理时
                    if (delay <= 0 || delay >1.0)
                    {
                        //退回到理论值
                        delay = frame_delay;
                    }
                    
                }
                //更新时间戳
                last_pts =pts;
                //时间戳累加
                video_clock += delay;

                //把帧显示到窗口
                SDL_UpdateYUVTexture(
                    texture,NULL,
                    //Y平面，的亮度和字节数
                    frame->data[0],frame->linesize[0],
                    //U平面的亮度和字节数
                    frame->data[1],frame->linesize[1],
                    //V平面的亮度和字节数
                    frame->data[2],frame->linesize[2]
                );
                //纹理的复制到渲染
                SDL_RenderCopy(renderer,texture,NULL,NULL);
                SDL_RenderPresent(renderer);
                
                //把秒转换成毫秒并且取整数
                int delay_ms = (int)(delay * 1000);

                //设置最大的延迟为100ms
                if(delay_ms >100) delay_ms = 100;
                //设置最小的延迟是1ms
                if(delay_ms < 1) delay_ms = 1;
                //程序暂停
                SDL_Delay(delay_ms);
                
            }
            
        }
        //释放包的内容
        av_packet_unref(&pkt);

        //按ESC退出
        SDL_PollEvent(&event);
        if (event.type == SDL_QUIT) break; 
    }
    
    //有音频后停2s
    if (audio_buf.device_id)
    {
        SDL_Delay(2000);
    } 
 
    // 释放资源
    if (audio_buf.data) av_freep(&audio_buf.data);
    if (audio_buf.device_id) SDL_CloseAudioDevice(audio_buf.device_id);
    if (audio_buf.mutex) SDL_DestroyMutex(audio_buf.mutex);
    if (audio_buf.cond) SDL_DestroyCond(audio_buf.cond);
    av_frame_free(&audio_frame);
    av_frame_free(&frame);
    avcodec_close(codec_cxt);
    if (audio_ctx) avcodec_close(audio_ctx);
    if (swr_ctx) swr_free(&swr_ctx);
    avformat_close_input(&ctx);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("播放器正常退出\n");
    return 0;  
}