#ifndef FLOODINGPRENEGOTIATIONELEMENT_HH
#define FLOODINGPRENEGOTIATIONELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinkstat.hh"

#include "flooding_db.hh"
#include "floodinglinktable.hh"

CLICK_DECLS

/*
 * Linkinformation
 */
struct fooding_linkinformation {
  uint8_t src_ea[6]; //ids refers to this source
  uint8_t src_ids;   //ids include in this packet
  uint8_t rx_nodes;  //number of nodes, which rx infos is included
} CLICK_SIZE_PACKED_ATTRIBUTE;

/*
 * list of ids
 *
 * list of number of transmissions per id
 */

struct fooding_single_linkinformation {
  uint8_t node[6];
  uint8_t rx_pobability;
} CLICK_SIZE_PACKED_ATTRIBUTE;


#define FLOODING_PRENEGOTIATION_STARTTIME 100000 /*ms (100s)*/

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
  int lpReceiveHandler(char *buffer, int size);

  BRN2LinkStat *_linkstat;
  FloodingLinktable *_link_table;
  FloodingDB *_flooding_db;

};

CLICK_ENDDECLS
#endif
