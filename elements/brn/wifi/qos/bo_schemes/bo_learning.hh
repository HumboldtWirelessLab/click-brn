#ifndef BO_LEARNING_HH
#define BO_LEARNING_HH

#include <click/element.hh>
#include <elements/brn/brnelement.hh>

#include "backoff_scheme.hh"

CLICK_DECLS

#define BO_LEARNING_START_BO 32
#define BO_LEARNING_RETRY_THRESHOLD 1

class BoLearning : public BackoffScheme {

/* Derived Functions */
public:
  /* Element */
  const char *class_name() const  { return "BoLearning"; }
  const char *processing() const  { return AGNOSTIC; }
  const char *port_count() const  { return "0/0"; }
  void *cast(const char *name);

  int configure(Vector<String> &, ErrorHandler *);

  /* BackoffScheme */
  int get_cwmin(Packet *p, uint8_t tos);
  void handle_feedback(uint8_t retries);

  /* Own Functions */
 public:
  BoLearning();
  ~BoLearning();

 private:
  void increase_cw();
  void increase_cw_strict(uint8_t retries);
  void decrease_cw();
  //void keep_cw();


/* Own Variables */
private:
  /* scheme flavour: strict */
  uint8_t _strict;

  /* learning bo */
  uint32_t _current_bo;

  /* packet stats */
  //uint32_t _feedback_cnt;
  //uint32_t _tx_cnt;

  /* bo stats */
  uint32_t _bo_cnt_up;
  uint32_t _bo_cnt_down;

  // override min/max for cwmin normally determined by starting queues
  uint32_t _learning_min_cwmin;
  uint32_t _learning_max_cwmin;

  uint8_t _cap;
};



CLICK_ENDDECLS

#endif /* BO_LEARNING_HH */
