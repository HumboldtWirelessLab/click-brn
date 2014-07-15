#ifndef CLICK_PULLSTATS_HH
#define CLICK_PULLSTATS_HH
#include "elements/brn/brnelement.hh"

CLICK_DECLS

class PullStats : public BRNElement {

public:

  PullStats();
  ~PullStats();

  const char *class_name() const  { return "PullStats"; }
  const char *port_count() const  { return "1/1"; }

  const char *processing() const  { return PULL; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const { return true; }

  int initialize(ErrorHandler *);

  Packet *pull(int);

  void add_handlers();

   bool _last_pull_null;
   Timestamp _ts_last_pull_null;
   uint32_t _pull_null_duration;

};

CLICK_ENDDECLS
#endif
