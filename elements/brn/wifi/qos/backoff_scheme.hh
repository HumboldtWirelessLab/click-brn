#ifndef BACKOFF_SCHEME_HH
#define BACKOFF_SCHEME_HH

#include <click/packet.hh>

CLICK_DECLS


struct bo_scheme_utils {
  uint8_t no_queues;
  uint16_t *cwmin;
  uint16_t *cwmax;
  uint16_t *aifs;
};


class BackoffScheme {
public:
  BackoffScheme(struct bo_scheme_utils scheme_utils) :
    _no_queues(scheme_utils.no_queues),
    _cwmin(scheme_utils.cwmin),
    _cwmax(scheme_utils.cwmax),
    _aifs(scheme_utils.aifs)
  {}

  virtual int get_cwmin() = 0;
  virtual void handle_feedback(Packet *p) = 0;

protected:
  uint8_t _no_queues;
  uint16_t *_cwmin;
  uint16_t *_cwmax;
  uint16_t *_aifs;
};


CLICK_ENDDECLS

#endif /* BACKOFF_SCHEME_HH */
