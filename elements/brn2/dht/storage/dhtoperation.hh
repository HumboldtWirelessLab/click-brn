#ifndef DHT_OPERATION_HH
#define DHT_OPERATION_HH
#include <click/element.hh>
#include "elements/brn2/standard/md5.h"

CLICK_DECLS

#define OPERATION_UNKNOWN 0
#define OPERATION_INSERT  1
#define OPERATION_REMOVE  2
#define OPERATION_READ    4
#define OPERATION_WRITE   8
#define OPERATION_LOCK    16
#define OPERATION_UNLOCK  32

#define OPERATION_REQUEST 0
#define OPERATION_REPLY   128
#define OPERATION_REPLY_REQUEST   128

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

struct DHTOperationHeader {
  uint16_t id;
  uint8_t replica;
  uint8_t reserved;
  uint8_t operation;
  uint8_t status;
  uint8_t etheraddress[6];
  md5_byte_t key_digest[16];
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

    DHTOperation();
    ~DHTOperation();

    void insert(uint8_t *_key, uint16_t _keylen, uint8_t *_value, uint16_t _valuelen);
    void remove(uint8_t *key, uint16_t keylen);
    void read(uint8_t *key, uint16_t keylen);
    void write(uint8_t *key, uint16_t keylen, uint8_t *value, uint16_t valuelen);
    void write(uint8_t *key, uint16_t keylen, uint8_t *value, uint16_t valuelen, bool insert);
    void lock(uint8_t *key, uint16_t keylen);
    void unlock(uint8_t *key, uint16_t keylen);

    void set_value(uint8_t *value, uint16_t valuelen);
    void set_request();
    void set_reply();
    bool is_request();
    bool is_reply();
    void set_id(uint16_t _id);
    uint16_t get_id();
    void set_replica(uint8_t _replica);
    uint8_t get_replica();
    void set_status(uint8_t status);

    int length();
    int serialize(uint8_t **buffer, uint16_t *len);
    int serialize_buffer(uint8_t *buffer, uint16_t maxlen);
    int unserialize(uint8_t *buffer, uint16_t len);

  private:

};

CLICK_ENDDECLS
#endif
