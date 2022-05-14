#pragma once


#include "gpu.h"
#include "event.h"


void sleep(int tm);
void yield(void);
unsigned int get_events(InputEvent event_buffer[], unsigned int max_events);
int open(const char *path, int flags);
int read(int fd, char *buffer, int max_size);
int write(int fd, const char *buffer, int bytes);
void close(int fd);
int gpu_get_display_info(int scanout_id, VirtioGpuRectangle* rect);
int gpu_fill_and_flush(int scanout_id, VirtioGpuRectangle* fill_rect, VirtioGpuPixel* pixel);

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
