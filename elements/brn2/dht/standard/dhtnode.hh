#ifndef BRN2_DHTNODE_HH
#define BRN2_DHTNODE_HH

#include <click/etheraddress.hh>
#include <clicknet/ether.h>

#include "md5.h"

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
    void set_age_now();
    int  get_age_s();

    void set_last_ping_s(int s);
    void set_last_ping(Timestamp *);
    void set_last_ping_now();
    int  get_last_ping_s();

};

CLICK_ENDDECLS
#endif
