#ifndef BRN2LINKTABLEELEMENT_HH
#define BRN2LINKTABLEELEMENT_HH

#include <click/element.hh>
#include <click/timer.hh>

CLICK_DECLS

/*
 * =c
 * BRN2Linktable()
 * =s
 * 
 * =d
 */

class BRN2Linktable : public Element {

 public:

   int _debug;
  //
  //methods
  //

/* brn2_linktable.cc**/

  BRN2Linktable();
  ~BRN2Linktable();

  const char *class_name() const  { return "BRN2Linktable"; }
  const char *processing() const  { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void run_timer(Timer *t);

  int initialize(ErrorHandler *);
  void add_handlers();

  class Brn2_LinkInfo {
    public:
      EtherAddress _from;
      EtherAddress _to;
      uint32_t _metric;
      uint32_t _age;
      struct timeval _last_updated;

      Brn2_LinkInfo()
      {
        _from = EtherAddress();
        _to = EtherAddress();
        _metric = 0;
        _age = 0;
        _last_updated.tv_sec = 0;
      }

      ~Brn_LinkInfo() {}
  }

 private:

  int _interval;

  uint32_t _size;

  Timer _timer;

  Packet *create_linkprobe(int size);
};

CLICK_ENDDECLS
#endif
