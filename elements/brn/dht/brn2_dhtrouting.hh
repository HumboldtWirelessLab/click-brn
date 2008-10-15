#ifndef DHT_ROUTING_HH
#define DHT_ROUTING_HH
#include <click/element.hh>
#include <click/etheraddress.hh>

CLICK_DECLS

class DHTRouting : public Element
{
  public:
    DHTRouting();
    virtual ~DHTRouting();

    virtual const char *dhtrouting_name() const = 0;

    virtual bool replication_support() const = 0;
    virtual int max_replication() const = 0;

    virtual int set_notify_callback(void *info_func, void *info_obj) { return 0; };
    virtual bool is_me(EtherAddress *addr) {return true; };
//  virtual nodeaddress get_responsibly_node( md5 keyid );
//  virtual Vector<nodeaddress> get_all_responsibly_nodes( md5_keyid);

};

CLICK_ENDDECLS
#endif
