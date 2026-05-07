#include<stdio.h>
#include<libavformat/avformat.h>
#include<libavcodec/avcodec.h>
#include<libswscale/swscale.h>
#include<SDL2/SDL.h>

//视频显示的窗口大小
//窗口的宽
#define window_width 640
//窗口的长
#define window_high 360

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
    for (int  i = 0; i < ctx->nb_streams; i++)
    {
        if (ctx-> streams[i]->codecpar ->codec_type == AVMEDIA_TYPE_VIDEO)
        //codecpar:编码的参数，cpdec_type：流的类型
        {
            video_stream = i;
            break;
        }
        
    }
    if (video_stream == -1)
    //0是未知，1是视频，2是音频，
    {
        printf("没有找到视频流\n");
        return -1;
    }
    
    //5.获得解码器
    AVCodecParameters *codec_par = ctx ->streams[video_stream]->codecpar;
    //把存入的流放到ctx中，从而获取这个些流的信息，方便告诉编码器
    AVCodec *codec = avcodec_find_decoder(codec_par->codec_id);
    //寻找合适的编码器
    AVCodecContext *codec_cxt =avcodec_alloc_context3(codec);
    //空白一个解码器让流的信息有地方储存
    avcodec_parameters_to_context(codec_cxt,codec_par);
    //把之前的流的信息都储存到编码器中
    avcodec_open2(codec_cxt,codec ,NULL);
    //打开编码器

    //检查
    if (!codec)
    {
        printf("找不到解码器\n");
        return-1;
    }
    
    if (!codec_cxt)
    {
        printf("分配上下文失败\n");
        return-1;
    }
    
    if (avcodec_open2(codec_cxt, codec, NULL) < 0)
     {
    printf("打开解码器失败\n");
    return -1;
     }

    //6.初始化SDL
         SDL_Init(SDL_INIT_VIDEO);
         //初始化SDL，并且告诉SDL我接下来是要用视频相关的功能
    //创建一个窗口用来播放视频
    SDL_Window *window = SDL_CreateWindow(
        "FFmpeg 播放器",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        window_width,window_high,
        SDL_WINDOW_SHOWN
    );
    //创建渲染器
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_YV12,
        SDL_TEXTUREACCESS_STREAMING,
        codec_cxt->width,
        codec_cxt ->height
    );
    //7. 开始读取帧和解码和显示
    AVPacket pkt ;
    //数据包
    AVFrame *frame = av_frame_alloc();
    //帧
    SDL_Event event;

    //读取每一帧的数据
    while (av_read_frame(ctx ,&pkt )>=0)
    {
        //看当前帧是不是视频帧  
        if (pkt.stream_index == video_stream)
        {
            //把包里的数据发送到解码器中
            avcodec_send_packet(codec_cxt,&pkt);
            //遍历解码器中的帧
            while (avcodec_receive_frame(codec_cxt,frame )==0)
            {
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
                
                //控制播放速度25fps
                SDL_Delay(25);
            }
            
        }
        //释放包的内容
        av_packet_unref(&pkt);

        //按ESC退出
        SDL_PollEvent(&event);
        if (event.type == SDL_QUIT) break; 
    }
    // 释放资源
    av_frame_free(&frame);
    avcodec_close(codec_cxt);
    avformat_close_input(&ctx);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;  
}