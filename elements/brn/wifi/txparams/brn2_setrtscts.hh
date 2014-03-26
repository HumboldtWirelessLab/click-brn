#ifndef CLICK_BRN2_SETRTSCTS_HH
#define CLICK_BRN2_SETRTSCTS_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include "elements/brn/brnelement.hh"
#include <click/string.hh>
#include <click/hashtable.hh>
#include <click/etheraddress.hh>

#include "rtsctsschemes/rtscts_scheme.hh"

CLICK_DECLS

#define RTS_CTS_MIXED_STRATEGY_NONE        0 /* default */
#define RTS_CTS_MIXED_PS_AND_HN            1 /* combine packet size and hidden node */
#define RTS_CTS_MIXED_PS_AND_HN_AND_RANDOM 2 /* combine packet size and hidden node and random */

#define HEADER_AUTO  0
#define HEADER_WIFI  1
#define HEADER_ETHER 2
#define HEADER_ANNO  3


class Brn2_SetRTSCTS : public BRNElement {

 public:

  Brn2_SetRTSCTS();
  ~Brn2_SetRTSCTS();

  const char *class_name() const	{ return "Brn2_SetRTSCTS"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return AGNOSTIC;}

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  Packet *simple_action(Packet *);//Function for the Agnostic Port

  void add_handlers();

  String stats();

  void reset();

  uint32_t get_strategy() { return _rts_cts_strategy; }
  void set_strategy(uint32_t strategy) { _rts_cts_strategy = strategy; _scheme = get_rtscts_scheme(strategy); }

 private:

  RtsCtsScheme *_scheme;
  RtsCtsScheme *get_rtscts_scheme(uint32_t rts_cts_strategy);

  uint16_t _rts_cts_mixed_strategy; //use combination of different RTS-CTS Strategies
  uint16_t _rts_cts_strategy;       //RTS-CTS Strategy

  SchemeList _scheme_list;

  uint32_t _header;

  //total number of packets who got through this element
  uint32_t pkt_total;
  uint32_t rts_on;

  /** stats stuff */
  struct rtscts_neighbour_statistics {
     uint32_t pkt_total;
     uint32_t rts_on;
  };

  struct rtscts_neighbour_statistics nstats_dummy;
  struct rtscts_neighbour_statistics* _bcast_nstats;

  HashMap<EtherAddress, struct rtscts_neighbour_statistics> rtscts_neighbours;

  PacketInfo _pinfo;
};

CLICK_ENDDECLS
#endif
