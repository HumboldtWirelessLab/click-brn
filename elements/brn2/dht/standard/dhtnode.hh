#ifndef BRN2_DHTNODE_HH
#define BRN2_DHTNODE_HH

#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/timer.hh>

#include "elements/brn2/standard/md5.h"

CLICK_DECLS

/**
 * age and last ping are timestamps of last seen or rather last send ping
 * get_age_s and get_last_ping_s returns the second since the last ping pr last receive packet
 * set_age_s and set_last_ping_s set the value. the value in second will be substract from the time now and result is stored
 * this means that positive values are appears to the past, negative to the future
 *
 * TODO: change second to milisecond
*/

#define STATUS_UNKNOWN 0
#define STATUS_NEW     1
#define STATUS_OK      2
#define STATUS_MISSED  3
#define STATUS_AWAY    4
#define STATUS_ALL     128


#define DEFAULT_DIGEST_LENGTH  128 /* 16*8 */
#define MAX_DIGEST_LENGTH      128 /* 16*8 */

#define INVALID_NODE_ID        MAX_DIGEST_LENGTH + 1

#define MAX_NODEID_LENTGH 16



class DHTnode
{

  public:

    md5_byte_t _md5_digest[MAX_NODEID_LENTGH];
    int _digest_length;             //number of used BITS of _md5_digest

    EtherAddress _ether_addr;

    uint8_t _status;
    Timestamp _age;
    Timestamp _last_ping;
    int _failed_ping;
    bool _neighbor;

    int _hop_distance;
    int _rtt;                     //round trip time in ms

    void *_extra;

    DHTnode();
    ~DHTnode() {};

    DHTnode(EtherAddress addr);
    DHTnode(EtherAddress addr, md5_byte_t *nodeid);
    DHTnode(EtherAddress addr, md5_byte_t *nodeid, int digest_length);

    void init();

    void set_update_addr(uint8_t *ea);  //TODO: finds better name
    void set_etheraddress(uint8_t *ea);
    void set_nodeid(md5_byte_t *nodeid);
    void set_nodeid(md5_byte_t *nodeid, int digest_length);

    void get_nodeid(md5_byte_t *nodeid, uint8_t *digest_length);

    void set_age_s(int s);           //TODO: Is that used ??
    void set_age(Timestamp *);
    void set_age_now();
    int  get_age_s();                //TODO: change to ms (msec1())

    void set_last_ping_s(int s);
    void set_last_ping(Timestamp *);
    void set_last_ping_now();
    int  get_last_ping_s();

    String get_status_string();

    DHTnode *clone(void);
    bool equals(DHTnode *);
    bool equalsID(DHTnode *);
    bool equalsEtherAddress(DHTnode *n);

};

CLICK_ENDDECLS
#endif
