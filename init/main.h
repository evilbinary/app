/*******************************************************************
 * Copyright 2021-present evilbinary
 * 作者: evilbinary on 01/01/20
 * 邮箱: rootdebug@163.com
 ********************************************************************/
#ifndef MAIN_H
#define MAIN_H

#include "kernel/kernel.h"

extern void start();
void kstart(int argc, char* argv[], char** envp);
int kmain(int argc, char* argv[]);
int ksecondary(int cpu, int argc, char* argv[]);


#endif