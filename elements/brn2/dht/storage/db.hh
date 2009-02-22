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
  public:
    class DBrow {
      public:
        md5_byte_t *md5_key;

        uint8_t *value;
        uint16_t valuelen;

        uint8_t *key;
        uint16_t keylen;

        int lock;
        char lock_node[6];

        DBrow()
        {
        }

        ~DBrow()
        {
        }

    };

  public:

    //BRNDB();
    BRNDB();
    ~BRNDB();

    int insert(md5_byte_t *md5_key, uint8_t *key, uint16_t keylen, uint8_t *value, uint16_t valuelen, int lock, char *lock_node);

    BRNDB::DBrow *getRow(char *key, uint16_t keylen);
    BRNDB::DBrow *getRow(md5_byte_t *md5_key);

    int size();

  private:

    Vector<DBrow*> _table;

    int _debug;
};

CLICK_ENDDECLS
#endif
