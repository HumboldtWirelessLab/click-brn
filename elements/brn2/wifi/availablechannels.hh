#ifndef CLICK_AVAILABLECHANNELS_HH
#define CLICK_AVAILABLECHANNELS_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
=c

AvailableChannels()

=s Wifi, Wireless Station, Wireless AccessPoint

=d


=h insert write-only

=h remove write-only

 */


class AvailableChannels : public Element { public:

  AvailableChannels();
  ~AvailableChannels();

  const char *class_name() const  { return "AvailableChannels"; }
  const char *port_count() const  { return PORTS_0_0; }

  int configure(Vector<String> &, ErrorHandler *);
  void *cast(const char *n);
  bool can_live_reconfigure() const  { return true; }

  void add_handlers();
  void take_state(Element *e, ErrorHandler *);

  Vector<int> _channels;

  int insert(int channel);
  int size() { return _channels.size(); };
  int get(int i);

  bool _debug;

};

CLICK_ENDDECLS
#endif
