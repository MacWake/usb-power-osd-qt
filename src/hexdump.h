static void hexdump(const char *buffer) {
    bool isEof = false;
    bool isEof2 = false;
    for (int i = 0;; i += 16) {
        printf("%04x:  ", i);
        for (int j = 0; j < 16; j++) {
            if (isEof) {
                printf("%2s ", " ");
            } else {
                printf("%02x ", buffer[i + j] & 0xff);
                if (buffer[i + j] == 0x00) {
                    isEof = true;
                }
            }
        }
        printf("%s", "  ");
        for (int j = 0; j < 16; j++) {
            if (isEof2) {
                printf("%s", " ");
            } else {
                printf("%c ", isprint(buffer[i + j]) ? buffer[i + j] : '.');
                if (buffer[i + j] == 0x00) {
                    isEof2 = true;
                }
            }
        }
        printf("\n");
        if (isEof) {
            break;
        }
    }
}
