#ifndef CLICK_BRN2DSRPROTOCOL_HH
#define CLICK_BRN2DSRPROTOCOL_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>

CLICK_DECLS


class DSRProtocol : public Element { public:

  DSRProtocol();
  ~DSRProtocol();

  const char *class_name() const	{ return "DSRProtocol"; }

};

CLICK_ENDDECLS
#endif
