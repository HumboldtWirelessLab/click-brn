#ifndef BRN2CRCERRORRECOVERYELEMENT_HH
#define BRN2CRCERRORRECOVERYELEMENT_HH

#include <click/element.hh>

CLICK_DECLS
/*
 * =c
 * BRN2CRCErrorRecory()
 * =s
 * does nothing
 * =d
 */
class BRN2CRCErrorRecory : public Element {

 public:
  //
  //methods
  //
  BRN2CRCErrorRecory();
  ~BRN2CRCErrorRecory();

  const char *class_name() const	{ return "BRN2CRCErrorRecory"; }
  const char *processing() const	{ return PULL; }

  const char *port_count() const  { return "-/1"; }

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
};

CLICK_ENDDECLS
#endif
