#ifndef BACKOFF_SCHEME_HH
#define BACKOFF_SCHEME_HH

#include <click/element.hh>
#include <click/packet.hh>

#include "elements/brn/brnelement.hh"

CLICK_DECLS



class BackoffScheme: public BRNElement {
public:
  BackoffScheme();
  virtual ~BackoffScheme();

  virtual uint16_t get_id() = 0;
  virtual int get_cwmin(Packet *p, uint8_t tos) = 0;
  virtual void handle_feedback(uint8_t retries) = 0;

  virtual void set_conf(uint32_t min, uint32_t max);

protected:
  uint32_t _min_cwmin;
  uint32_t _max_cwmin;
};



CLICK_ENDDECLS

#endif /* BACKOFF_SCHEME_HH */
