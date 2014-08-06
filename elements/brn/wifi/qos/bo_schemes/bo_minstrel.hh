#ifndef BO_MINSTREL_HH
#define BO_MINSTREL_HH

#include <click/element.hh>
#include <elements/brn/brnelement.hh>

#include "backoff_scheme.hh"

CLICK_DECLS

#define BOSTATS_MAX_BO_EXP 16

#define BO_MINSTREL_DEFAULT_STATSSIZE            32
#define BO_MINSTREL_DEFAULT_STATSINTERVAL       100

class BoStats {
 public:
  uint16_t tx_count[BOSTATS_MAX_BO_EXP];
  uint16_t succ_count[BOSTATS_MAX_BO_EXP];
  uint16_t succ_rate[BOSTATS_MAX_BO_EXP];

  BoStats() {
    reset();
  }

  void reset() {
    memset(tx_count,0,sizeof(tx_count));
    memset(succ_count,0,sizeof(succ_count));
    memset(succ_rate,0,sizeof(succ_rate));
  }

  void add_stats(uint start_bo, uint8_t end_bo, bool succ) {
    for( int i = start_bo; i <= end_bo; i++) tx_count[i]++;
    if (succ) succ_count[end_bo]++;
  }

  void calc_stats() {
    for( int i = 0; i < BOSTATS_MAX_BO_EXP; i++)
      if (tx_count[i] != 0) succ_rate[i] = (uint16_t)((100*(int)succ_count[i])/(int)tx_count[i]);
  }
};

class BoMinstrel : public BackoffScheme {
  /* Derived Functions */
 public:

  BoMinstrel();
  ~BoMinstrel();

  /* Element */
  const char *class_name() const  { return "BoMinstrel"; }
  const char *processing() const  { return AGNOSTIC; }
  const char *port_count() const  { return "0/0"; }
  void *cast(const char *name);

  int configure(Vector<String> &, ErrorHandler *);
  void add_handlers();

  /* BackoffScheme */
  int get_cwmin(Packet *p, uint8_t tos);
  void handle_feedback(uint8_t retries);

  Timestamp _last_update;
  uint16_t _stats_index;
  uint16_t _best_bo;
  uint16_t _best_bo_exp;
  uint16_t _last_bo;
  uint16_t _last_bo_exp;

  BoStats _bostats[BO_MINSTREL_DEFAULT_STATSSIZE];

};

CLICK_ENDDECLS

#endif /* BO_MINSTREL_HH */
