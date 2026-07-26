/*
 * nix.c
 *
 * System interface for *nix systems.
 */
#undef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include <SDL2/SDL.h>

#include "defs.h"
#include "rc.h"

void *sys_timer()
{
	Uint32 *tv;
	
	tv = malloc(sizeof *tv);
	*tv = SDL_GetTicks() * 1000;
	return tv;
}

int sys_elapsed(Uint32 *cl)
{
	Uint32 now;
	Uint32 usecs;

	now = SDL_GetTicks() * 1000;
	usecs = now - *cl;
	*cl = now;
	/* 防御：时钟回绕/异常会得到超大间隔，导致 sys_sleep 卡死无画面 */
	if (usecs > 200000) /* >200ms */
		usecs = 0;
	return (int)usecs;
}

void sys_sleep(int us)
{
	if (us <= 0) return;
	if (us > 50000) us = 50000; /* 最多睡 50ms */
	SDL_Delay(us / 1000);
}

void sys_checkdir(char *path, int wr)
{
}

void sys_initpath()
{
    char *buf = "";

    if (rc_getstr("rcpath") == NULL)
        rc_setvar("rcpath", 1, &buf);

    if (rc_getstr("savedir") == NULL)
        rc_setvar("savedir", 1, &buf);
}

void sys_sanitize(char *s)
{
}




