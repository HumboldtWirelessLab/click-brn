#ifndef BO_LEARNING_STRICT_HH
#define BO_LEARNING_STRICT_HH

#include <click/element.hh>
#include <elements/brn/brnelement.hh>

#include "backoff_scheme.hh"
#include "tos2queuemapper.hh"


CLICK_DECLS



struct bos_learningstrict_stats {
  uint32_t feedback_cnt;
  uint32_t tx_cnt;
  uint32_t bo_cnt_up;
  uint32_t bo_cnt_down;
};


class BoLearningStrict : public BRNElement, public BackoffScheme {
public:
  /* Element */
  const char *class_name() const  { return "BoLearningStrict"; }

  /* BackoffScheme */
  int get_cwmin();
  void handle_feedback(Packet *p);


public:
  BoLearningStrict(struct bo_scheme_utils scheme_utils);
  BoLearningStrict();
  ~BoLearningStrict();

  struct bos_learningstrict_stats get_stats();

private:
  void increase_cw(uint8_t retries);
  void decrease_cw();
  void keep_cw();


private:
  static const uint8_t  _retry_threshold = 1;
  static const uint16_t _min_cwmin       = 31;
  static const uint16_t _max_cwmin       = 255;
  static const uint16_t _bo_start        = 63;

  uint32_t _current_bo;

  int32_t  _pkt_in_q;
  uint32_t _feedback_cnt;
  uint32_t _tx_cnt;
  uint32_t _bo_cnt_up;
  uint32_t _bo_cnt_down;
};



CLICK_ENDDECLS

#endif /* BO_LEARNING_STRICT_HH */
