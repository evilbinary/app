#include <SDL.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stdio.h"

/* 音频输出固定转成设备支持的 S16LE 立体声（内核 sound 模块只支持 S16LE，
 * 采样率 8000~48000 都行；这里统一用 44100）。 */
#define AUDIO_OUT_RATE 44100
#define AUDIO_BUF_SAMPLES 32768 /* 重采样输出缓冲（样本数，S16 立体声 = 128KB） */

int ffplayer(int argc, char *argv[]) {
  uint8_t errstr[1024];

  AVFormatContext *pFormatCtx;
  int i, videoindex, audioindex;
  AVCodecContext *pCodecCtx;
  AVCodec *pCodec;
  AVCodecContext *pAudioCodecCtx = NULL;
  AVCodec *pAudioCodec = NULL;
  AVFrame *pAudioFrame = NULL;
  struct SwrContext *audio_swr = NULL;
  uint8_t *audio_buf = NULL;
  SDL_AudioDeviceID audio_dev = 0;
  int audio_rate = AUDIO_OUT_RATE;
  Uint32 audio_queue_limit = 0;
  /* YiYiYa: 输入文件/URL 从命令行参数取，不要写死：
   *   player <file|url>    例如：player /roms/video.mp4
   *                          player rtsp://user:pass@host:port/...
   * 不给参数时才回退到默认路径（方便调试）。 */
  const char *filepath = "/test.mp4";
  if (argc > 1 && argv[1] != NULL && argv[1][0] != '\0') {
    filepath = argv[1];
  }
  printf("player: open %s\n", filepath);

  //av_log_set_level(AV_LOG_TRACE);  // AV_LOG_TRACE AV_LOG_VERBOSE
  //   char rtspUrl[] = "rtsp://admin:12345@192.168.10.76:522";
  av_register_all();                      // 注册组件
  avformat_network_init();                // 支持网络流
  pFormatCtx = avformat_alloc_context();  // 初始化AVFormatContext

  int err =
      avformat_open_input(&pFormatCtx, filepath /*rtspUrl*/, NULL, NULL);
  if (err != 0) {  // 打开文件或网络流
    av_strerror(err, errstr, sizeof(errstr));
    printf("无法打开文件 %d %s\n", err, errstr);
    return -1;
  }

  if (avformat_find_stream_info(pFormatCtx, NULL) < 0)  // 查找流信息
  {
    printf("Couldn't find stream information.\n");
    return -1;
  }
  videoindex = -1;
  for (i = 0; i < pFormatCtx->nb_streams; i++)  // 获取视频流ID
    if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
      videoindex = i;
      break;
    }
  if (videoindex == -1) {
    printf("Didn't find a video stream.\n");
    return -1;
  }
  pCodecCtx = pFormatCtx->streams[videoindex]->codec;
  pCodec = avcodec_find_decoder(pCodecCtx->codec_id);  // 查找解码器
  if (pCodec == NULL) {
    printf("Codec not found.\n");
    return -1;
  }

  printf("1 width:%d height:%d\n", pCodecCtx->width, pCodecCtx->height);

  if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)  // 打开解码器
  {
    printf("Could not open codec.\n");
    return -1;
  }
  printf("width:%d height:%d\n", pCodecCtx->width, pCodecCtx->height);
  if (pCodecCtx->width == 0) {
    pCodecCtx->width = 320;
    pCodecCtx->height = 240;
  }

  /* 音频流：有就解码播放，没有/打开失败就按无声播放（不影响视频） */
  audioindex = -1;
  for (i = 0; i < pFormatCtx->nb_streams; i++)
    if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
      audioindex = i;
      break;
    }
  if (audioindex >= 0) {
    pAudioCodecCtx = pFormatCtx->streams[audioindex]->codec;
    pAudioCodec = avcodec_find_decoder(pAudioCodecCtx->codec_id);
    if (pAudioCodec == NULL ||
        avcodec_open2(pAudioCodecCtx, pAudioCodec, NULL) < 0) {
      printf("player: 音频解码器不可用，无声播放\n");
      pAudioCodecCtx = NULL;
      audioindex = -1;
    } else {
      printf("player: audio %d Hz %d ch fmt=%d\n", pAudioCodecCtx->sample_rate,
             pAudioCodecCtx->channels, pAudioCodecCtx->sample_fmt);
    }
  }

  //------------SDL初始化--------
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    /* 音频子系统失败不致命：退回纯视频再试一次 */
    printf("SDL init (video|audio) failed: %s\n", SDL_GetError());
    if (SDL_Init(SDL_INIT_VIDEO)) {
      printf("Could not initialize SDL - %s\n", SDL_GetError());
      return -1;
    }
  }

  /* YiYiYa: 视频分辨率超过屏幕分辨率时，等比缩小到屏幕内再显示，
     避免按视频原始分辨率分配纹理/转换缓冲导致内存不足。 */
  int dst_w = pCodecCtx->width, dst_h = pCodecCtx->height;
  SDL_DisplayMode dmode;
  if (SDL_GetCurrentDisplayMode(0, &dmode) == 0 && dmode.w > 0 && dmode.h > 0 &&
      (dst_w > dmode.w || dst_h > dmode.h)) {
    if ((long long)dst_w * dmode.h > (long long)dmode.w * dst_h) {
      dst_w = dmode.w;
      dst_h = (int)((long long)pCodecCtx->height * dmode.w / pCodecCtx->width);
    } else {
      dst_h = dmode.h;
      dst_w = (int)((long long)pCodecCtx->width * dmode.h / pCodecCtx->height);
    }
    if (dst_w < 2) dst_w = 2;
    if (dst_h < 2) dst_h = 2;
    dst_w &= ~1;  // YUV420P 宽高取偶数
    dst_h &= ~1;
    printf("player: screen %dx%d, video %dx%d -> display %dx%d\n", dmode.w,
           dmode.h, pCodecCtx->width, pCodecCtx->height, dst_w, dst_h);
  }

  AVFrame *pFrame, *pFrameYUV;
  pFrame = av_frame_alloc();     // 存储解码后AVFrame
  pFrameYUV = av_frame_alloc();  // 存储转换后AVFrame
  uint8_t *out_buffer;
  out_buffer = malloc(
      avpicture_get_size(AV_PIX_FMT_YUV420P, dst_w, dst_h));  // 分配AVFrame所需内存
  if (!out_buffer) {
    printf("player: 内存不足，分配 %dx%d 转换缓冲失败\n", dst_w, dst_h);
    return -1;
  }
  avpicture_fill((AVPicture *)pFrameYUV, out_buffer, AV_PIX_FMT_YUV420P, dst_w,
                 dst_h);  // 填充AVFrame

  SDL_Window *screen = SDL_CreateWindow(
      "RTSP Client Demo", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      dst_w, dst_h,
      SDL_WINDOW_RESIZABLE /* SDL_WINDOW_HIDDEN*/);  //| SDL_WINDOW_OPENGL
  if (!screen) {
    printf("SDL: could not set video mode - exiting\n");
    return -1;
  }
  SDL_Renderer *sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
  /* 注意：sws 输出是 AV_PIX_FMT_YUV420P（Y+U+V），对应 SDL 的 IYUV；
     用 YV12（Y+V+U）会把 U/V 平面搞反 ⇒ 蓝红互换。 */
  SDL_Texture *sdlTexture = SDL_CreateTexture(
      sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, dst_w,
      dst_h);
  if (!sdlTexture) {
    printf("SDL: could not create texture - %s\n", SDL_GetError());
    return -1;
  }

  /* 显示区域：按窗口实际大小居中（yiyiya 后端会把窗口固定为整屏） */
  SDL_Rect rect;
  int win_w = dst_w, win_h = dst_h;
  SDL_GetWindowSize(screen, &win_w, &win_h);
  rect.x = (win_w - dst_w) / 2;
  rect.y = (win_h - dst_h) / 2;
  if (rect.x < 0) rect.x = 0;
  if (rect.y < 0) rect.y = 0;
  rect.w = dst_w;
  rect.h = dst_h;

  /* 音频设备 + 重采样（解码帧 → S16 立体声 @ have.freq，用 SDL_QueueAudio 送） */
  if (audioindex >= 0 && SDL_WasInit(SDL_INIT_AUDIO)) {
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = AUDIO_OUT_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = NULL; /* 回调留空 = 用 SDL_QueueAudio */
    audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio_dev == 0) {
      printf("player: 打开音频设备失败: %s（无声播放）\n", SDL_GetError());
    } else {
      int64_t in_layout = pAudioCodecCtx->channel_layout;
      if (in_layout == 0) {
        in_layout = av_get_default_channel_layout(pAudioCodecCtx->channels);
      }
      audio_swr = swr_alloc_set_opts(
          NULL, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, have.freq, in_layout,
          pAudioCodecCtx->sample_fmt, pAudioCodecCtx->sample_rate, 0, NULL);
      pAudioFrame = av_frame_alloc();
      audio_buf = malloc(AUDIO_BUF_SAMPLES * 2 * sizeof(int16_t));
      if (audio_swr == NULL || swr_init(audio_swr) < 0 ||
          pAudioFrame == NULL || audio_buf == NULL) {
        printf("player: 音频重采样初始化失败（无声播放）\n");
        if (audio_swr != NULL) {
          swr_free(&audio_swr);
        }
        if (pAudioFrame != NULL) {
          av_frame_free(&pAudioFrame);
        }
        if (audio_buf != NULL) {
          free(audio_buf);
          audio_buf = NULL;
        }
        SDL_CloseAudioDevice(audio_dev);
        audio_dev = 0;
      } else {
        audio_rate = have.freq;
        /* 队列上限 ≈ 2 秒，防解码快于播放时内存无限增长 */
        audio_queue_limit = (Uint32)audio_rate * 2 * 2 * 2;
        SDL_PauseAudioDevice(audio_dev, 0);
        printf("player: audio out %d Hz %d ch (%s)\n", have.freq,
               have.channels, SDL_GetCurrentAudioDriver());
      }
    }
  }
  //-----------------------------
  int ret, got_picture;
  /* 音视频节奏基准：按包 PTS 控制显示节奏（原来固定 20ms，25fps 视频会播成 2 倍速） */
  double pts_start = 0;
  AVRational video_tb = pFormatCtx->streams[videoindex]->time_base;
  static struct SwsContext *img_convert_ctx;
  int y_size = pCodecCtx->width * pCodecCtx->height;

  SDL_Event event;
  AVPacket *packet =
      (AVPacket *)malloc(sizeof(AVPacket));  // 存储解码前数据包AVPacket
  av_new_packet(packet, y_size);
  // 输出一下信息-----------------------------
  printf("文件信息-----------------------------------------\n");
  av_dump_format(pFormatCtx, 0, filepath, 0);
  printf("-------------------------------------------------\n");
  //------------------------------
  while (av_read_frame(pFormatCtx, packet) >= 0)  // 循环获取压缩数据包AVPacket
  {
    if (packet->stream_index == videoindex) {
      ret =
          avcodec_decode_video2(pCodecCtx, pFrame, &got_picture,
                                packet);  // 解码。输入为AVPacket，输出为AVFrame
      if (ret < 0) {
        printf("解码错误 %d\n", ret);
        return -1;
      }
      if (got_picture) {
        // 像素格式转换（超过屏幕分辨率时缩到 dst_w x dst_h）。
        img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
                                         pCodecCtx->pix_fmt, dst_w, dst_h,
                                         AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL,
                                         NULL, NULL);
        sws_scale(img_convert_ctx, (const uint8_t *const *)pFrame->data,
                  pFrame->linesize, 0, pCodecCtx->height, pFrameYUV->data,
                  pFrameYUV->linesize);
        sws_freeContext(img_convert_ctx);
        //------------SDL显示--------
        SDL_Rect src_rect;
        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.w = dst_w;
        src_rect.h = dst_h;

        SDL_UpdateTexture(sdlTexture, &src_rect, pFrameYUV->data[0],
                          pFrameYUV->linesize[0]);
        SDL_RenderClear(sdlRenderer);
        SDL_RenderCopy(sdlRenderer, sdlTexture, &src_rect, &rect);
        SDL_RenderPresent(sdlRenderer);
        /* 按 PTS 控制节奏（拿不到 PTS 时退回固定 20ms） */
        int64_t pts = packet->pts;
        if (pts == AV_NOPTS_VALUE) {
          pts = packet->dts;
        }
        if (pts != AV_NOPTS_VALUE) {
          double t = pts * av_q2d(video_tb);
          double now = av_gettime_relative() / 1000000.0;
          if (pts_start == 0) {
            pts_start = now - t;
          }
          double wait = pts_start + t - now;
          if (wait > 0.001) {
            if (wait > 0.2) {
              wait = 0.2; /* 单次最多 200ms，防异常 PTS 卡住 */
            }
            SDL_Delay((Uint32)(wait * 1000));
          }
        } else {
          SDL_Delay(20);
        }
        //------------SDL-----------
      }
    } else if (audio_dev != 0 && packet->stream_index == audioindex) {
      /* 音频：解码 → 重采样成设备格式 → 排队（SDL 音频线程负责写到 /dev/dsp） */
      int got_audio = 0;
      ret =
          avcodec_decode_audio4(pAudioCodecCtx, pAudioFrame, &got_audio, packet);
      if (ret >= 0 && got_audio && pAudioFrame->nb_samples > 0) {
        uint8_t *outp = audio_buf;
        int out_cap = pAudioFrame->nb_samples * 8 + 256; /* 覆盖 8k→48k 的 6 倍重采样 */
        if (out_cap > AUDIO_BUF_SAMPLES) {
          out_cap = AUDIO_BUF_SAMPLES;
        }
        int n = swr_convert(audio_swr, &outp, out_cap,
                            (const uint8_t **)pAudioFrame->data,
                            pAudioFrame->nb_samples);
        if (n > 0) {
          SDL_QueueAudio(audio_dev, audio_buf, (Uint32)n * 2 * sizeof(int16_t));
          /* 队列超过约 2 秒就等一等，避免解码快于播放时内存无限增长 */
          while (audio_queue_limit > 0 &&
                 SDL_GetQueuedAudioSize(audio_dev) > audio_queue_limit) {
            SDL_Delay(20);
          }
        }
      }
    }
    av_free_packet(packet);
    SDL_PollEvent(&event);
    switch (event.type) {
      case SDL_QUIT:
        SDL_Quit();
        exit(0);
        break;
      default:
        break;
    }
  }

  if (audio_dev != 0) {
    SDL_CloseAudioDevice(audio_dev);
    audio_dev = 0;
  }
  if (audio_swr != NULL) {
    swr_free(&audio_swr);
  }
  if (pAudioFrame != NULL) {
    av_frame_free(&pAudioFrame);
  }
  if (audio_buf != NULL) {
    free(audio_buf);
  }
  if (pAudioCodecCtx != NULL) {
    avcodec_close(pAudioCodecCtx);
  }
  SDL_DestroyTexture(sdlTexture);
  free(out_buffer);
  av_free(pFrameYUV);
  avcodec_close(pCodecCtx);
  avformat_close_input(&pFormatCtx);

  return 0;
}

int main(int argc, char *argv[]) {
  ffplayer(argc, argv);

  return 0;
}