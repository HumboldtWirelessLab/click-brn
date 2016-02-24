#ifndef CLICK_COLLISIONINFO_HH
#define CLICK_COLLISIONINFO_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/bighashmap.hh>
#include <click/vector.hh>
#include <click/timestamp.hh>


#include "elements/brn/brnelement.hh"

CLICK_DECLS

/*
 * =c
 * CollisionInfo([TAG] [, KEYWORDS])
 * =s debugging
 * =d
 *
 * Keyword arguments are:
 * *
 * =a
 * Print, Wifi
 */

#define COLLISIONINFO_MAX_HW_QUEUES        4
#define COLLISIONINFO_DEFAULT_MAX_SAMPLES 10
#define COLLISIONINFO_DEFAULT_INTERVAL  1000


class CollisionInfo : public BRNElement {

 public:
  class RetryStats {
    public:
      // (Pointer to) Array of (uint32_t) stats
      uint32_t *_unicast_tx;
      uint32_t *_unicast_retries;
      uint32_t *_unicast_succ;
      uint32_t *_unicast_frac;

      // pointer to current stats
      uint32_t *_c_unicast_tx;
      uint32_t *_c_unicast_retries;
      uint32_t *_c_unicast_succ;

      // pointer to current stats
      uint32_t *_l_unicast_tx;
      uint32_t *_l_unicast_retries;
      uint32_t *_l_unicast_succ;
      uint32_t *_l_unicast_frac;

      uint16_t _max_samples;
      uint16_t _curr_sample;

      uint16_t _no_queues;

      uint32_t _interval;
      Timestamp _last_index_inc;

      RetryStats(uint32_t no_queues, uint32_t records, uint32_t interval, Timestamp *now): _max_samples(records), _curr_sample(0), _no_queues(no_queues), _interval(interval)
      {
        _unicast_tx = new uint32_t[no_queues * records];
        _unicast_retries = new uint32_t[no_queues * records];
        _unicast_succ = new uint32_t[no_queues * records];
        _unicast_frac = new uint32_t[no_queues * records];

        memset(_unicast_tx, 0, sizeof(uint32_t) * no_queues * records);
        memset(_unicast_retries, 0, sizeof(uint32_t) * no_queues * records);
        memset(_unicast_succ, 0, sizeof(uint32_t) * no_queues * records);
        for ( uint32_t i = 0; i < (no_queues * records); i++ ) _unicast_frac[i] = 0; //TODO: ErrorHandling -1;

        _c_unicast_tx = _unicast_tx;
        _c_unicast_retries = _unicast_retries;
        _c_unicast_succ = _unicast_succ;

        _l_unicast_tx = &(_unicast_tx[no_queues * (records-1)]);
        _l_unicast_retries = &(_unicast_retries[no_queues * (records-1)]);
        _l_unicast_succ = &(_unicast_succ[no_queues * (records-1)]);
        _l_unicast_frac = &(_unicast_frac[no_queues * (records-1)]);

        _last_index_inc = *now;
      }

      RetryStats(const RetryStats &rs) : _max_samples(rs._max_samples),
                                         _curr_sample(rs._curr_sample),_no_queues(rs._no_queues),_interval(rs._interval),
                                         _last_index_inc(rs._last_index_inc) {
        _unicast_tx = new uint32_t[_no_queues * _max_samples];
        _unicast_retries = new uint32_t[_no_queues * _max_samples];
        _unicast_succ = new uint32_t[_no_queues * _max_samples];
        _unicast_frac = new uint32_t[_no_queues * _max_samples];
        _c_unicast_tx = _unicast_tx;
        _c_unicast_retries = _unicast_retries;
        _c_unicast_succ = _unicast_succ;
        _l_unicast_tx = &(_unicast_tx[_no_queues * (_max_samples-1)]);
        _l_unicast_retries = &(_unicast_retries[_no_queues * (_max_samples-1)]);
        _l_unicast_succ = &(_unicast_succ[_no_queues * (_max_samples-1)]);
        _l_unicast_frac = &(_unicast_frac[_no_queues * (_max_samples-1)]);
      }

      ~RetryStats() {
        delete[] _unicast_tx;
        delete[] _unicast_retries;
        delete[] _unicast_succ;
        delete[] _unicast_frac;
      }

      void set_to_next() {
        _l_unicast_frac = &(_unicast_frac[_no_queues * _curr_sample]);

        _curr_sample = (_curr_sample + 1) % _max_samples;

        _l_unicast_tx = _c_unicast_tx;
        _l_unicast_retries = _c_unicast_retries;
        _l_unicast_succ = _c_unicast_succ;
        _c_unicast_tx = &(_unicast_tx[_no_queues * _curr_sample]);
        _c_unicast_retries = &(_unicast_retries[_no_queues * _curr_sample]);
        _c_unicast_succ = &(_unicast_succ[_no_queues * _curr_sample]);
        memset(_c_unicast_tx, 0, sizeof(uint32_t) * _no_queues);
        memset(_c_unicast_retries, 0, sizeof(uint32_t) * _no_queues);
        memset(_c_unicast_succ, 0, sizeof(uint32_t) * _no_queues);
        _last_index_inc += Timestamp::make_msec(_interval);

        for ( int q = 0; q < _no_queues; q++ ) {
          if ( _l_unicast_tx[q] == 0 ) _l_unicast_frac[q] = -1;
          else _l_unicast_frac[q] = (100 * _l_unicast_succ[q]) / _l_unicast_tx[q];
        }
      }

      inline void update(Timestamp *ts, uint16_t retries, uint32_t flags, uint8_t q ) {
        while ( (*ts - _last_index_inc).msecval() > _interval ) {
          set_to_next();
        }

        _c_unicast_tx[q] += retries + 1;
        _c_unicast_retries[q] += retries;

        if (!(flags & WIFI_EXTRA_TX_FAIL)) {
          _c_unicast_succ[q]++;
        }
      }

      inline uint32_t get_frac(uint32_t q) {
        return _l_unicast_frac[q];
      }

  };

 
  
 public:
  typedef HashMap<EtherAddress, RetryStats*> RetryStatsTable;
  typedef RetryStatsTable::const_iterator RetryStatsTableIter;

  CollisionInfo();
  ~CollisionInfo();

  const char *class_name() const { return "CollisionInfo"; }
  const char *port_count() const { return PORTS_1_1; }
  const char *processing() const { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);

  Packet *simple_action(Packet *);
  void add_handlers();

  String stats_handler(int mode);
  RetryStatsTable rs_tab;

  uint32_t _interval;
  uint32_t _max_samples;

  RetryStats *_global_rs;

  RetryStats *get_global_stats() {
    return _global_rs;
  }
};

CLICK_ENDDECLS
#endif
