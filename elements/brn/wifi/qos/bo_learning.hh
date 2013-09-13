#ifndef BO_LEARNING_HH
#define BO_LEARNING_HH

#include <click/element.hh>
#include <elements/brn/brnelement.hh>

#include "backoff_scheme.hh"


CLICK_DECLS



class BoLearning : public BackoffScheme {
public:
  /* Element */
  const char *class_name() const  { return "BoLearning"; }
  const char *processing() const  { return AGNOSTIC; }
  const char *port_count() const  { return "0/0"; }
  void *cast(const char *name);

  int configure(Vector<String> &, ErrorHandler *);
  void add_handlers();

  /* BackoffScheme */
  int get_cwmin();
  void handle_feedback(uint8_t retries);


public:
  BoLearning();
  ~BoLearning();

private:
  void increase_cw();
  void decrease_cw();
  void keep_cw();


private:
  static const uint8_t  _retry_threshold = 1;
  static const uint16_t _min_cwmin       = 31;  // cwmin lower bound
  static const uint16_t _max_cwmin       = 255; // cwmin upper bound
  static const uint16_t _bo_start        = 63;  // initial bo

  /* learning bo */
  uint32_t _current_bo;

  /* packet stats */
  uint32_t _feedback_cnt;
  uint32_t _tx_cnt;

  /* bo stats */
  uint32_t _bo_cnt_up;
  uint32_t _bo_cnt_down;
};



CLICK_ENDDECLS

#endif /* BO_LEARNING_HH */
