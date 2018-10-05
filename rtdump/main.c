#include <endian.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char g_zero[20] = {0};

static void format_rt(const char buf[32]) {

    // static uint32_t null_ctr = 0;

    if (0 == memcmp(buf, g_zero, 20)) {
        // null_ctr += 1;
        return;
    }

    // if (null_ctr > 0) {
    //     printf("NULL x %d\n", null_ctr);
    //     null_ctr = 0;
    // }

    for (uint32_t ix = 0; ix < 20; ix += 4) {
        printf("%08X", be32toh(*(uint32_t *)&buf[ix]));
    }

    char ip_buf[17];
    char port_buf[6];

    sprintf(ip_buf, " %u.%u.%u.%u", (uint8_t)buf[20], (uint8_t)buf[21],
            (uint8_t)buf[22], (uint8_t)buf[23]);
    sprintf(port_buf, "%-hu", be16toh(*(uint16_t *)&buf[24]));

    printf("%17s:%-5s", ip_buf, port_buf);

    uint8_t qual = (buf[25] & 0xe0) >> 5;

    printf(" %d", qual);
    puts("");
    return;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: rtdump [RT_FILE]");
        goto die;
    }

    FILE *f_rt = fopen(argv[1], "r");
    if (!f_rt) {
        fprintf(stderr, "Unable to open rt file for reading!");
        goto die;
    }

    char buf[32];
    size_t read;

    for (int ix = 0; ix < 256 * 256 * 256; ix++) {
        read = fread(buf, 1, 32, f_rt);
        if (read != 32) {
            return 0;
        }
        format_rt(buf);
    }

    return 0;
die:
    return -1;
}
