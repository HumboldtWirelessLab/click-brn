#ifndef BRN2_DHTNODE_HH
#define BRN2_DHTNODE_HH

#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/timer.hh>

#include "elements/brn/standard/brn_md5.hh"

CLICK_DECLS

/**
 * age and last ping are timestamps of last seen or rather last send ping
 * get_age_s and get_last_ping_s returns the second since the last ping pr last receive packet
 * set_age_s and set_last_ping_s set the value. the value in second will be substract from the time now and result is stored
 * this means that positive values are appears to the past, negative to the future
 *
 * TODO: change second to milisecond
*/

#define STATUS_UNKNOWN     0  /*Status is unknown*/
#define STATUS_NEW         1  /*status is new. I known the node, 'cause other nodes told me*/
#define STATUS_OK          2
#define STATUS_MISSED      3  /*Node doesn't reply*/
#define STATUS_AWAY        4  /*Node doesn't replay for long time. Neighboring node told me that there is no beacon*/
#define STATUS_LEAVE       5  /*node has/will leave the network*/
#define STATUS_NONEXISTENT 6  /*node doesn't exist. Use to handle request for node that doesn't exist*/

static String dht_node_status_string[] = { "unknown", "new", "ok", "missed", "away", "leave", "nonexist" };

#define DEFAULT_DIGEST_LENGTH  128 /* 16*8 Bits*/
#define MAX_DIGEST_LENGTH      128 /* 16*8 Bits*/

//TODO: use 0 for invalid
#define INVALID_NODE_ID        MAX_DIGEST_LENGTH + 1

#define MAX_NODEID_LENTGH    16   /*Bytes*/
#define MAX_NODEID_LENTGH_32  4   /*32 Bits*/

class DHTnode
{

  public:

    md5_byte_t _md5_digest[MAX_NODEID_LENTGH];
    int _digest_length;                        //number of used BITS of _md5_digest
    uint32_t *_md5_digest32;                   //TODO: use this for faster compare of nodeids

    EtherAddress _ether_addr;

    uint8_t _status;
    Timestamp _age;
    Timestamp _last_neighbouring_rx_msg;       //last time a direct msg (as neighbour) from this node is received
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

    void reset();                       //TODO: check need

    void set_update_addr(uint8_t *ea);  //Takes and sets etheraddr and calculates node_id (md5) TODO: finds better name
    void set_etheraddress(uint8_t *ea); //only sets node_id
    void set_nodeid(md5_byte_t *nodeid);//only sets node_id; len = max_len
    void set_nodeid(md5_byte_t *nodeid, int digest_length);

    void get_nodeid(md5_byte_t *nodeid, uint8_t *digest_length);

    void set_age_s(int s);           //TODO: Is that used ??
    void set_age(Timestamp *);
    void set_age_now();
    int  get_age_s(Timestamp *now = NULL);                //TODO: change to ms (msec1())
    Timestamp get_age();

    void set_last_ping_s(int s);
    void set_last_ping(Timestamp *);
    void set_last_ping_now();
    int  get_last_ping_s();
    Timestamp get_last_ping();

    String get_status_string();

    DHTnode *clone(void);
    bool equals(DHTnode *);
    bool equalsID(DHTnode *);
    bool equalsEtherAddress(DHTnode *n);

};

CLICK_ENDDECLS
#endif
