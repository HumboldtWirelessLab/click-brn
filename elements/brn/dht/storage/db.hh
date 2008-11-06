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
        EtherAddress lock_node;

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

  private:

    Vector<DBrow*> _rows;

    int _debug;
};

CLICK_ENDDECLS
#endif
