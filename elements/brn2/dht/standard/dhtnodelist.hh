#ifndef BRN2_DHTNODELIST_HH
#define BRN2_DHTNODELIST_HH

#include <click/element.hh>
#include <click/vector.hh>
#include <click/etheraddress.hh>
#include "dhtnode.hh"

CLICK_DECLS
/**
 TODO: what's with add_dhtnode(): delete already include node and insert in the list again or only update existing entry ?

 Maybe it's possible to define different kind of tables to make sure that entry are not deleted
 German: gerade falcon hat problem (mehrere Tabellen). Für die Fingertablle wäre es besser wenn einträge mehrfach vorkommen könnten.
         Um weniger Speicher zu verbrauchen und die aktualisierung der Daten zu vereinfachen ist es ausserdem besser nur ein Object zu allokieren und in den Tabellen
         nur mit referenzen auf diese zu arbeiten.
*/
class DHTnodelist {

  public:
    DHTnodelist();
    ~DHTnodelist();

    int add_dhtnode(DHTnode *_new_node);
    DHTnode *swap_dhtnode(DHTnode *_node, int i);

    DHTnode* get_dhtnode(DHTnode *_search_node);
    DHTnode* get_dhtnode(EtherAddress *_etheradd);
    DHTnode* get_dhtnode(int i);

    int get_index_dhtnode(DHTnode *_search_node);

    void remove_dhtnode(int i);
    void remove_dhtnode(DHTnode *node);
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
    void clear(int start_index, int end_index);
    void del();                             //delete elements of list and clear

    bool contains(DHTnode *node);


  private:
    Vector<DHTnode*> _nodelist;

    //Hashmap<EtherAddress,DHTnode*) _node_ea_map;

    //Hashmap<NodeID,DHTnode*) _node_id_map;
};

CLICK_ENDDECLS
#endif
