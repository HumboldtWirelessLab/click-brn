#include <click/config.h>
#include "bitfield.hh"
#include "bitfieldstreamarray.hh"

CLICK_DECLS

BitfieldStreamArray::BitfieldStreamArray(unsigned char* field, int len ) {
  _field = field;
  _max_len = len;
  _position = 0;
  _bitfield = new Bitfield(field,len);
}

BitfieldStreamArray::~BitfieldStreamArray() {
  delete _bitfield;
}

void
BitfieldStreamArray::writeBits( int value, int bits) {
  _bitfield->setValue(_position, _position + bits - 1, value);
  _position += bits;
}

int
BitfieldStreamArray::readBits(int bits) {
  int result = _bitfield->getValue(_position, _position + bits - 1);
  _position += bits;
  return result;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(Bitfield)
ELEMENT_PROVIDES(BitfieldStreamArray)
