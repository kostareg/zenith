int ipow(int base, int exp) {
    int result = 1;
    while (exp > 0) {
        if (exp & 1) {
            result *= base;
        }

        base *= base;
        exp >>= 1;
    }
    return result;
}