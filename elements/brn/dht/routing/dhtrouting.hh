#ifndef DHT_ROUTING_HH
#define DHT_ROUTING_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include "elements/brn/dht/standard/dhtnode.hh"
#include "elements/brn/dht/standard/dhtnodelist.hh"

CLICK_DECLS

/*TODO:
 Notify_callbacks and objects should be lists, so that several objects can connect
*/

class DHTRouting : public Element
{
  public:
    DHTRouting();
    ~DHTRouting();

    virtual const char *dhtrouting_name() const = 0;

    virtual bool replication_support() const = 0;
    virtual int max_replication() const = 0;

    int set_notify_callback(void (*info_func)(void*,int), void *info_obj) {
      //click_chatter("Set notify");
      if ( ( info_func == NULL ) || ( info_obj == NULL ) ) return -1;
      else
      {
        _info_func = info_func;
        _info_obj = info_obj;
        return 0;
      }
    }

    void notify_callback(int status) {
      (*_info_func)(_info_obj,status);
    }

    bool is_me(EtherAddress *addr) { return ( *addr != _me->_ether_addr ); }
    bool is_me(uint8_t *ether_addr) { return ( memcmp(_me->_ether_addr.data(),ether_addr,6) == 0 ); }
//  virtual nodeaddress get_responsibly_node( md5 keyid );
//  virtual Vector<nodeaddress> get_all_responsibly_nodes( md5_keyid);

    void (*_info_func)(void*,int);
    void *_info_obj;

    DHTnode *_me;

};

CLICK_ENDDECLS
#endif
