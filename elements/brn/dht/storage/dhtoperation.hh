#ifndef DHT_OPERATION_HH
#define DHT_OPERATION_HH
#include <click/element.hh>

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

#define DHT_STATUS_UNKNOWN    0
#define DHT_STATUS_OK         1

#define SERIALIZE_STATIC_SIZE sizeof(uint8_t)+sizeof(uint8_t)+sizeof(uint16_t)+sizeof(uint16_t)+sizeof(uint32_t)

class DHTOperation {

  public:
    //TODO: build a struct for better/faster serialize
    uint8_t operation;
    uint8_t status;
    uint32_t id;
    uint16_t keylen;
    uint16_t valuelen;
    char *key;
    char *value;

    DHTOperation();
    ~DHTOperation();

    void insert(char *_key, uint16_t _keylen, char *_value, uint16_t _valuelen);
    void remove(char *key, uint16_t keylen);
    void read(char *key, uint16_t keylen);
    void write(char *key, uint16_t keylen, char *value, uint16_t valuelen);
    void lock(char *key, uint16_t keylen);
    void unlock(char *key, uint16_t keylen);
    void set_request();
    void set_reply();
    bool is_request();
    bool is_reply();
    void set_id(uint32_t _id);
    uint32_t get_id();

    int serialize(char **buffer, uint16_t *len);
    int serialize_buffer(char *buffer, uint16_t maxlen);
    int unserialize(char *buffer, uint16_t len);

  private:

};

CLICK_ENDDECLS
#endif
