#ifndef PRINTCRCERRORELEMENT_HH
#define PRINTCRCERRORELEMENT_HH

#include <click/element.hh>

CLICK_DECLS
/*
 * =c
 * PrintCRCError()
 * =s
 * does nothing
 * =d
 */
class PrintCRCError : public Element {

 public:
  //
  //methods
  //
  PrintCRCError();
  ~PrintCRCError();

  const char *class_name() const	{ return "PrintCRCError"; }
  const char *processing() const	{ return AGNOSTIC; }

  const char *port_count() const  { return "1-2/1"; }

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
  uint32_t _offset;
  bool _analyse;     //analyse content instead of comparing with zero
  uint32_t _bits;    //sum several bits for printing
  uint32_t _pad;
};

CLICK_ENDDECLS
#endif
