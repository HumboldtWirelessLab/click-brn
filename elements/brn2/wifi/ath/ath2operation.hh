#ifndef CLICK_ATH2OPERATION_HH
#define CLICK_ATH2OPERATION_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include "ath2_desc.h"

CLICK_DECLS

class Ath2Operation : public Element { public:

  Ath2Operation();
  ~Ath2Operation();

  const char *class_name() const  { return "Ath2Operation"; }
  const char *processing() const  { return PUSH; }
  const char *port_count() const  { return "1/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  void push( int port, Packet *packet );

  void add_handlers();

  bool _debug;

  void set_channel(int channel);
};

CLICK_ENDDECLS
#endif
