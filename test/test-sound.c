#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "cmocka.h"

#define DSPIORATE 1
#define DSPIOSTEREO 2
#define DSPIOBITS 3
#define DSPIOSIZE 4
#define DSPIOSIGN 5

#define WAVE_SIZE (44100 / 8)
static short wave[WAVE_SIZE][2];
int sound_fd = -1;

static void sound_output(int hz) {
  if (sound_fd != -1) {
    for (int i = 0; i < WAVE_SIZE; i++) {
      wave[i][1] = wave[i][0] = 30000 * sin(2 * 3.14159265358f * hz * i / 44100.0f);
    }
    short(*channel)[WAVE_SIZE] = wave;
    write(sound_fd, channel[0], WAVE_SIZE * sizeof(short));
    write(sound_fd, channel[1], WAVE_SIZE * sizeof(short));
  }
}

void sound(int second, int hz) {
  int time[9] = {0, 1, 2, 0, 4, 0, 0, 0, 8};
  for (int i = 0; i < time[second]; i++) sound_output(hz);
}

static void test() {
  int f[8] = {0, 494, 554, 622, 659, 740, 881, 932};
  int fh[8] = {0, 988, 1109, 1245, 1318, 1480, 1661, 1865};
  int fhh[8] = {0, 1975, 1109, 1245, 1318, 1480, 1661, 1865};
  int p[4] = {1, 2, 4, 8};
  sound(p[1], f[5]);
  sound(p[1], fh[1]);
  sound(p[1], fh[2]);

  sound(p[1], fh[2]);
  sound(p[1], fh[3]);
  sound(p[1], fh[3]);
  sound(p[1], f[0]);
  sound(p[1], fh[2]);
  sound(p[1], fh[3]);
  sound(p[1], fh[3]);
  sound(p[1], f[0]);

  sound(p[1], fh[2]);
  sound(p[1], fh[3]);
  sound(p[1], fh[5]);
  sound(p[1], fh[3]);
  sound(p[1], fh[1]);
  sound(p[1], f[0]);
  sound(p[1], fh[1]);
  sound(p[1], f[5]);

  sound(p[2], f[6]);
  sound(p[1], f[0]);
  sound(p[1], fh[3]);
  sound(p[2], fh[2]);
  sound(p[1], fh[1]);
  sound(p[1], fh[2]);
  //
  sound(p[3], fh[3]);
  sound(p[1], f[0]);
  sound(p[1], f[5]);
  sound(p[1], fh[1]);
  sound(p[1], fh[2]);

  sound(p[1], fh[2]);
  sound(p[1], fh[3]);
  sound(p[1], fh[3]);
  sound(p[1], f[0]);
  sound(p[1], fh[2]);
  sound(p[1], fh[3]);
  sound(p[1], fh[3]);
  sound(p[1], f[0]);

  sound(p[1], fh[2]);
  sound(p[1], fh[3]);
  sound(p[1], fh[5]);
  sound(p[1], fh[3]);
  sound(p[1], fh[1]);
  sound(p[1], f[0]);
  sound(p[1], fh[1]);
  sound(p[1], f[7]);

  sound(p[2], f[6]);
  sound(p[1], f[0]);
  sound(p[1], fh[3]);
  sound(p[2], fh[2]);
  sound(p[1], fh[1]);
  sound(p[1], fh[2]);

  sound(p[3], fh[1]);
  sound(p[2], f[0]);
  sound(p[1], fh[1]);
  sound(p[1], f[7]);

  sound(p[2], f[6]);
  sound(p[2], fh[1]);
  sound(p[1], fh[2]);
  sound(p[1], f[0]);
  sound(p[1], fh[1]);
  sound(p[1], fh[2]);

  //
  sound(p[1], fh[3]);
  sound(p[1], fh[5]);
  sound(p[1], fh[2]);
  sound(p[1], fh[3]);
  sound(p[1], fh[1]);
  sound(p[1], f[0]);
  sound(p[1], fh[1]);
  sound(p[1], f[7]);

  sound(p[2], f[6]);
  sound(p[2], fh[1]);
  sound(p[1], fh[2]);
  sound(p[0], fh[3]);
  sound(p[0], fh[2]);
  sound(p[1], fh[1]);
  sound(p[1], fh[2]);

  sound(p[3], fh[3]);
  sound(p[2], f[0]);
  sound(p[1], fh[1]);
  sound(p[1], f[7]);

  //
  sound(p[2], f[6]);
  sound(p[1], fh[1]);
  sound(p[1], f[0]);
  sound(p[1], fh[2]);
  sound(p[0], fh[3]);
  sound(p[0], fh[2]);
  sound(p[1], fh[1]);
  sound(p[1], fh[2]);

  sound(p[1], fh[3]);
  sound(p[1], fh[5]);
  sound(p[1], fh[5]);
  sound(p[1], fh[3]);
  sound(p[1], fh[6]);
  sound(p[1], f[0]);
  sound(p[1], fh[3]);
  sound(p[1], fh[2]);

  sound(p[2], fh[1]);
  sound(p[1], f[0]);
  sound(p[1], fh[3]);
  sound(p[1], fh[2]);
  sound(p[1], fh[2]);
  sound(p[1], fh[1]);
  sound(p[1], fh[2]);

  //
  sound(p[3], fh[1]);
  sound(p[1], f[0]);
  sound(p[1], fh[1]);
  sound(p[1], fh[3]);
  sound(p[1], fh[4]);

  sound(p[1], fh[5]);
  sound(p[1], fh[1]);
  sound(p[1], fh[3]);
  sound(p[1], fh[4]);
  sound(p[2], fh[5]);
  sound(p[1], fh[6]);
  sound(p[1], fh[7]);

  sound(p[1], fhh[1]);
  sound(p[1], fh[3]);
  sound(p[1], fh[3]);
  sound(p[1], fh[4]);
  sound(p[2], fh[5]);
  sound(p[2], f[0]);
}

void test_sound() {
  sound_fd = open("/dev/dsp", 0);
  if (sound_fd < 0) {
    return -1;
  }

  int val;

  //   val = NX_AUDIO_FTM_S16_LE;
  // NX_DeviceControl(sound_fd, NX_AUDIO_SET_FMT, &val);
  val = 44100;
  // NX_DeviceControl(sound_fd, NX_AUDIO_SET_SPEED, &val);
  val = 2;
  // NX_DeviceControl(sound_fd, NX_AUDIO_SET_CHANNELS, &val);

  test();
  close(sound_fd);
}

int main(int argc, char *argv[]) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_sound),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}