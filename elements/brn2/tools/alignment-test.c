/* confdefs.h.  */
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
void get_value(char *buf, int offset, int32_t *value) {
    int i;
    for (i = 0; i < 4; i++)
    buf[i + offset] = i;
    *value = *((int32_t *)(buf + offset));
}
int main(int argc, char *argv[]) {
    char buf[12];
    int i;
    int32_t value, try_value;
    get_value(buf, 0, &value);
    for (i = 1; i < 4; i++) {
        get_value(buf, i, &try_value);
        printf("Check");
        if (value != try_value)
        exit(1);
    }
    exit(0);
}


