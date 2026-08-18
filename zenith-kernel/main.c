#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int main() {
    enable_framebuffer();
    enable_keyboard();

    while (1) {
        char line[100] = {};
        printf("$ ");
        scanf(line);
        if (strcmp(line, "")) {
            continue;
        } else if (strcmp(line, "test")) {
            printf("ran test command\n");
        } else {
            printf("command not found\n");
        }
    }

    return 0;
}