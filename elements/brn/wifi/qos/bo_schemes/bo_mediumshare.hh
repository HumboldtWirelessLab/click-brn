#ifndef BO_MEDIUMSHARE_HH
#define BO_MEDIUMSHARE_HH

#include <click/element.hh>
#include <click/hashmap.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/wifi/rxinfo/channelstats/channelstats.hh"
#include "elements/brn/wifi/rxinfo/channelstats/cooperativechannelstats.hh"
#include "elements/brn/wifi/rxinfo/hiddennodedetection.hh"

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
  void cohesion(uint32_t mean_tx, uint32_t own_tx, uint32_t sum_tx);
  void gravitation(uint32_t own_tx);
  void seperation(uint32_t retries, int pkt_cnt);
  void calc_new_bo();
  void reset_counts();

  // Helper
  bool stats_are_new(struct airtime_stats *);
  void print_reg_info(struct airtime_stats *);
  void print_2hop_tx_dur(NodeChannelStats *ncst);
  EtherAddress get_src_etheraddr(Packet *p);
  EtherAddress get_dst_etheraddr(Packet *p);
  void limit_bo(int lower, int upper);
  void cohesion_decision(int, int, int);
  void gravitation_decision(void);
  void seperation_decision(void);
  void eval_all_rules(void);

private:
  ChannelStats *_cst;
  CooperativeChannelStats *_cocst;
  String _cocst_string;
  HiddenNodeDetection *_hnd;
  bool _hn_detected;
  uint16_t _retry_threshold;

  uint16_t _current_bo;

  uint32_t _last_tx;
  uint32_t _last_id_cw;
  uint32_t _last_id_hf;
  uint32_t _retry_sum;
  double _cohesion_value;
  double _gravity_value;
  double _seperation_value;
  uint32_t _alpha;
  uint32_t _beta;
  uint32_t _gamma;

  int _bo_decision;
  int _pkt_cnt;
};



CLICK_ENDDECLS

#endif /* BO_MEDIUMSHARE_HH */
