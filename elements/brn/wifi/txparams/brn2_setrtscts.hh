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

  void set_strategy(uint16_t value) { _rts_cts_strategy = value; }
  uint16_t get_strategy() { return _rts_cts_strategy; }
  int parse_schemes(String s_schemes, ErrorHandler* errh);

 private:

  RtsCtsScheme *_pre_scheme;
  RtsCtsScheme *_scheme;
  
  Vector<RtsCtsScheme *> _schemes;

  uint16_t _rts_cts_pre_strategy; //RTS-CTS Strategy
  uint16_t _rts_cts_strategy;     //RTS-CTS Strategy

  RtsCtsScheme *get_rtscts_scheme(int rts_cts_strategy);

  //total number of packets who got through this element
  uint32_t pkt_total;
  uint32_t rts_on;

  struct rtscts_neighbour_statistics {
     uint32_t pkt_total;
     uint32_t rts_on;
  };

  struct rtscts_neighbour_statistics nstats_dummy;

  HashMap<EtherAddress, struct rtscts_neighbour_statistics> rtscts_neighbours;

};

CLICK_ENDDECLS
#endif
