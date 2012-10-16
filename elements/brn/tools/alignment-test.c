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
    int32_t test = 0x12345678;

    int32_t *p_test = &test;
    char *p_c_test = (char*)p_test;
    int32_t test_compare;
    int32_t test_a[] = { 0x12345678, 0x9abcdef0 };

    int32_t *test_b;
    test_b = (int32_t*)malloc(2 * sizeof(int32_t));
    test_b[0] = 0x12345678;
    test_b[1] = 0x9abcdef0;

    get_value(buf, 0, &value);
    for (i = 1; i < 4; i++) {
        get_value(buf, i, &try_value);
        printf("Check\n");
        if (value != try_value)
        exit(1);
    }

    p_c_test += 1;
    p_test = (int32_t*)p_c_test;

    printf("%x\n",test);
    test_compare = *p_test;

    printf("%x\n",test_compare);


   printf("Start: %x %x\n",test_a[0], test_a[1]);
   p_test = &test_a[0];
   p_c_test = (char*)p_test;
   for( i = 0; i < 4; i++ ) {
     p_c_test += 1;
     p_test = (int32_t*)p_c_test;
     test_compare = *p_test;

     printf("%d: %x\n",i,test_compare);
   }

   printf("Start_2: %x %x\n",test_b[0], test_b[1]);
   p_test = &test_b[0];
   p_c_test = (char*)p_test;
   for( i = 0; i < 4; i++ ) {
     p_c_test += 1;
     p_test = (int32_t*)p_c_test;
     test_compare = *p_test;

     printf("%d: %x\n",i,test_compare);
   }

    exit(0);
}


