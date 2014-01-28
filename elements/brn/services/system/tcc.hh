#ifndef TCC_HH
#define TCC_HH

#include <click/confparse.hh>

#include "elements/brn/brnelement.hh"

#include "libtcc.h"

CLICK_DECLS

class TCC : public BRNElement {
public:
  TCC();
  ~TCC();

  const char *class_name() const { return "TCC"; }
  const char *port_count() const { return "1/1"; }
  const char *processing() const { return AGNOSTIC; }

  int configure(Vector<String> &conf, ErrorHandler *errh);
  bool can_live_reconfigure() const { return false; }
  int initialize(ErrorHandler *);
  void add_handlers();

  Packet* simple_action(Packet *p);
  void add_code(String code);
  String stats();

  static void *tcc_packet_resize(void *, int, int);
  static int tcc_packet_size(void *);
  static const uint8_t *tcc_packet_data(void *);
  static void tcc_packet_kill(void *);

 private:

   TCCState *_tcc_s;

   Packet *(*click_tcc_simple_action)(Packet *p);
};

CLICK_ENDDECLS
#endif
