#ifndef DHT_DB_HH
#define DHT_DB_HH
#include <click/element.hh>
#include <click/vector.hh>
#include "elements/brn/dht/md5.h"
#include <click/etheraddress.hh>

#define DB_INT 0
#define DB_ARRAY 1

CLICK_DECLS


class BRNDB
{

  enum {
    DATA_OK,
    DATA_INIT_MOVED,
    DATA_MODED,
    DATA_NEW,
    DATA_TIMEOUT
  };


  public:
    class DBrow {
      public:
        md5_byte_t *md5_key;

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

        int status;                    //status of the data (ok, moved, timeout)
        int move_id;                   //id use to move the data. Target ack the receive of the data, soure ack the deletiona of the data

        DBrow()
        {
          locked = false;
        }

        ~DBrow()
        {
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
    };

  public:

    //BRNDB();
    BRNDB();
    ~BRNDB();

    int insert(md5_byte_t *md5_key, uint8_t *key, uint16_t keylen, uint8_t *value, uint16_t valuelen);

    BRNDB::DBrow *getRow(char *key, uint16_t keylen);
    BRNDB::DBrow *getRow(md5_byte_t *md5_key);
    BRNDB::DBrow *getRow(int index);                      //TODO: use iterator

    int delRow(md5_byte_t *md5_key);

    int size();

  private:

    Vector<DBrow*> _table;

    int _debug;
};

CLICK_ENDDECLS
#endif
