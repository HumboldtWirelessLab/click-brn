#ifndef BO_LEARNING_HH
#define BO_LEARNING_HH

#include <click/element.hh>
#include <elements/brn/brnelement.hh>

#include "backoff_scheme.hh"
#include "tos2queuemapper.hh"

#define BOLEARNING_STRICT 0
#define BOLEARNING_MIN_CWMIN 31
#define BOLEARNING_MAX_CWMIN 255
#define BOLEARNING_STARTING_BO 63

CLICK_DECLS


struct bos_learning_stats {
  uint32_t feedback_cnt;
  uint32_t tx_cnt;
  uint32_t bo_cnt_up;
  uint32_t bo_cnt_down;
};


class BoLearning : public BRNElement, public BackoffScheme {

public: /* derived */
  const char *class_name() const  { return "BoLearning"; }

  int get_cwmin();
  void handle_feedback(Packet *p);

public: /* own */
  BoLearning(struct bo_scheme_utils scheme_utils);
  BoLearning();
  ~BoLearning();

  struct bos_learning_stats get_stats();

private:
  uint32_t _current_bo;

  uint32_t _feedback_cnt;
  uint32_t _tx_cnt;
  uint32_t _bo_cnt_up;
  uint32_t _bo_cnt_down;

  int32_t _pkt_in_q;
};


CLICK_ENDDECLS

#endif /* BO_LEARNING_HH */
