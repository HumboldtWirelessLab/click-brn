#ifndef BITFIELD_HH
#define BITFIELD_HH

CLICK_DECLS

class Bitfield {
 public:
  Bitfield(unsigned char* field, int len );
  ~Bitfield();

  void setBit(int position);
  void delBit(int position);

  void setValueOnBit(int position, unsigned char value);
  unsigned char getBit(int position);

  void setValue(int start, int end, int value);

  int getValue(int start, int end);

 private:
   unsigned char* _field;
   int _max_len;
};

CLICK_ENDDECLS
#endif
