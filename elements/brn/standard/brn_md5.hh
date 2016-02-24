#ifndef MD5_HH
#define MD5_HH
#include <click/config.h>
#include <click/md5.h>
#include <click/straccum.hh>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

CLICK_DECLS

#define MD5_DIGEST_LENGTH 16

class MD5 {

 public:

  MD5();
  ~MD5();

  /* Initialize the algorithm. */
  static void init(md5_state_t *pms)
  {
    md5_init(pms);
  }

  /* Append a string to the message. */
  static void append(md5_state_t *pms, const md5_byte_t *data, int nbytes)
  {
    md5_append(pms, data, nbytes);
  }

  /* Finish the message and return the digest. */
  static void finish(md5_state_t *pms, md5_byte_t digest[16])
  {
    md5_finish(pms, digest);
  }

  inline static int hexcompare(const md5_byte_t *sa, const md5_byte_t *sb)
  {
  /* Sort numbers in the usual way, where -0 == +0.  Put NaNs after
    conversion errors but before numbers; sort them by internal
    bit-pattern, for lack of a more portable alternative.  */
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
    {
      if (sa[i] > sb[i])
        return +1;
      if (sa[i] < sb[i])
        return -1;
    }
    return 0;
  }

  inline static bool is_equals(const md5_byte_t *sa, const md5_byte_t *sb)
  {
    uint32_t *sa32 = (uint32_t*)sa;
    uint32_t *sb32 = (uint32_t*)sb;
    return ( (sa32[0]==sb32[0]) && (sa32[1]==sb32[1]) && (sa32[2]==sb32[2]) && (sa32[3]==sb32[3]));
  }

  inline static bool is_smaller(const md5_byte_t *sa, const md5_byte_t *sb)
  {
    return (MD5::hexcompare(sa, sb) == -1);
  }

  inline static bool is_bigger(const md5_byte_t *sa, const md5_byte_t *sb)
  {
    return (MD5::hexcompare(sa, sb) == 1);
  }

  inline static bool is_smaller_or_equals(const md5_byte_t *sa, const md5_byte_t *sb)
  {
    return (MD5::hexcompare(sa, sb) <= 0);
  }

  inline static bool is_bigger_or_equals(const md5_byte_t *sa, const md5_byte_t *sb)
  {
    return (MD5::hexcompare(sa, sb) >= 0);
  }

  static void calculate_md5(const char *msg, uint32_t msg_len, md5_byte_t *digest)
  {
    md5_state_t state;

    init(&state);
    append(&state, (const md5_byte_t *)msg, msg_len); //strlen()
    finish(&state, digest);

  }

  static void printDigest(const md5_byte_t *md5, char *hex_output)
  {
    assert((md5) && (hex_output));
    for (int di = 0; di < 16; ++di)
      sprintf(hex_output + di * 2, "%02x", md5[di]);
  }

  static String convert_ether2hex(const unsigned char *p)
  {
    char buf[24];
    sprintf(buf, "%02x%02x%02x%02x%02x%02x",
            p[0], p[1], p[2], p[3], p[4], p[5]);
    return String(buf, 17);
  }

  /*static void inc()
  {
    int overflow = 0;
    for ( int i = MD5_DIGEST_LENGTH; i >= 0; i-- ) {
    }
  }*/

  static void digestFromString(md5_byte_t *md5, const char *hex_input)
  {
    char ac_dig[3];
    ac_dig[2] = '\0';
    uint ac_int_dig;

    for (int di = 0; di < 16; di++) {
      memcpy(ac_dig,&(hex_input[di << 1]),2);
      sscanf(ac_dig,"%02x",&ac_int_dig);
      md5[di] = ac_int_dig;
    }
  }
};

CLICK_ENDDECLS
#endif
