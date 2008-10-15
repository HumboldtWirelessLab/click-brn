#include "./elements/brn/md5.h"
#include "brn2_md5.hh"

CLICK_DECLS


static int BRN2_MD5::hexcompare(const md5_byte_t *sa, const md5_byte_t *sb)
{
  int len = sizeof(sa) > sizeof(sb) ? sizeof(sb): sizeof(sa);

  /* Sort numbers in the usual way, where -0 == +0.  Put NaNs after
     conversion errors but before numbers; sort them by internal
     bit-pattern, for lack of a more portable alternative.  */
  for (int i = 0; i < len; i++)
  {
    if (sa[i] > sb[i])
      return +1;
    if (sa[i] < sb[i])
      return -1;
  }
  return 0;
}

static void BRN2_MD5::calculate_md5(const char *msg, uint32_t msg_len, md5_byte_t *digest)
{
  md5_state_t state;

  MD5::md5_init(&state);
  MD5::md5_append(&state, (const md5_byte_t *)msg, msg_len); //strlen()
  MD5::md5_finish(&state, digest);
  
}

static void BRN2_MD5::printDigest(md5_byte_t *md5, char *hex_output)
{
  for (int di = 0; di < 16; ++di)
    sprintf(hex_output + di * 2, "%02x", md5[di]);
}

static void BRN2_MD5::printDigest(char *hex_output)
{
  return printDigest(md5_digest, hex_output);
}

CLICK_ENDDECLS
