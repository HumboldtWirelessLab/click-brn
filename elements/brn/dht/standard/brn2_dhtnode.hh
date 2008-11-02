#ifndef BRN2_DHTNODE_HH
#define BRN2_DHTNODE_HH

#include <click/etheraddress.hh>
#include <clicknet/ether.h>

#include "elements/brn/dht/brn2_md5.hh"

CLICK_DECLS

#define STATUS_UNKNOWN 0
#define STATUS_NEW     1
#define STATUS_OK      2
#define STATUS_MISSED  3
#define STATUS_AWAY    4
#define STATUS_ALL     128
class DHTnode
{

  public:

    class MD5helper
    {
      public:

        static String convert_ether2hex(unsigned char *p)
        {
          char buf[24];
          sprintf(buf, "%02x%02x%02x%02x%02x%02x",
                  p[0], p[1], p[2], p[3], p[4], p[5]);
          return String(buf, 17);
        }

        static int hexcompare(const md5_byte_t *sa, const md5_byte_t *sb) {
          int len = sizeof(sa) > sizeof(sb) ? sizeof(sb): sizeof(sa);

          /* Sort numbers in the usual way, where -0 == +0.  Put NaNs after
            conversion errors but before numbers; sort them by internal
            bit-pattern, for lack of a more portable alternative.  */
          for (int i = 0; i < len; i++) {
            if (sa[i] > sb[i])
              return +1;
            if (sa[i] < sb[i])
              return -1;
          }
          return 0;
        }

        static void calculate_md5(const char *msg, uint32_t msg_len, md5_byte_t *digest)
        {
          md5_state_t state;

          MD5::md5_init(&state);
          MD5::md5_append(&state, (const md5_byte_t *)msg, msg_len); //strlen()
          MD5::md5_finish(&state, digest);

        }

        static void printDigest(md5_byte_t *md5, char *hex_output) {
          for (int di = 0; di < 16; ++di)
              sprintf(hex_output + di * 2, "%02x", md5[di]);
        }
    };

    md5_byte_t _md5_digest[16];
    EtherAddress _ether_addr;

    uint8_t _status;
    Timestamp _age;
    Timestamp _last_ping;
    int _failed_ping;
    bool  _neighbor;

    void *_extra;

    DHTnode() {};
    ~DHTnode() {};

    DHTnode(EtherAddress addr);
    DHTnode(EtherAddress addr, md5_byte_t *nodeid);

    void set_age_s(int s);
    void set_age(Timestamp *);
    int  get_age_s();

    void set_last_ping_s(int s);
    void set_last_ping(Timestamp *);
    int  get_last_ping_s();

};

CLICK_ENDDECLS
#endif
