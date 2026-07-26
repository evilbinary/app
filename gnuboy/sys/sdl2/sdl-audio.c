/*
 * sdl-audio.c
 * YiYiYa: write PCM to /dev/dsp on the main thread.
 *
 * Do NOT use SDL_OpenAudioDevice callback threads here: duck's pthread/clone
 * copies the address space (vmemory_clone), so audio_done / pcm.buf are not
 * shared and the emu thread blocks forever while the audio thread spins.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "pcm.h"
#include "rc.h"
#include "sys.h"

#ifndef SOUND_DEVICE
#define SOUND_DEVICE "/dev/dsp"
#endif

/* YiYiYa OSS ioctl numbers (duck/modules/sound/sound.h) */
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
#ifndef SNDCTL_DSP_GETFMTS
#define SNDCTL_DSP_GETFMTS 22
#endif

struct pcm pcm;

static int samplerate = 22050;
static int stereo = 1;
static int sound = 1;
static int dsp_fd = -1;

rcvar_t pcm_exports[] =
{
	RCV_BOOL("sound", &sound),
	RCV_INT("stereo", &stereo),
	RCV_INT("samplerate", &samplerate),
	RCV_END
};

void pcm_init()
{
	int fmt;
	int ch;
	int hz;

	pcm.hz = samplerate;
	pcm.stereo = stereo;
	pcm.len = (samplerate / 60) * (1 + stereo) * 2; /* ~1 frame, S16 */
	if (pcm.len < 512)
		pcm.len = 512;
	pcm.buf = malloc(pcm.len);
	pcm.pos = 0;
	if (pcm.buf)
		memset(pcm.buf, 0, pcm.len);

	if (!sound)
		return;

	dsp_fd = open(SOUND_DEVICE, O_WRONLY);
	if (dsp_fd < 0) {
		printf("WARNING: cannot open %s, sound disabled\n", SOUND_DEVICE);
		sound = 0;
		return;
	}

	fmt = AFMT_S16_LE;
	ioctl(dsp_fd, SNDCTL_DSP_SETFMT, &fmt);
	ch = 1 + stereo;
	ioctl(dsp_fd, SNDCTL_DSP_CHANNELS, &ch);
	hz = samplerate;
	ioctl(dsp_fd, SNDCTL_DSP_SPEED, &hz);
	if (hz > 0)
		pcm.hz = hz;
	pcm.stereo = (ch > 1) ? 1 : 0;

	printf("pcm: %s size=%d freq=%d stereo=%d\n",
	       SOUND_DEVICE, pcm.len, pcm.hz, pcm.stereo);
}

int pcm_submit()
{
	if (!pcm.buf)
		return 0;
	if (pcm.pos < pcm.len)
		return 1;

	if (sound && dsp_fd >= 0) {
		ssize_t n = write(dsp_fd, pcm.buf, pcm.len);
		(void)n;
	}
	pcm.pos = 0;
	/* return 0 so emu uses framelen sleep (no audio-thread sync) */
	return 0;
}

void pcm_close()
{
	if (dsp_fd >= 0) {
		close(dsp_fd);
		dsp_fd = -1;
	}
	if (pcm.buf) {
		free(pcm.buf);
		pcm.buf = NULL;
	}
}

void pcm_pause()
{
}

void pcm_resume()
{
}
