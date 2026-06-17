unsigned long int __zenith_libc_rand_next = 1;

int rand(void) {
    __zenith_libc_rand_next = __zenith_libc_rand_next * 1103515245 + 12345;
    return (__zenith_libc_rand_next / 65536) % 32768;
}

void srand(unsigned int seed) {
    __zenith_libc_rand_next = seed;
}