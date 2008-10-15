#ifndef BRN2CRCERRORELEMENT_HH
#define BRN2CRCERRORELEMENT_HH

#include <click/element.hh>

CLICK_DECLS
/*
 * =c
 * BRN2CRCerror()
 * =s
 * does nothing
 * =d
 */
class BRN2CRCerror : public Element {

 public:
  //
  //methods
  //
  BRN2CRCerror();
  ~BRN2CRCerror();

  const char *class_name() const	{ return "BRN2CRCerror"; }
  const char *processing() const	{ return AGNOSTIC; }

  const char *port_count() const  { return "1/1"; } 

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }

  Packet *simple_action(Packet *);

  int initialize(ErrorHandler *);
  void add_handlers();

 private:
  //
  //member
  //
  String _label;
  uint32_t _rate;
};

CLICK_ENDDECLS
#endif
