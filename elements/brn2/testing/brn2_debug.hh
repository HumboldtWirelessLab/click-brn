#ifndef BRN2DEBUG_HH
#define BRN2DEBUG_HH

#include <click/element.hh>

CLICK_DECLS

/*
 * =c
 * BRN2Debug()
 * =s
 * 
 * =d
 */

class BRN2Debug : public Element {

 public:
  BRN2Debug();
  ~BRN2Debug();
  //
  //methods
  //

  const char *class_name() const  { return "BRN2Debug"; }
  const char *processing() const  { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  int get_debug_level();

  int _debug;

 private:

};

CLICK_ENDDECLS
#endif
