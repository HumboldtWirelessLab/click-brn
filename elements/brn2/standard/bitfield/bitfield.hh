#ifndef BITFIELD_HH
#define BITFIELD_HH

void setBit(unsigned char *field, int position) {
  field[position/8] |= 1 << (position%8);  
}

void delBit(unsigned char *field, int position) {
  field[position/8] &= ~((unsigned char)1 << (position%8));  
}

void setValueOnBit(unsigned char *field, int position, unsigned char value) {
  if ( value == 0 ) delBit(field, position);
  else if ( value == 1 ) setBit(field, position);
}

unsigned char getBit(unsigned char *field, int position) {
  return ( ( field[position/8] >> (position%8) ) & 1 );  
}

void setValue(unsigned char *field, int start, int end, int value) {
  int ac_v = value;
  for ( int i = start; i <= end; i++ ) {
  	setValueOnBit(field, i, (ac_v & 1));
  	ac_v = ac_v >> 1;
  } 
}

int getValue(unsigned char *field, int start, int end) {
  int ac_v = 0;
  for ( int i = end; i >= start; i-- ) {
  	ac_v = ac_v << 1;
  	ac_v |= getBit(field, i);
  } 
  return ac_v;
}

#endif
