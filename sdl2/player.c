#include <SDL.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stdio.h"

int ffplayer(int argc, char *argv[]) {
  uint8_t errstr[1024];

  AVFormatContext *pFormatCtx;
  int i, videoindex;
  AVCodecContext *pCodecCtx;
  AVCodec *pCodec;
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

  //------------SDL初始化--------
  if (SDL_Init(SDL_INIT_VIDEO)) {  //| SDL_INIT_AUDIO | SDL_INIT_TIMER
    printf("Could not initialize SDL - %s\n", SDL_GetError());
    return -1;
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
  //-----------------------------
  int ret, got_picture;
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
        // 延时20ms
        SDL_Delay(20);
        //------------SDL-----------
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