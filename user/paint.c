#include "os.h"
#include "event.h"
#include "printf.h"


#define SCANOUT_ID 0

#define BRUSH_SIZE 10

#define EVENT_BUF_SIZE 512

#define NUM_COLORS 6
#define COLORS_HEIGHT 50


typedef struct AppState {
    struct {
        uint32_t x_pos;
        uint32_t y_pos;
        int8_t clicking;
    } cursor;

    struct {
        uint32_t width;
        uint32_t height;
    } screen;

    struct {
        VirtioGpuRectangle rect;
        VirtioGpuRectangle prev_brush_rect;
    } canvas;

    struct {
        VirtioGpuRectangle rect;
        VirtioGpuPixel pixel;
    } colors[NUM_COLORS];
} State;


State app_state;


int app_init() {
    VirtioGpuRectangle screen_rect;
    int rv;
    int i;

    rv = gpu_get_display_info(SCANOUT_ID, &screen_rect);
    if (rv != 0) {
        return rv;
    }

    app_state.screen.width = screen_rect.width;
    app_state.screen.height = screen_rect.height;

    app_state.canvas.rect = (VirtioGpuRectangle) {
        0, COLORS_HEIGHT,
        screen_rect.width,
        screen_rect.height - COLORS_HEIGHT
    };

    rv = gpu_fill(SCANOUT_ID, &app_state.canvas.rect, &((VirtioGpuPixel) {255, 255, 255, 255}));
    if (rv != 0) {
        return rv;
    }

    app_state.colors[0].pixel = (VirtioGpuPixel) {255, 0, 0, 255};
    app_state.colors[1].pixel = (VirtioGpuPixel) {0, 255, 0, 255};
    app_state.colors[2].pixel = (VirtioGpuPixel) {0, 0, 255, 255};
    app_state.colors[3].pixel = (VirtioGpuPixel) {255, 255, 0, 255};
    app_state.colors[4].pixel = (VirtioGpuPixel) {255, 0, 255, 255};
    app_state.colors[5].pixel = (VirtioGpuPixel) {0, 255, 255, 255};

    for (i = 0; i < NUM_COLORS; i++) {
        app_state.colors[i].rect = (VirtioGpuRectangle) {
            screen_rect.width / NUM_COLORS * i,
            0,
            screen_rect.width / NUM_COLORS,
            COLORS_HEIGHT
        };

        rv = gpu_fill(SCANOUT_ID, &app_state.colors[i].rect, &app_state.colors[i].pixel);
        if (rv != 0) {
            return rv;
        }
    }

    rv = gpu_flush(SCANOUT_ID, &screen_rect);
    if (rv != 0) {
        return rv;
    }

    return 0;
}

void draw_line(VirtioGpuRectangle from_rect, VirtioGpuRectangle to_rect, VirtioGpuPixel pixel) {
    VirtioGpuRectangle flush_rect;
    int rise;
    int run;
    int abs_rise;
    int abs_run;

    if (from_rect.x < to_rect.x) {
        flush_rect.x = from_rect.x;
        flush_rect.width = to_rect.x - from_rect.x + to_rect.width;
    } else {
        flush_rect.x = to_rect.x;
        flush_rect.width = from_rect.x - to_rect.x + from_rect.width;
    }

    if (from_rect.y < to_rect.y) {
        flush_rect.y = from_rect.y;
        flush_rect.height = to_rect.y - from_rect.y + to_rect.height;
    } else {
        flush_rect.y = to_rect.y;
        flush_rect.height = from_rect.y - to_rect.y + from_rect.height;
    }

    while (from_rect.x != to_rect.x || from_rect.y != to_rect.y) {
        gpu_fill(SCANOUT_ID, &from_rect, &pixel);

        rise = to_rect.y - from_rect.y;
        run = to_rect.x - from_rect.x;

        abs_rise = rise;
        if (rise < 0) {
            abs_rise = -rise;
        }

        abs_run = run;
        if (run < 0) {
            abs_run = -run;
        }

        if (abs_rise > abs_run && abs_run != 0) {
            run /= abs_run;
            rise /= abs_run;

            abs_rise /= abs_run;
            abs_run = 1;
        } else if (abs_run >= abs_rise && abs_rise != 0) {
            run /= abs_rise;
            rise /= abs_rise;

            abs_run /= abs_rise;
            abs_rise = 1;
        }
        
        if (abs_rise > BRUSH_SIZE) {
            rise = rise / abs_rise * (BRUSH_SIZE - 1);
        }
        
        if (abs_run > BRUSH_SIZE) {
            run = run / abs_run * (BRUSH_SIZE - 1);
        }

        from_rect.y += rise;
        from_rect.x += run;
    }

    gpu_fill(SCANOUT_ID, &to_rect, &pixel);
    gpu_flush(SCANOUT_ID, &flush_rect);
}

