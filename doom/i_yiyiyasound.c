//
// i_yiyiyasound.c
//
// YiYiYa(duck) 的声音后端：SFX 自己混音后直接写 /dev/dsp；音乐暂时是空实现。
//
// 【为什么不用 i_sdlsound.c / i_sdlmusic.c】那两个模块依赖 SDL_mixer
// （<SDL_mixer.h>），本仓库没有 SDL_mixer（SDL2 只有内核的 yiyiya OSS 后端）。
// 而 duck 的声卡设备就是 OSS 风格：duck/modules/sound/sound.h 里定义了
// AFMT_S16_LE / SNDCTL_DSP_SET{FMT,CHANNELS,SPEED}，项目里 InfONES
// (app/infones/InfoNES_System_YiYiYa.c)、gnuboy (app/gnuboy/sys/sdl2/sdl-audio.c)
// 都是直接写这个设备，所以这里也走同一条路。
//
// 【关键设计】
//   1) 不用音频线程：duck 的 pthread 克隆会复制地址空间（gnuboy 那份注释里记过），
//      回调线程写进缓冲、主线程看不到。这里在 I_UpdateSound()（每个 tic 被 S_UpdateSounds()
//      调一次）里由主线程混音 + write()，write 阻塞在 DMA 上 ⇒ /dev/dsp 本身就是音频时钟。
//   2) 每次补多少帧按【实际流逝时间】算（I_GetTimeMS），而不是固定 1/35s：机器慢、掉帧时
//      音高和时长仍然正确，只是补的块更大（上限 MAX_SLICE_MS，超过就丢，不追）。
//   3) 不预展开样本：Doom 的 SFX 是 DMX 格式（8bit 无符号、单声道，多为 11025Hz），
//      展开成 16bit/44.1k 会让内存翻好几倍；这里混音时用 16.16 定点步进边播边重采样，
//      原始 PCM 就留在 WAD lump 里（PU_STATIC 缓存住指针）。
//

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "deh_str.h"
#include "doomtype.h"
#include "i_sound.h"
#include "i_timer.h"
#include "m_misc.h"
#include "w_wad.h"
#include "z_zone.h"

#ifndef arrlen
#define arrlen(array) (sizeof(array) / sizeof(*array))
#endif

/* duck/modules/sound/sound.h 里的 OSS 常量（用户态不便包含内核头，照抄数值） */
#ifndef AFMT_S16_LE
#define AFMT_S16_LE 16
#endif
#ifndef SNDCTL_DSP_SETFMT
#define SNDCTL_DSP_SETFMT 11
#endif
#ifndef SNDCTL_DSP_CHANNELS
#define SNDCTL_DSP_CHANNELS 33
#endif
#ifndef SNDCTL_DSP_SPEED
#define SNDCTL_DSP_SPEED 44
#endif

#define SOUND_DEVICE "/dev/dsp"

/* 与 i_sdlsound.c 保持一致的通道数：Doom 的 snd_channels 默认 8，
 * handle 就是通道号，所以 16 足够。 */
#define NUM_CHANNELS 16

/* 卡顿后一次最多补 100ms：再多声音也已经晚了，一次写爆设备缓冲只会更糟 */
#define MAX_SLICE_MS 100

/* 声音缓存：按 lump 号只解析一次 DMX 头。
 * samples 指向 WAD lump 内部（W_CacheLumpNum(..., PU_STATIC) 常驻），不另外拷贝。 */
typedef struct {
    int lumpnum;
    unsigned char *samples; /* 8bit 无符号，128 为静音中心 */
    int length;             /* 样本数 */
    int samplerate;
} cached_sound_t;

#define MAX_CACHED 512

static cached_sound_t sound_cache[MAX_CACHED];
static int sound_cache_count = 0;

typedef struct {
    boolean active;
    unsigned char *samples;
    int length;
    int pos;            /* 16.16 定点播放位置 */
    int step;           /* 16.16 定点步进 = 源采样率 / 输出采样率 */
    int gain_l, gain_r; /* 0..65535 定点增益（>>16 即得 vol*side/255） */
} channel_t;

