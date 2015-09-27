#ifndef BASE64_HH
#define BASE64_HH

#include <click/vector.hh>

CLICK_DECLS

class Base64
{
 public:

  Base64();
  ~Base64();

  static int encode(unsigned char *input, int inputlen, unsigned char *encoded, int max_encodedlen);
  static int decode(unsigned char *encoded, int encodedlen, unsigned char *decoded, int max_decodedlen);

  static void base64_test();

 private:

  //int _debug;
};

CLICK_ENDDECLS
#endif
