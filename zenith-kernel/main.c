#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int main() {
    enable_framebuffer();
    enable_keyboard();

    while (1) {
        char line[100] = {};
        scanf(line);
        if (strcmp(line, "run-command")) {
            printf("yay\n");
        }
    }

    return 0;
}