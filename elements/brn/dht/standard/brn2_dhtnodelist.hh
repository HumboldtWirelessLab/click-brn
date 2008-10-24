#ifndef BRN2_DHTNODELIST_HH
#define BRN2_DHTNODELIST_HH

#include <click/element.hh>
#include <click/vector.hh>
#include <click/etheraddress.hh>
#include "brn2_dhtnode.hh"

CLICK_DECLS

class DHTnodelist {

  public:
    DHTnodelist();
    ~DHTnodelist();

    int add_dhtnode(DHTnode *_new_node);
    DHTnode* get_dhtnode(DHTnode *_search_node);
    DHTnode* get_dhtnode(EtherAddress *_etheradd);

    DHTnode* get_dhtnode(int i);

    int erase_dhtnode(EtherAddress *_etheradd);
    int size();
    void sort();
    void clear();

  private:
    Vector<DHTnode*> _nodelist;
};

CLICK_ENDDECLS
#endif
