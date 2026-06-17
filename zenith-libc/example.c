int add(int a, int b) {
    return a + b;
}

int relu(float x) {
    return (x > 0.0f) ? x : 0.0f;
}

#define __ZENITH_LIBC_FB_WIDTH 1920
#define __ZENITH_LIBC_FB_HEIGHT 1080
#define __ZENITH_LIBC_FB_PIXEL_BASE 0xffffffffffa11400ULL
#define __ZENITH_LIBC_FB_CONTROL_BASE 0xffffffffffa01400ULL

void enable_framebuffer() {
    long *control = __ZENITH_LIBC_FB_CONTROL_BASE;
    *control = 2L << 32;
}

void __zenith_libc_set_framebuffer_byte(long byte_offset) {
    long byte_in_word = byte_offset % 8;
    long aligned_offset = byte_offset - byte_in_word;
    long shift = byte_in_word * 8;
    long mask = 255L << shift;

    long *word = __ZENITH_LIBC_FB_PIXEL_BASE + aligned_offset;
    *word = *word | mask;
}

void __zenith_libc_put_white_pixel(int x, int y) {
    if (x < 0) return;
    if ((x - __ZENITH_LIBC_FB_WIDTH) >= 0) return;
    if (y < 0) return;
    if ((y - __ZENITH_LIBC_FB_HEIGHT) >= 0) return;

    long pixel_offset = ((y * __ZENITH_LIBC_FB_WIDTH) + x) * 3;

    __zenith_libc_set_framebuffer_byte(pixel_offset + 0);
    __zenith_libc_set_framebuffer_byte(pixel_offset + 1);
    __zenith_libc_set_framebuffer_byte(pixel_offset + 2);
}

void draw_white_line(int x0, int y0, int x1, int y1) {
    int dx = x1 - x0;
    int dy = y1 - y0;

    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;

    int sx = -1;
    int sy = -1;

    if ((x0 - x1) < 0) sx = 1;
    if ((y0 - y1) < 0) sy = 1;

    int error = dx - dy;

    while (1) {
        __zenith_libc_put_white_pixel(x0, y0);

        if ((x0 - x1) == 0) {
            if ((y0 - y1) == 0) return;
        }

        int doubled_error = error * 2;

        if ((doubled_error + dy) > 0) {
            error = error - dy;
            x0 = x0 + sx;
        }

        if ((doubled_error - dx) < 0) {
            error = error + dx;
            y0 = y0 + sy;
        }
    }
}

// int main() {
//     enable_framebuffer();
//     draw_white_line(0, 0, 100, 100);
//     return 7;
// }

static unsigned long int next = 1;

int rand(void) {
    // RAND_MAX is assumed to be 32767 for this specific implementation
    next = next * 1103515245 + 12345;
    return (unsigned int)(next / 65536) % 32768;
}

void srand(unsigned int seed) {
    next = seed;
}

int strcmp(char *s1, char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

int max_of_three(int a, int b, int c) {
    int max = a;

    if (b > max) {
        max = b;
    }
    if (c > max) {
        max = c;
    }

    return max;
}

int min_of_three(int a, int b, int c) {
    int min = a;       // Start by assuming 'a' is the smallest

    if (b < min) {     // If 'b' is smaller, update 'min'
        min = b;
    }
    if (c < min) {     // If 'c' is smaller, update 'min'
        min = c;
    }

    return min;        // Return the smallest value
}

// Approximates the square root of a non-negative number
double simple_sqrt(double x) {
    // Handle edge cases explicitly
    if (x <= 0.0) {
        return 0.0;
    }

    // A reasonable initial guess is half of the number
    double guess = x / 2.0; 
    double last_guess;

    // Fixed loop for guaranteed, fast O(1) performance
    // 10 to 15 iterations are typically enough for double precision
    for (int i = 0; i < 10; i++) {
        last_guess = guess;
        guess = 0.5 * (guess + x / guess);
        
        // Optional early exit if the precision goal is reached
        if (guess == last_guess) {
            break;
        }
    }

    return guess;
}

int isalpha(int c) {
    return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
}

int isalpha_switch(int c) {
    switch (c) {
        // Uppercase cases
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
        case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
        case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
        case 'V': case 'W': case 'X': case 'Y': case 'Z':
        // Lowercase cases
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
        case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
        case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
        case 'v': case 'w': case 'x': case 'y': case 'z':
            return 1; // Return non-zero if it matches any letter
        default:
            return 0; // Return zero for all other inputs
    }
}