static channel_t channels[NUM_CHANNELS];

static int dsp_fd = -1;
static boolean sound_initialized = false;
static boolean use_sfx_prefix = false;

static int out_channels = 2; /* 设备实际声道数 */
static int next_ms = 0;      /* 上次混音对应的时间点 */
static int frames_max = 0;   /* 单次混音缓冲容量（帧） */
static int32_t *mix32 = NULL; /* 累加用，立体声交错 */
static int16_t *mix16 = NULL; /* 输出用，立体声交错 */

/* i_sound.c / m_config.c 会引用这两个变量（原本定义在 i_sdlsound.c 里） */
int use_libsamplerate = 0;
float libsamplerate_scale = 0.65f;

/* ------------------------------------------------------------------ */
/* 声音 lump：DMX 头解析                                               */
/* ------------------------------------------------------------------ */

static void GetSfxLumpName(sfxinfo_t *sfx, char *buf, size_t buf_len)
{
    if (sfx->link != NULL)
    {
        sfx = sfx->link;
    }

    if (use_sfx_prefix)
    {
        M_snprintf(buf, buf_len, "ds%s", DEH_String(sfx->name));
    }
    else
    {
        M_snprintf(buf, buf_len, "%s", DEH_String(sfx->name));
    }
}

static int I_YYS_GetSfxLumpNum(sfxinfo_t *sfx)
{
    char namebuf[9];

    GetSfxLumpName(sfx, namebuf, sizeof(namebuf));

    return W_GetNumForName(namebuf);
}

/* 解析一个 DMX 声音 lump（格式与 i_sdlsound.c 的 CacheSFX 完全一致） */
static cached_sound_t *CacheLump(int lumpnum)
{
    unsigned char *data;
    cached_sound_t *c;
    int i, lumplen, samplerate;
    unsigned int length;

    for (i = 0; i < sound_cache_count; i++)
    {
        if (sound_cache[i].lumpnum == lumpnum)
        {
            return &sound_cache[i];
        }
    }

    if (lumpnum < 0 || sound_cache_count >= MAX_CACHED)
    {
        return NULL;
    }

    data = W_CacheLumpNum(lumpnum, PU_STATIC);
    lumplen = W_LumpLength(lumpnum);

    if (data == NULL || lumplen < 8 || data[0] != 0x03 || data[1] != 0x00)
    {
        return NULL; /* 不是有效的 DMX 音效 */
    }

    samplerate = (data[3] << 8) | data[2];
    length = ((unsigned int) data[7] << 24) | ((unsigned int) data[6] << 16)
           | ((unsigned int) data[5] << 8) | (unsigned int) data[4];

    if (length > (unsigned int) (lumplen - 8) || length <= 48)
    {
        return NULL;
    }

    /* DMX 库会跳过 lump 的头 16 字节和尾 16 字节（沿用 i_sdlsound.c 的处理），
     * 再跳过 8 字节头 ⇒ PCM 从 lump 偏移 24 开始。 */
    length -= 32;
    if (length > (unsigned int) (lumplen - 24))
    {
        length = (unsigned int) (lumplen - 24);
    }

    c = &sound_cache[sound_cache_count++];
    c->lumpnum = lumpnum;
    c->samples = data + 24;
    c->length = (int) length;
    c->samplerate = samplerate > 0 ? samplerate : 11025;

    return c;
}

static cached_sound_t *GetSoundData(sfxinfo_t *sfxinfo)
{
    char namebuf[9];
    int lumpnum = sfxinfo->lumpnum;

    if (lumpnum < 0)
    {
        GetSfxLumpName(sfxinfo, namebuf, sizeof(namebuf));
        lumpnum = W_CheckNumForName(namebuf);
        if (lumpnum < 0)
        {
            return NULL;
        }
        sfxinfo->lumpnum = lumpnum;
    }

    return CacheLump(lumpnum);
}

