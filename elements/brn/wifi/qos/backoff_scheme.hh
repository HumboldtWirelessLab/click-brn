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

  virtual int get_cwmin() = 0;
  virtual void handle_feedback(uint8_t retries) = 0;

  void set_scheme_id(uint16_t id);


protected:
  uint16_t _id;


};



CLICK_ENDDECLS

#endif /* BACKOFF_SCHEME_HH */
