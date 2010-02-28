#ifndef BRN2_DHTNODELIST_HH
#define BRN2_DHTNODELIST_HH

#include <click/element.hh>
#include <click/vector.hh>
#include <click/etheraddress.hh>
#include "dhtnode.hh"

CLICK_DECLS

class DHTnodelist {

  public:
    DHTnodelist();
    ~DHTnodelist();

    int add_dhtnode(DHTnode *_new_node);
    DHTnode *swap_dhtnode(DHTnode *_node, int i);

    DHTnode* get_dhtnode(DHTnode *_search_node);
    DHTnode* get_dhtnode(EtherAddress *_etheradd);
    DHTnode* get_dhtnode(int i);

    void remove_dhtnode(int i);
    int erase_dhtnode(EtherAddress *_etheradd);

    DHTnode* get_dhtnode_oldest_age();
    DHTnode* get_dhtnode_oldest_ping();

    DHTnodelist *get_dhtnodes_oldest_age(int number);
    DHTnodelist *get_dhtnodes_oldest_ping(int number);

    int size();
    void sort();
    void sort_last_ping();
    void sort_age();
    void clear();
    void del();                             //delete elements of list and clear

    bool contains(DHTnode *node);


  private:
    Vector<DHTnode*> _nodelist;
};

CLICK_ENDDECLS
#endif
