#include "os.h"
#include "printf.h"


#define SCANOUT_ID 0


int main() {
    VirtioGpuRectangle rect;
    int rv;

    rv = gpu_get_display_info(SCANOUT_ID, &rect);

    printf("paint: rv: %d\n", rv);
    if (rv == 0) {
        printf(
            "paint: x: %d, y: %d, w: %d, h: %d\n",
            rect.x, rect.y,
            rect.width, rect.height
        );
    }

    return rv;
}