/* ------------------------------------------------------------------ */
/* 混音                                                                */
/* ------------------------------------------------------------------ */

/* 音量/声道分配语义与 i_sdlsound.c 的 I_SDL_UpdateSoundParams 一致：
 * vol ∈ 0..127，sep ∈ 0..254（0=全左，254=全右），left/right ∈ 0..255；
 * 这里再 *257 变成 0..65535 的定点增益。 */
static void SetChannelGains(channel_t *ch, int vol, int sep)
{
    int left = ((254 - sep) * vol) / 127;
    int right = ((sep) * vol) / 127;

    if (left < 0)
        left = 0;
    else if (left > 255)
        left = 255;
    if (right < 0)
        right = 0;
    else if (right > 255)
        right = 255;

    ch->gain_l = left * 257;
    ch->gain_r = right * 257;
}

static void MixChannel(channel_t *ch, int frames)
{
    int i;

    if (!ch->active)
    {
        return;
    }

    for (i = 0; i < frames; i++)
    {
        int ipos, frac, s0, s1, s;
        int32_t v;

        ipos = ch->pos >> 16;
        if (ipos >= ch->length)
        {
            ch->active = false; /* 播完了：S_Sound 下次查询会回收 */
            return;
        }

        /* 8bit 无符号 → -128..127，再线性插值重采样 */
        frac = ch->pos & 0xffff;
        s0 = (int) ch->samples[ipos] - 128;
        s1 = (ipos + 1 < ch->length) ? ((int) ch->samples[ipos + 1] - 128) : s0;
        s = (s0 * (0x10000 - frac) + s1 * frac) >> 16;

        v = (int32_t) s << 8; /* 16bit 有符号 */
        mix32[i * 2 + 0] += (int32_t) ((v * ch->gain_l) >> 16);
        mix32[i * 2 + 1] += (int32_t) ((v * ch->gain_r) >> 16);

        ch->pos += ch->step;
    }
}

/* 把整块写进设备；write 会阻塞到 DMA 腾出空间 */
static void WriteAll(const void *buf, size_t bytes)
{
    const char *p = (const char *) buf;

    while (bytes > 0)
    {
        ssize_t n = write(dsp_fd, p, bytes);

        if (n <= 0)
        {
            break; /* 设备出错：丢掉这一块，别把游戏卡死 */
        }
        p += n;
        bytes -= (size_t) n;
    }
}

static void I_YYS_UpdateSound(void)
{
    int now, elapsed, frames, i, c;

    if (!sound_initialized || dsp_fd < 0)
    {
        return;
    }

    now = I_GetTimeMS();
    if (next_ms == 0)
    {
        next_ms = now; /* 第一次调用只对表 */
        return;
    }

    elapsed = now - next_ms;
    if (elapsed <= 0)
    {
        return; /* 同一毫秒内的重复调用：攒着一起算 */
    }
    if (elapsed > MAX_SLICE_MS)
    {
        elapsed = MAX_SLICE_MS;
    }
    next_ms = now;

    frames = (elapsed * snd_samplerate) / 1000;
    if (frames <= 0)
    {
        return;
    }
    if (frames > frames_max)
    {
        frames = frames_max;
    }

    memset(mix32, 0, (size_t) frames * 2 * sizeof(int32_t));
    for (c = 0; c < NUM_CHANNELS; c++)
    {
        MixChannel(&channels[c], frames);
    }

    if (out_channels == 1)
    {
        /* 设备被配成单声道：左右相加（相当于 (L+R)/2，防溢出用 >>1 前先限幅） */
        for (i = 0; i < frames; i++)
        {
            int32_t v = mix32[i * 2] + mix32[i * 2 + 1];
            v >>= 1;
            if (v > 32767)
                v = 32767;
            else if (v < -32768)
                v = -32768;
            mix16[i] = (int16_t) v;
        }
        WriteAll(mix16, (size_t) frames * sizeof(int16_t));
    }
    else
    {
        for (i = 0; i < frames * 2; i++)
        {
            int32_t v = mix32[i];
            if (v > 32767)
                v = 32767;
            else if (v < -32768)
                v = -32768;
            mix16[i] = (int16_t) v;
        }
        WriteAll(mix16, (size_t) frames * 2 * sizeof(int16_t));
    }
}

