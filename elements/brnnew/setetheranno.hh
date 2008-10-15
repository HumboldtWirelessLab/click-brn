#ifndef CLICK_SETEHERANNO_HH
#define CLICK_SETEHERANNO_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
 * Removes an ethernet header from a given packet.
 */
class SetEtherAnno : public Element {

 public:
  //
  //methods
  //
  SetEtherAnno();
  ~SetEtherAnno();

  const char *class_name() const	{ return "SetEtherAnno"; }
  const char *port_count() const	{ return "1/1"; }
  const char *processing() const	{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }

  Packet * simple_action(Packet *p);
 
 private:
  
  unsigned _offset;
  
};

CLICK_ENDDECLS
#endif /* CLICK_SETEHERANNO_HH */
