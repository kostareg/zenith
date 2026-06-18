#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define KEYBOARD_STATUS_CONTROL 0xFFFFFFFFFFA00000ULL
#define KEYBOARD_KEY_EVENT 0xFFFFFFFFFFA00004ULL
#define KEYBOARD_FIFO_COUNT 0xFFFFFFFFFFA00008ULL

int main() {
    enable_framebuffer();

    uint32_t *control = KEYBOARD_STATUS_CONTROL;
    uint32_t *key_event = KEYBOARD_KEY_EVENT;
    uint32_t *fifo_count = KEYBOARD_FIFO_COUNT;

    *control = 0x8;

    while (1) {
        uint32_t status = *control;

        if (status & 0x1) {
            uint32_t event = *key_event;

            int values[4];
            values[0] = event & 0xFFFF;
            values[1] = (event >> 16) & 1;
            values[2] = (event >> 17) & 1;
            values[3] = *fifo_count;

            printf_ints("k %d p %d r %d R %d\n", values);
        }
    }

    return 0;
}