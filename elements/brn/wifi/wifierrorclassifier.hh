#ifndef CLICK_WIFIERRORCLASSIFIER_HH
#define CLICK_WIFIERRORCLASSIFIER_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>

#include "elements/brn/brnelement.hh"

CLICK_DECLS

class WifiErrorClassifier : public BRNElement {
 public:

  WifiErrorClassifier();
  ~WifiErrorClassifier();

  const char *class_name() const  { return "WifiErrorClassifier"; }
  const char *port_count() const  { return "1/1-9"; }
  const char *processing() const  { return PROCESSING_A_AH; }

  void add_handlers();
  Packet *simple_action(Packet *);

  String print_stats();
  void reset();

  uint32_t _p_all;
  uint32_t _p_ok;
  uint32_t _p_crc;
  uint32_t _p_phy;
  uint32_t _p_fifo;
  uint32_t _p_decrypt;
  uint32_t _p_mic;
  uint32_t _p_zerorate;
  uint32_t _p_unknown;
  uint32_t _p_phantom;
};

CLICK_ENDDECLS
#endif
