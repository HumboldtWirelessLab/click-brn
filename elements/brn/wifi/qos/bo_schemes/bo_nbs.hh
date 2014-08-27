#ifndef BO_NEIGHBOURS_HH
#define BO_NEIGHBOURS_HH

#include <click/element.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/wifi/rxinfo/channelstats/channelstats.hh"
#include "backoff_scheme.hh"

CLICK_DECLS

#define BO_NEIGHBOURS_START_BO 31
#define BO_NEIGHBOURS_ALPHA 85
#define BO_NEIGHBOURS_BETA 50

class BoNeighbours : public BackoffScheme {
/* Derived Functions */
public:
  /* Element */
  const char *class_name() const  { return "BoNeighbours"; }
  const char *processing() const  { return AGNOSTIC; }
  const char *port_count() const  { return "0/0"; }
  void *cast(const char *name);

  int configure(Vector<String> &, ErrorHandler *);

  /* BackoffScheme */
  int get_cwmin(Packet *p, uint8_t tos);

 public:
  BoNeighbours();
  ~BoNeighbours();

 private:

  ChannelStats *_cst;
  uint8_t _cst_sync;
  uint32_t _last_id;
  int32_t _current_bo;
  
  int32_t _alpha;
  int32_t _beta;
};


CLICK_ENDDECLS

#endif /* BO_NEIGHBOURS_HH */
