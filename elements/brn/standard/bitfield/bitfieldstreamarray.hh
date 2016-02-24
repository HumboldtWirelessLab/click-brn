#ifndef BITFIELDSTREAMARRAY_HH
#define BITFIELDSTREAMARRAY_HH

#include "bitfield.hh"

CLICK_DECLS

class BitfieldStreamArray {
 public:
  BitfieldStreamArray(unsigned char* field, int len );
  BitfieldStreamArray(const BitfieldStreamArray &bfsa);
  ~BitfieldStreamArray();

  void writeBits( int value, int bits);
  int readBits(int bits);

  int get_position() { return _position; }
  bool eof() { return _position >= _max_len; }
  void reset() { _position = 0; }

 private:
   Bitfield *_bitfield;
   unsigned char* _field;
   int _max_len;
   int _position;
};

CLICK_ENDDECLS
#endif
