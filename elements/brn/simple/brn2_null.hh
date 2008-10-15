#ifndef BRN2NULLELEMENT_HH
#define BRN2NULLELEMENT_HH

#include <click/element.hh>

CLICK_DECLS
/*
 * =c
 * BRN2Null()
 * =s
 * does nothing
 * =d
 */
class BRN2Null : public Element {

 public:
  //
  //methods
  //
  BRN2Null();
  ~BRN2Null();

  const char *class_name() const	{ return "BRN2Null"; }
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
};

CLICK_ENDDECLS
#endif