/* ------------------------------------------------------------------ */
/* sound_module_t 接口                                                 */
/* ------------------------------------------------------------------ */

static void I_YYS_UpdateSoundParams(int handle, int vol, int sep)
{
    if (!sound_initialized || handle < 0 || handle >= NUM_CHANNELS)
    {
        return;
    }

    SetChannelGains(&channels[handle], vol, sep);
}

static int I_YYS_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep)
{
    cached_sound_t *snd;
    channel_t *ch;

    if (!sound_initialized || channel < 0 || channel >= NUM_CHANNELS)
    {
        return -1;
    }

    snd = GetSoundData(sfxinfo);
    if (snd == NULL)
    {
        return -1;
    }

    /* 同一通道上已有音效就直接顶掉（Doom 的通道语义） */
    ch = &channels[channel];
    ch->active = false;
    ch->samples = snd->samples;
    ch->length = snd->length;
    ch->step = (int) (((uint64_t) snd->samplerate << 16) / (uint64_t) snd_samplerate);
    if (ch->step <= 0)
    {
        ch->step = 0x10000;
    }
    ch->pos = 0;
    SetChannelGains(ch, vol, sep);
    ch->active = true;

    return channel;
}

static void I_YYS_StopSound(int handle)
{
    if (!sound_initialized || handle < 0 || handle >= NUM_CHANNELS)
    {
        return;
    }

    channels[handle].active = false;
}

static boolean I_YYS_SoundIsPlaying(int handle)
{
    if (!sound_initialized || handle < 0 || handle >= NUM_CHANNELS)
    {
        return false;
    }

    return channels[handle].active;
}

static void I_YYS_PrecacheSounds(sfxinfo_t *sounds, int num_sounds)
{
    char namebuf[9];
    int i;

    if (!sound_initialized)
    {
        return;
    }

    for (i = 0; i < num_sounds; i++)
    {
        GetSfxLumpName(&sounds[i], namebuf, sizeof(namebuf));
        CacheLump(W_CheckNumForName(namebuf));
    }
}

static boolean I_YYS_InitSound(boolean _use_sfx_prefix)
{
    int fmt, ch, hz, i;

    use_sfx_prefix = _use_sfx_prefix;

    for (i = 0; i < NUM_CHANNELS; i++)
    {
        memset(&channels[i], 0, sizeof(channel_t));
    }

    dsp_fd = open(SOUND_DEVICE, O_WRONLY);
    if (dsp_fd < 0)
    {
        printf("i_yiyiyasound: cannot open %s, sound disabled\n", SOUND_DEVICE);
        return false;
    }

    fmt = AFMT_S16_LE;
    ioctl(dsp_fd, SNDCTL_DSP_SETFMT, &fmt);
    ch = 2;
    ioctl(dsp_fd, SNDCTL_DSP_CHANNELS, &ch);
    hz = snd_samplerate;
    ioctl(dsp_fd, SNDCTL_DSP_SPEED, &hz);

    /* 以设备【实际】接受的值混音：驱动可能把请求调成它支持的档位 */
    if (hz > 0)
    {
        snd_samplerate = hz;
    }
    out_channels = (ch > 0) ? ch : 2;
    if (out_channels != 1)
    {
        out_channels = 2;
    }

    frames_max = (snd_samplerate * MAX_SLICE_MS) / 1000;
    if (frames_max < 256)
    {
        frames_max = 256;
    }

    mix32 = (int32_t *) malloc((size_t) frames_max * 2 * sizeof(int32_t));
    mix16 = (int16_t *) malloc((size_t) frames_max * 2 * sizeof(int16_t));
    if (mix32 == NULL || mix16 == NULL)
    {
        printf("i_yiyiyasound: out of memory\n");
        if (mix32 != NULL) free(mix32);
        if (mix16 != NULL) free(mix16);
        mix32 = NULL;
        mix16 = NULL;
        close(dsp_fd);
        dsp_fd = -1;
        return false;
    }

    next_ms = 0;
    sound_initialized = true;

    printf("i_yiyiyasound: %s %dHz %dch, slice<=%dms\n", SOUND_DEVICE,
           snd_samplerate, out_channels, MAX_SLICE_MS);

    return true;
}

