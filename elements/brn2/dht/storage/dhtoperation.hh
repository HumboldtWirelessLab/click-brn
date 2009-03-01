#ifndef DHT_OPERATION_HH
#define DHT_OPERATION_HH
#include <click/element.hh>
#include "elements/brn/dht/md5.h"

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

#define DHT_STATUS_UNKNOWN        0
#define DHT_STATUS_OK             1
#define DHT_STATUS_KEY_NOT_FOUND  2
#define DHT_STATUS_TIMEOUT        3

struct DHTOperationHeader {
  uint32_t id;
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

    DHTOperation();
    ~DHTOperation();

    void insert(uint8_t *_key, uint16_t _keylen, uint8_t *_value, uint16_t _valuelen);
    void remove(uint8_t *key, uint16_t keylen);
    void read(uint8_t *key, uint16_t keylen);
    void write(uint8_t *key, uint16_t keylen, uint8_t *value, uint16_t valuelen);
    void lock(uint8_t *key, uint16_t keylen);
    void unlock(uint8_t *key, uint16_t keylen);

    void set_value(uint8_t *value, uint16_t valuelen);
    void set_request();
    void set_reply();
    bool is_request();
    bool is_reply();
    void set_id(uint32_t _id);
    uint32_t get_id();
    void set_status(uint8_t status);

    int length();
    int serialize(uint8_t **buffer, uint16_t *len);
    int serialize_buffer(uint8_t *buffer, uint16_t maxlen);
    int unserialize(uint8_t *buffer, uint16_t len);

  private:

};

CLICK_ENDDECLS
#endif
