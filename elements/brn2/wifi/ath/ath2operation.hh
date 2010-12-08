#ifndef CLICK_ATH2OPERATION_HH
#define CLICK_ATH2OPERATION_HH
#include <click/element.hh>
#include <clicknet/ether.h>

#include "elements/brn2/brnelement.hh"
#include "ath2_desc.h"

CLICK_DECLS

class Ath2Operation : public BRNElement {
 public:

  Ath2Operation();
  ~Ath2Operation();

  const char *class_name() const  { return "Ath2Operation"; }
  const char *processing() const  { return PUSH; }
  const char *port_count() const  { return "1/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  void push( int port, Packet *packet );

  void add_handlers();

  void set_channel(int channel);

  void read_config();
  String madwifi_config();

  uint8_t channel;       //channel to set
  uint8_t cu_pkt_threshold; //channel utility: rx time
  uint8_t cu_update_mode;   //channel utility: tx time
  uint8_t cu_anno_mode;     //channel utility: rx time

  uint8_t cw_min[4];
  uint8_t cw_max[4];
  uint8_t aifs[4];

  uint8_t cca_threshold;

  uint32_t driver_flags;
};

CLICK_ENDDECLS
#endif
