#ifndef CLICK_PACKETLOSSINFORMATION_GRAPH_HH
#define CLICK_PACKETLOSSINFORMATION_GRAPH_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include "elements/brn/brnelement.hh"
#include "packetlossreason.hh"
#include <click/hashtable.hh>
#include <click/string.hh>

CLICK_DECLS

class PacketLossInformation_Graph{

  public:
    PacketLossInformation_Graph();
    ~PacketLossInformation_Graph();

    PacketLossReason* root_node_get();
    PacketLossReason* reason_get(String name);
    PacketLossReason* reason_get(PacketLossReason::PossibilityE id);

    typedef enum _PacketLossReason_ATTRIBUTE {
      FRACTION,
      LABEL,
      ID,
      ADDSTATS
    } ATTRIBUTE;

    /*
     "packet_loss"
     "interference"
     "channel_fading"
     "wifi"
     "non_wifi"
     "weak_signal"
     "shadowing"
     "multipath_fading"
     "co_channel"
     "adjacent_channel"
     "g_vs_b"
     "narrowband"
     "broadband"
     "in_range"
     "hidden_node"
     "narrowband_cooperative"
     "narrowband_non_cooperative"
     "broadband_cooperative"
     "broadband_non_cooperative"
    */

    void packetlossreason_set(String name,ATTRIBUTE feature, unsigned int value);
    void packetlossreason_set(String name,ATTRIBUTE feature, void *ptr);

    int overall_fract(PacketLossReason* ptr_node,int depth);
    void graph_build();
    String print();

  private:
    PacketLossReason *_root;
    HashTable<int,PacketLossReason*> map_poss_packetlossreson;
    PacketLossReason::PossibilityE possibility_get(String name);
};

CLICK_ENDDECLS
#endif
