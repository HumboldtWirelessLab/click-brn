#include <click/config.h>
#include "bitfield.hh"

CLICK_DECLS

Bitfield::Bitfield(unsigned char* field, int len ) {
  _field = field;
  _max_len = len;
}

Bitfield::~Bitfield() {
}

void Bitfield::setBit(int position) {
  _field[position/8] |= 1 << (position%8);
}

void
Bitfield::delBit(int position) {
  _field[position/8] &= ~((unsigned char)1 << (position%8));
}

void
Bitfield::setValueOnBit(int position, unsigned char value) {
  if ( value == 0 ) delBit(position);
  else if ( value == 1 ) setBit(position);
}

unsigned char
Bitfield::getBit(int position) {
  return ( ( _field[position/8] >> (position%8) ) & 1 );
}

void
Bitfield::setValue(int start, int end, int value) {
  int ac_v = value;
  for ( int i = start; i <= end; i++ ) {
    setValueOnBit( i, (ac_v & 1));
    ac_v = ac_v >> 1;
  }
}

int
Bitfield::getValue(int start, int end) {
  int ac_v = 0;
  for ( int i = end; i >= start; i-- ) {
    ac_v = ac_v << 1;
    ac_v |= getBit(i);
  }
  return ac_v;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(Bitfield)
