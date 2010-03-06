#ifndef DHT_ROUTING_HH
#define DHT_ROUTING_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include "elements/brn2/dht/standard/dhtnode.hh"
#include "elements/brn2/dht/standard/dhtnodelist.hh"
#include "elements/brn2/standard/md5.h"

CLICK_DECLS

#define ROUTING_STATUS_NEW_NODE         1
#define ROUTING_STATUS_NEW_NEIGHBOUR    2
#define ROUTING_STATUS_NEW_CLOSE_NODE   4
#define ROUTING_STATUS_LOST_NODE        8
#define ROUTING_STATUS_LOST_NEIGHBOUR  16
#define ROUTING_STATUS_LOST_CLOSE_NODE 32


/*TODO:
 Notify_callbacks and objects should be lists, so that several objects can connect
*/

class DHTRouting : public Element
{
  public:

    class CallbackFunction {

      public:
        void (*_info_func)(void*, int);
        void *_info_obj;

        CallbackFunction( void (*info_func)(void*,int), void *info_obj ) {
          _info_func = info_func;
          _info_obj = info_obj;
        }

        ~CallbackFunction(){}
    };

    Vector<CallbackFunction*> _callbacklist;

    DHTRouting();
    ~DHTRouting();

    virtual const char *dhtrouting_name() const = 0; //const : function doesn't change the object (members).
                                                     //virtual: sp�te Bindung
    virtual bool replication_support() const = 0;
    virtual int max_replication() const = 0;

    virtual DHTnode *get_responsibly_node(md5_byte_t *key) = 0;
    virtual DHTnode *get_responsibly_replica_node(md5_byte_t *key, int replica_number) = 0;
    //virtual Vector<DHTnode *> get_all_responsibly_nodes( md5_keyid ) = 0;

    virtual int update_node(EtherAddress *ea, md5_byte_t *key, int keylen) = 0;

    int add_notify_callback(void (*info_func)(void*,int), void *info_obj)
    {
      if ( ( info_func == NULL ) || ( info_obj == NULL ) ) return -1;
      _callbacklist.push_back(new CallbackFunction(info_func, info_obj));
      return 0;
    }

    void notify_callback(int status) {
      CallbackFunction *cbf;
      for ( int i = 0; i < _callbacklist.size(); i++ ) {
        cbf = _callbacklist[i];

        (*cbf->_info_func)(cbf->_info_obj, status);
      }
    }

    bool is_me(EtherAddress *addr) { return ( *addr == _me->_ether_addr ); }
    bool is_me(uint8_t *ether_addr) { return ( memcmp(_me->_ether_addr.data(),ether_addr,6) == 0 ); }
    bool is_me(DHTnode *node) { return ( memcmp(_me->_ether_addr.data(),node->_ether_addr.data(),6) == 0 ); }

    DHTnode *_me;

};

CLICK_ENDDECLS
#endif
