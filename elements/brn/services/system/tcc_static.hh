#ifndef TCC_STATIC_HH
#define TCC_STATIC_HH

void *tcc_packet_resize(void *, int, int);
int tcc_packet_size(void *);
const uint8_t *tcc_packet_data(void *);
void tcc_packet_kill(void *);

#endif
