#ifndef DHT_OPERATION_HH
#define DHT_OPERATION_HH
#include <click/element.hh>
#include <click/etheraddress.hh>

#include "elements/brn/standard/brn_md5.hh"

CLICK_DECLS

#define OPERATION_UNKNOWN 0
#define OPERATION_INSERT  1   /*BIT 0*/
#define OPERATION_REMOVE  2   /*BIT 1*/
#define OPERATION_READ    4   /*BIT 2*/
#define OPERATION_WRITE   8   /*BIT 3*/
#define OPERATION_LOCK    16  /*BIT 4*/
#define OPERATION_UNLOCK  32  /*BIT 5*/

/*Lock and unlock: this can be 1 bit. but then the locker must set the lock bit for each operation until he wants to unlock*/

#define OPERATION_REQUEST           0 /*BIT 7*/
#define OPERATION_REPLY           128 /*BIT 7*/
#define OPERATION_REPLY_REQUEST   128 /*mask to test whether it is a request or reply*/

#define DHT_OPERATION_ID_LOCAL_REPLY 0

#define DHT_STATUS_UNKNOWN            0
#define DHT_STATUS_OK                 1
#define DHT_STATUS_KEY_NOT_FOUND      2
#define DHT_STATUS_KEY_IS_LOCKED      3
#define DHT_STATUS_KEY_ALREADY_EXISTS 4
#define DHT_STATUS_TIMEOUT            5
#define DHT_STATUS_MAXRETRY           6

#define DHT_RETRIES_UNLIMITED -1
#define DHT_DURATION_UNLIMITED 0

/**
 * TODO: use replica as bitfield (max 8 replicas should be enough. replicas with the next equals hops can be pu together (split later)
 * Etheraddress is remove from operation-header. Now the src-etheraddr in DHT-header is used instead.
 * for locking, the src-etheraddr is used. Maybe its better to use some id (this should add to the header (lockid))
 */

struct DHTOperationHeader {
  uint16_t id;
  uint8_t replica;                          //bitmap of replica
  uint8_t hops;                             //for stats: hops in overlay-network
  uint8_t operation;
  uint8_t status;
  md5_byte_t key_digest[MD5_DIGEST_LENGTH];
  uint16_t keylen;
  uint16_t valuelen;

} __attribute__ ((packed));

#define SERIALIZE_STATIC_SIZE sizeof(struct DHTOperationHeader)

class DHTOperation {

  public:
    //TODO: build a struct for better/faster serialize

    struct DHTOperationHeader header;
    uint8_t *key;
    uint8_t *value;

    int max_retries;
    int retries;

    Timestamp request_time;
    uint32_t max_request_duration;
    uint32_t request_duration;

    bool digest_is_set;

    EtherAddress src_of_operation;  //this is set by storage layer, taken from src of dht-operation (see dhtprotocol-header)

    DHTOperation();
    ~DHTOperation();

    void insert(uint8_t *_key, uint16_t _keylen, uint8_t *_value, uint16_t _valuelen);
    void remove(uint8_t *key, uint16_t keylen);
    void read(uint8_t *key, uint16_t keylen);
    void write(uint8_t *key, uint16_t keylen, uint8_t *value, uint16_t valuelen);
    void write(uint8_t *key, uint16_t keylen, uint8_t *value, uint16_t valuelen, bool insert);
    void lock(uint8_t *key, uint16_t keylen);
    void unlock(uint8_t *key, uint16_t keylen);

    void set_lock(bool lock);

    void set_value(uint8_t *value, uint16_t valuelen);

    void set_key_digest(md5_byte_t *key_digest);
    void unset_key_digest();
    bool is_set_key_digest();

    void set_request();
    bool is_request();

    void set_reply();
    bool is_reply();

    void set_id(uint16_t _id);
    uint16_t get_id();
    void set_replica(uint8_t _replica);
    uint8_t get_replica();

    void set_status(uint8_t status);

    void set_src_address_of_operation(uint8_t *ea);

    int length();
    int serialize(uint8_t **buffer, uint16_t *len);
    int serialize_buffer(uint8_t *buffer, uint16_t maxlen);
    int unserialize(uint8_t *buffer, uint16_t len);
    static void inc_hops_in_header(uint8_t *buffer, uint16_t len);

  private:

};

CLICK_ENDDECLS
#endif