int is_inside_rectangle(uint32_t x, uint32_t y, VirtioGpuRectangle* rect) {
    return (
        x >= rect->x &&
        x < rect->x + rect->width &&
        y >= rect->y &&
        y < rect->y + rect->height
    );
}

VirtioGpuRectangle get_brush_rect_from_pos() {
    int x, y;
    int x1, y1;
    int x2, y2;
    
    x = app_state.cursor.x_pos;
    y = app_state.cursor.y_pos;

    x1 = x - BRUSH_SIZE / 2;
    x2 = x + BRUSH_SIZE / 2;
    y1 = y - BRUSH_SIZE / 2;
    y2 = y + BRUSH_SIZE / 2;

    if (x1 < 0) {
        x1 = 0;
        x2 = BRUSH_SIZE;
    }

    if ((uint32_t) x2 >= app_state.screen.width) {
        x2 = app_state.screen.width - 1;
        x1 = x2 - BRUSH_SIZE;
    }

    if (y1 < 0) {
        y1 = 0;
        y2 = BRUSH_SIZE;
    }

    if ((uint32_t) y2 >= app_state.screen.height) {
        y2 = app_state.screen.height - 1;
        y1 = y2 - BRUSH_SIZE;
    }

    return (VirtioGpuRectangle) {
        x1, y1,
        x2 - x1,
        y2 - y1
    };
}

void app_canvas_handle_cursor_event() {
    VirtioGpuRectangle brush_rect;

    if (!app_state.cursor.clicking) {
        app_state.canvas.prev_brush_rect = (VirtioGpuRectangle) {
            -1U, -1U, -1U, -1U
        };

        return;
    }

    brush_rect = get_brush_rect_from_pos();

    if (
        app_state.canvas.prev_brush_rect.x == -1U &&
        app_state.canvas.prev_brush_rect.y == -1U &&
        app_state.canvas.prev_brush_rect.width == -1U &&
        app_state.canvas.prev_brush_rect.height == -1U
    ) {
        app_state.canvas.prev_brush_rect = brush_rect;
    }

    draw_line(app_state.canvas.prev_brush_rect, brush_rect, (VirtioGpuPixel) {255, 0, 0, 255});

    app_state.canvas.prev_brush_rect = brush_rect;
}

void app_handle_cursor_event(InputEvent event_x, InputEvent event_y) {
    app_state.cursor.x_pos = event_x.value * app_state.screen.width / 0x7fff;
    app_state.cursor.y_pos = event_y.value * app_state.screen.height / 0x7fff;

    app_canvas_handle_cursor_event();
}

void app_handle_click_event(InputEvent event) {
    app_state.cursor.clicking = event.value;
}

int app_handle_event(InputEvent event, InputEvent handle_event_buffer[], int n) {
    if (n == 0 && event.type == EV_ABS && event.code == ABS_X) {
        handle_event_buffer[n] = event;
        return 1;
    }

    if (n == 1 && event.type == EV_ABS && event.code == ABS_Y) {
        app_handle_cursor_event(handle_event_buffer[0], event);
    } else if (n == 0 && event.type == EV_KEY && event.code == BTN_LEFT) {
        app_handle_click_event(event);
    }

    return 0;
}

void app_loop() {
    InputEvent event_buffer[EVENT_BUF_SIZE];
    InputEvent handle_event_buffer[1];
    unsigned int num_events;
    int n;
    unsigned int i;

    n = 0;
    while (1) {
        num_events = get_events(event_buffer, EVENT_BUF_SIZE);

        for (i = 0; i < num_events; i++) {
            n = app_handle_event(event_buffer[i], handle_event_buffer, n);
        }
    }
}

int main() {
    int rv;

    rv = app_init();
    if (rv != 0) {
        printf("paint: failed to init screen\n");
        return rv;
    }

    app_loop();

    return 0;
}
