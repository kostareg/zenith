bool strcmp(char* s1, char* s2) {
    while (*s1 && (*s1 == *s2)) {
        ++s1;
        ++s2;
    }

    return *s1 == *s2;
}

bool strncmp(char* s1, char* s2, uint16_t n) {
    if (n == 0) return true;

    while (n != 1 && *s1 && (*s1 == *s2)) {
        ++s1;
        ++s2;
        --n;
    }

    return *s1 == *s2;
}

uint16_t strlen(char* s) {
    uint16_t l = 0;
    while (*s) {
        ++l;
        ++s;
    }
    return l;
}