static void I_YYS_ShutdownSound(void)
{
    int i;

    if (!sound_initialized)
    {
        return;
    }

    sound_initialized = false;

    if (dsp_fd >= 0)
    {
        close(dsp_fd);
        dsp_fd = -1;
    }

    if (mix32 != NULL)
    {
        free(mix32);
        mix32 = NULL;
    }
    if (mix16 != NULL)
    {
        free(mix16);
        mix16 = NULL;
    }

    /* 把 PU_STATIC 缓存的声音 lump 交回 Z_zone（它只降级 tag，不释放 WAD） */
    for (i = 0; i < sound_cache_count; i++)
    {
        W_ReleaseLumpNum(sound_cache[i].lumpnum);
    }
    sound_cache_count = 0;
}

static snddevice_t sound_yiyiya_devices[] =
{
    SNDDEVICE_SB,
    SNDDEVICE_PAS,
    SNDDEVICE_GUS,
    SNDDEVICE_WAVEBLASTER,
    SNDDEVICE_SOUNDCANVAS,
    SNDDEVICE_AWE32,
};

sound_module_t DG_sound_module =
{
    sound_yiyiya_devices,
    arrlen(sound_yiyiya_devices),
    I_YYS_InitSound,
    I_YYS_ShutdownSound,
    I_YYS_GetSfxLumpNum,
    I_YYS_UpdateSound,
    I_YYS_UpdateSoundParams,
    I_YYS_StartSound,
    I_YYS_StopSound,
    I_YYS_SoundIsPlaying,
    I_YYS_PrecacheSounds,
};

/* ------------------------------------------------------------------ */
/* music_module_t：暂时空实现                                          */
/* ------------------------------------------------------------------ */

/* Doom 的音乐是 MUS → MIDI（mus2mid.c 已编译进来），要真出声还需要 OPL/tiMidity
 * 之类的合成器，本平台暂时没有。这里提供完整空实现，让 I_InitMusic()/I_PlaySong()
 * 正常返回，不会因为 NULL 指针或初始化失败而报错。 */

static boolean I_YYS_MusicInit(void) { return true; }
static void I_YYS_MusicShutdown(void) {}
static void I_YYS_SetMusicVolume(int volume) { (void) volume; }
static void I_YYS_PauseMusic(void) {}
static void I_YYS_ResumeMusic(void) {}
static void *I_YYS_RegisterSong(void *data, int len) { (void) data; (void) len; return NULL; }
static void I_YYS_UnRegisterSong(void *handle) { (void) handle; }
static void I_YYS_PlaySong(void *handle, boolean looping) { (void) handle; (void) looping; }
static void I_YYS_StopSong(void) {}
static boolean I_YYS_MusicIsPlaying(void) { return false; }
static void I_YYS_PollMusic(void) {}

music_module_t DG_music_module =
{
    sound_yiyiya_devices,
    arrlen(sound_yiyiya_devices),
    I_YYS_MusicInit,
    I_YYS_MusicShutdown,
    I_YYS_SetMusicVolume,
    I_YYS_PauseMusic,
    I_YYS_ResumeMusic,
    I_YYS_RegisterSong,
    I_YYS_UnRegisterSong,
    I_YYS_PlaySong,
    I_YYS_StopSong,
    I_YYS_MusicIsPlaying,
    I_YYS_PollMusic,
};
