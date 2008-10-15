#ifndef BRN2NetworkcodingELEMENT_HH
#define BRN2NetworkcodingELEMENT_HH

#include <click/element.hh>

CLICK_DECLS
/*
 * =c
 * BRN2Networkcoding()
 * =s
 * does nothing
 * =d
 */
class BRN2Networkcoding : public Element {

 public:
  //
  //methods
  //
  BRN2Networkcoding();
  ~BRN2Networkcoding();

  const char *class_name() const	{ return "BRN2Networkcoding"; }
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
