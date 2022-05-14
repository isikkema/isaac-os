#include "os.h"
#include "event.h"
#include "printf.h"


#define SCANOUT_ID 0

#define BRUSH_SIZE 10

#define EVENT_BUF_SIZE  512
#define RECT_BUF_SIZE   256


int init_screen(VirtioGpuRectangle* screen_rect) {
    int rv;

    rv = gpu_get_display_info(SCANOUT_ID, screen_rect);
    if (rv != 0) {
        return rv;
    }

    rv = gpu_fill_and_flush(SCANOUT_ID, screen_rect, &((VirtioGpuPixel) {255, 255, 255, 255}));
    if (rv != 0) {
        return rv;
    }

    return 0;
}

unsigned int events_to_rects(InputEvent events[], VirtioGpuRectangle rects[], VirtioGpuRectangle screen_rect, unsigned int num_events, unsigned int max_rects) {
    unsigned int num_rects;
    InputEvent event_x;
    InputEvent event_y;
    unsigned int x, y;
    int x1, y1;
    unsigned int x2, y2;
    unsigned int i;

    if (max_rects == 0 || num_events == 0) {
        return 0;
    }

    num_rects = 0;

    for (i = 0; i < num_events - 1; i++) {
        event_x = events[i];
        event_y = events[i+1];

        if (
            event_x.type == EV_ABS && event_x.code == ABS_X &&
            event_y.type == EV_ABS && event_y.code == ABS_Y
        ) {
            // printf("paint: events_to_rects: event_x: %02x/%02x/%08x\n", event_x.type, event_x.code, event_x.value);
            // printf("paint: events_to_rects: event_y: %02x/%02x/%08x\n", event_y.type, event_y.code, event_y.value);

            x = event_x.value * screen_rect.width / 0x7fff;
            y = event_y.value * screen_rect.height / 0x7fff;

            x1 = x - BRUSH_SIZE / 2;
            x2 = x + BRUSH_SIZE / 2;
            y1 = y - BRUSH_SIZE / 2;
            y2 = y + BRUSH_SIZE / 2;

            if (x1 < 0) {
                x1 = 0;
            }

            if (x2 >= screen_rect.width) {
                x2 = screen_rect.width - 1;
            }

            if (y1 < 0) {
                y1 = 0;
            }

            if (y2 >= screen_rect.height) {
                y2 = screen_rect.height - 1;
            }

            rects[num_rects] = (VirtioGpuRectangle) {
                x1, y2,
                x2 - x1,
                y2 - y1
            };

            num_rects++;
            if (num_rects == max_rects) {
                return num_rects;
            }
        }
    }

    return num_rects;
}

void draw_line(VirtioGpuRectangle* from_rect, VirtioGpuRectangle* to_rect, VirtioGpuPixel* pixel) {
    int rise;
    int run;
    int abs_rise;
    int abs_run;

    while (from_rect->x != to_rect->x && from_rect->y != to_rect->y) {
        gpu_fill_and_flush(SCANOUT_ID, from_rect, pixel);

        rise = to_rect->y - from_rect->y;
        run = to_rect->x - from_rect->x;

        abs_rise = rise;
        if (rise < 0) {
            abs_rise = -rise;
        }

        abs_run = run;
        if (run < 0) {
            abs_run = -run;
        }

        if (abs_rise > abs_run) {
            run /= abs_run;
            rise /= abs_run;
        } else {
            run /= abs_rise;
            rise /= abs_rise;
        }

        from_rect->y += rise;
        from_rect->x += run;
    }

    gpu_fill_and_flush(SCANOUT_ID, to_rect, pixel);
}

void app_loop(VirtioGpuRectangle screen_rect) {
    InputEvent event_buffer[EVENT_BUF_SIZE];
    VirtioGpuRectangle rect_buffer[RECT_BUF_SIZE];
    VirtioGpuRectangle prev_rect;
    unsigned int num_events;
    unsigned int num_rects;
    unsigned int i;

    prev_rect.x = -1U;

    while (1) {
        num_events = get_events(event_buffer, EVENT_BUF_SIZE);
        num_rects = events_to_rects(event_buffer, rect_buffer, screen_rect, num_events, RECT_BUF_SIZE);

        for (i = 0; i < num_rects; i++) {
            if (prev_rect.x == -1U) {
                prev_rect = rect_buffer[i];
            }
            
            draw_line(&prev_rect, rect_buffer + i, &((VirtioGpuPixel) {255, 0, 0, 255}));

            prev_rect = rect_buffer[i];
        }
    }
}

int main() {
    VirtioGpuRectangle screen_rect;
    int rv;

    rv = init_screen(&screen_rect);
    if (rv != 0) {
        printf("paint: failed to init screen\n");
        return rv;
    }

    app_loop(screen_rect);

    return 0;
}
