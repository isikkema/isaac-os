#pragma once


#include "gpu.h"


void sleep(int tm);
void yield(void);
int open(const char *path, int flags);
int read(int fd, char *buffer, int max_size);
int write(int fd, const char *buffer, int bytes);
void close(int fd);
int gpu_get_display_info(int scanout_id, VirtioGpuRectangle* rect);

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
