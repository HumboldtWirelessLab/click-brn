#ifndef BO_MEDIUMSHARE_HH
#define BO_MEDIUMSHARE_HH

#include <click/element.hh>
#include <click/hashmap.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/wifi/rxinfo/channelstats/channelstats.hh"
#include "elements/brn/wifi/rxinfo/channelstats/cooperativechannelstats.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinkstat.hh"

#include "backoff_scheme.hh"
#include "elements/brn/wifi/brnwifi.hh"

CLICK_DECLS


#define BO_MEDIUMSHARE_RETRY_TRESHOLD 1

class BoMediumShare : public BackoffScheme {
/* Derived Functions */
 public:
  /* Element */
  const char *class_name() const  { return "BoMediumShare"; }
  const char *processing() const  { return AGNOSTIC; }
  const char *port_count() const  { return "0/0"; }
  void *cast(const char *name);

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void add_handlers();

  /* BackoffScheme */
  int get_cwmin(Packet *p, uint8_t tos);
  void handle_feedback(uint8_t retries);
  void set_conf(uint32_t min_cwmin, uint32_t max_cwmin);

 public:
  BoMediumShare();
  ~BoMediumShare();

 private:
  void increase_cw();
  void increase_cw_strict(uint8_t retries);
  void decrease_cw();


private:
  ChannelStats *_cst;
  CooperativeChannelStats *_cocst;
  String _cocst_string;

  uint16_t _current_bo;

  uint32_t _last_tx;
  uint32_t _last_id_cw;
  uint32_t _last_id_hf;
  uint32_t _retry_sum;

  int _bo_decision;
};



CLICK_ENDDECLS

#endif /* BO_MEDIUMSHARE_HH */
