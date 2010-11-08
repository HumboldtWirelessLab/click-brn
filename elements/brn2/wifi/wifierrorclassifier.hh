#ifndef CLICK_WIFIERRORCLASSIFIER_HH
#define CLICK_WIFIERRORCLASSIFIER_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

class WifiErrorClassifier : public Element {
 public:

  WifiErrorClassifier();
  ~WifiErrorClassifier();

  const char *class_name() const  { return "WifiErrorClassifier"; }
  const char *port_count() const  { return "1/1-8"; }
  const char *processing() const  { return PROCESSING_A_AH; }

  void add_handlers();
  Packet *simple_action(Packet *);

  uint32_t _p_ok;
  uint32_t _p_crc;
  uint32_t _p_phy;
  uint32_t _p_fifo;
  uint32_t _p_decrypt;
  uint32_t _p_mic;
  uint32_t _p_zerorate;
  uint32_t _p_unknown;
};

CLICK_ENDDECLS
#endif
