#ifndef BRNVLANSETANNO_HH
#define BRNVLANSETANNO_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include "elements/brn2/vlan/brn2vlantable.hh"

CLICK_DECLS

class VlanSetAnno : public Element {

 public:
  //
  //methods
  //
  VlanSetAnno();
  ~VlanSetAnno();

  const char *class_name() const	{ return "VlanSetAnno"; }
  const char *port_count() const        { return "1/1"; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }

  Packet *smaction(Packet *);
  void push(int, Packet *);
  Packet *pull(int);

  BRN2VLANTable *_vlantable;
};

CLICK_ENDDECLS
#endif
