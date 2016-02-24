#ifndef DHT_DB_HH
#define DHT_DB_HH
#include <click/vector.hh>
#include <click/etheraddress.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/standard/brn_md5.hh"

#define DB_INT 0
#define DB_ARRAY 1

CLICK_DECLS

enum DBRowStatus{
  DATA_OK,
  DATA_INIT_MOVE,
  DATA_MOVED,
  DATA_TIMEOUT
};


struct db_row_header {
  uint16_t valuelen;
  uint16_t keylen;
  uint16_t lock_time;
  uint16_t lock_duration;
  uint16_t store_time;
  uint16_t store_duration;
  uint8_t lock_etheraddress[6];
  md5_byte_t md5_key[MD5_DIGEST_LENGTH];
  uint8_t replica;
  uint8_t reserved;
};

class BRNDB : public BRNElement {

  public:
    class DBrow {

      public:
        md5_byte_t md5_key[MD5_DIGEST_LENGTH];

        uint8_t *value;
        uint16_t valuelen;

        uint8_t *key;
        uint16_t keylen;

        bool locked;
        uint8_t lock_node[6];

        Timestamp lock_timestamp;
        uint32_t  max_lock_duration;

        Timestamp store_time;
        uint32_t store_timeout;

        DBRowStatus status;                    //status of the data (ok, moved, timeout)
        uint32_t move_id;              //id use to move the data. Target ack the receive of the data, soure ack the deletiona of the data

        uint8_t replica;

        DBrow():
          value(NULL),
          valuelen(0),
          key(NULL),
          keylen(0),
          locked(false),
          max_lock_duration(0),
          store_timeout(0),
          status(DATA_OK),
          move_id(0),
          replica(0)
        {
          memset(md5_key,0,sizeof(md5_key));
          memset(lock_node,0,sizeof(lock_node));
        }

        ~DBrow()
        {
          if ( value != NULL ) delete[] value;
          if ( key != NULL ) delete[] key;
        }

        bool lock(EtherAddress *ea, int lock_duration) {
          Timestamp now = Timestamp::now();
          if ( ( ! locked ) || ( memcmp(lock_node,ea->data(),6) == 0 ) || ( ( now - lock_timestamp ).sec() > (int)max_lock_duration ) ) {
            locked = true;
            lock_timestamp = now;
            max_lock_duration = lock_duration;
            memcpy(lock_node, ea->data(), 6);
            return true;
          }

          return false;
        }

        bool unlock( EtherAddress *ea) {
          if (( !locked ) || ( memcmp(lock_node,ea->data(),6) == 0 )) {
            locked = false;
            return true;
          }

          return false;
        }

        bool isLocked() {
          Timestamp now = Timestamp::now();
          return ( ( locked ) && ( ( now - lock_timestamp ).sec() <= (int)max_lock_duration ) );
        }

        bool isLocked(EtherAddress *ea) {
          return ( isLocked() && ( memcmp(lock_node,ea->data(),6) != 0 ) );
        }

        EtherAddress getLockNode() {
          return ( EtherAddress(lock_node) );
        }

        void setStoreTime() {
          store_time = Timestamp::now();
        }

        void setStoreTimeOut(int _timeout) {
          store_timeout = _timeout;
        }

        bool isValid() {
          Timestamp now = Timestamp::now();
          return ( ( now - store_time ).sec() <= (int)store_timeout );
        }

        uint16_t serializeSize() {
          return sizeof(struct db_row_header) + valuelen + keylen;
        }

        int serialize(uint8_t *buffer, int max) {
          if ( serializeSize() > max ) return 0;
          struct db_row_header *rh;
          uint8_t *data;

          rh = (struct db_row_header *)buffer;

          rh->valuelen = htons(valuelen);
          rh->keylen = htons(keylen);
          rh->lock_time = 0;
          rh->lock_duration = 0;
          rh->store_time = 0;
          rh->store_duration = 0;
          rh->replica = replica;
          if (locked) memcpy(rh->lock_etheraddress,lock_node,6);
          else memset(rh->lock_etheraddress,0,6);

          memcpy(rh->md5_key, md5_key, MD5_DIGEST_LENGTH);

          data = &(buffer[sizeof(struct db_row_header)]);
          memcpy(data,key,keylen);
          memcpy(&(data[keylen]),value,valuelen);

          return serializeSize();
        }

        int unserialize(uint8_t *buffer, int size) {

          struct db_row_header *rh;
          uint8_t *data;

          if ( (uint32_t)size < sizeof(struct db_row_header)) return 0;

          rh = (struct db_row_header *)buffer;

          valuelen = ntohs(rh->valuelen);
          keylen = ntohs(rh->keylen);
          //rh->lock_time = 0;
          //rh->lock_duration = 0;
          //rh->store_time = 0;
          //rh->store_duration = 0;
          replica = rh->replica;
          //if (locked) memcpy(rh->lock_etheraddress,lock_node,6);
          //else memset(rh->lock_etheraddress,0,6);

          data = &(buffer[sizeof(struct db_row_header)]);
          key = new uint8_t[keylen];
          value = new uint8_t[valuelen];
          memcpy(key,data,keylen);
          memcpy(value,&(data[keylen]),valuelen);

          memcpy(md5_key, rh->md5_key, MD5_DIGEST_LENGTH);

          return serializeSize();
        }
    };

  public:

    //BRNDB();
    BRNDB();
    ~BRNDB();

    /*ELEMENT*/
    const char *class_name() const  { return "BRNDB"; }

    const char *processing() const  { return AGNOSTIC; }

    const char *port_count() const  { return "0/0"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void add_handlers();


    int insert(md5_byte_t *md5_key, uint8_t *key, uint16_t keylen, uint8_t *value, uint16_t valuelen, uint8_t replica=0);
    int insert(BRNDB::DBrow *row);

    BRNDB::DBrow *getRow(char *key, uint16_t keylen);
    BRNDB::DBrow *getRow(md5_byte_t *md5_key);
    BRNDB::DBrow *getRow(int index);                      //TODO: use iterator

    int delRow(md5_byte_t *md5_key);
    int delRow(int index);

    int size();

  private:

    Vector<DBrow*> _table;
};

CLICK_ENDDECLS
#endif
