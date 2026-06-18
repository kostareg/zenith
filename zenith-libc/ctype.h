bool isalpha(char c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

bool isdigit(char c) {
    return (c >= '0' && c <= '9');
}

bool isalnum(char c) {
    return (isalpha(c) || isdigit(c));
}