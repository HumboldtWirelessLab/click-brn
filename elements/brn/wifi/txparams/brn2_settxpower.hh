#ifndef BRN_SETTXPOWER_HH
#define BRN_SETTXPOWER_HH
#include <click/element.hh>
#include <click/glue.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/routing/identity/brn2_device.hh"

CLICK_DECLS

/*
=c

BrnSetTXPower([I<KEYWORDS>])

=s Wifi

Sets the transmit power for a packet.

=d

Sets the Wifi TXPower Annotation on a packet.

Regular Arguments:

=over 8
=item POWER

Unsigned integer. from 0 to 63
=back 8

=h power
Same as POWER argument.

=a  SetTXRate
*/

class BrnSetTXPower : public BRNElement {
 public:

  BrnSetTXPower();
  ~BrnSetTXPower();

  const char *class_name() const		{ return "BrnSetTXPower"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action(Packet *);

  int set_power_iwconfig(const String &devname, int power, ErrorHandler *errh);
  String get_info();

  void add_handlers();
  unsigned _power;

 private:
  BRN2Device *_device;

};

CLICK_ENDDECLS
#endif
