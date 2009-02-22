#ifndef BRNCANDIDATESELECTORTESTELEMENT_HH
#define BRNCANDIDATESELECTORTESTELEMENT_HH

#include <click/element.hh>
#include <click/timer.hh>

CLICK_DECLS

/*
 * =c
 * BRNCandidateSelectorTest()
 * =s
 * SEND Packet with given size
 * =d
 */

class BRNCandidateSelectorTest : public Element {

 public:

   int _debug;
  //
  //methods
  //

/* brncanseltest.cc**/

  BRNCandidateSelectorTest();
  ~BRNCandidateSelectorTest();

  const char *class_name() const  { return "BRNCandidateSelectorTest"; }
  const char *processing() const  { return PUSH; }
  const char *port_count() const  { return "1/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void run_timer(Timer *t);

  int initialize(ErrorHandler *);
  void add_handlers();

 private:
  int _interval;

  uint32_t _size;

  Timer _timer;

  Packet *createpacket(int size);

};

CLICK_ENDDECLS
#endif
