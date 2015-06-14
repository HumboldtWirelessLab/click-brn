#ifndef FLOODINGPRENEGOTIATIONELEMENT_HH
#define FLOODINGPRENEGOTIATIONELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinkstat.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"

#include "flooding_db.hh"

CLICK_DECLS

/*
 * Linkinformation
 */
struct fooding_link_information {
  uint8_t src_ea[6];   //ids refers to this source
  uint8_t no_rx_nodes; //number of nodes, which rx infos is included
  uint8_t no_src_ids;  //ids include in this packet
  uint16_t sum_transmissions;
} CLICK_SIZE_PACKED_ATTRIBUTE;

/*
 * list of ids  (uint16_t)
 *
 * list of macs (uint8_t[6])
 *
 * matrix (no_src_ids x no_rx_nodes) with rx probability
 */

#define DEFAULT_MAX_FLOODING_IDS_PER_PACKET 8
#define DEFAULT_FLOODING_NEIGHBOURS_TIMEOUT 5000

typedef HashMap<EtherAddress, Timestamp> NodeAgeMap;
typedef NodeAgeMap::const_iterator NodeAgeMapIter;

class FloodingPrenegotiation : public BRNElement {

 public:
  //
  //methods
  //
  FloodingPrenegotiation();
  ~FloodingPrenegotiation();

  const char *class_name() const  { return "FloodingPrenegotiation"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "0/0"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  int lpSendHandler(char *buffer, int size);
  int lpReceiveHandler(EtherAddress *ea, char *buffer, int size);

  BRN2LinkStat *_linkstat;
  Brn2LinkTable *_link_table;
  FloodingDB *_flooding_db;

  NodeAgeMap _neighbour_map;

  uint32_t _seq;

  uint32_t _max_ids_per_packet;
  uint32_t _node_timeout;

};

CLICK_ENDDECLS
#endif
