#ifndef BRN2_MD5_HH
#define BRN2_MD5_HH

#include "md5.h"

CLICK_DECLS

class BRN2_MD5 {

  BRN2_MD5()
  {}

  ~BRN2_MD5()
  {}

 public:
  static int hexcompare(const md5_byte_t *sa, const md5_byte_t *sb);
  static void calculate_md5(const char *msg, uint32_t msg_len, md5_byte_t *digest);
  static void printDigest(md5_byte_t *md5, char *hex_output);
  static void printDigest(char *hex_output);

};

CLICK_ENDDECLS
#endif
