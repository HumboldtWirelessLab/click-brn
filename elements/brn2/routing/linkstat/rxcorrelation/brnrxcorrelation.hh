#ifndef BRNRXCORRELATION_HH
#define BRNRXCORRELATION_HH

#include <click/element.hh>
#include <click/timer.hh>
#include <click/timestamp.hh>
#include <clicknet/ether.h>
#include <click/bighashmap.hh>
#include <click/vector.hh>
#include "elements/brn2/routing/linkstat/brn2_brnlinkstat.hh"

CLICK_DECLS

/*
 * =c
 * BrnRXCorrelation()
 * =s
 * 
 * =d
 */

struct rxc_header {
  uint32_t lp_id;
} __attribute__ ((packed));

struct rxc_neighbour_info {
  uint8_t mac[6];

  uint16_t reserved; //ack_highest_id;

  uint16_t first_id;
  uint16_t following_ids;
} __attribute__ ((packed));


class BrnRXCorrelation : public Element {

 public:

  class NeighbourInfo
  {
   public:

#define MAXIDS 16
#define MAXMENTIONIDS 64


    EtherAddress _addr;
    uint32_t _ids[MAXIDS];
    uint16_t _ids_base;           //_ids_base_index
    uint16_t _ids_next;           //_ids_next_index

    Timestamp _last_seen;
    unsigned int _last_lp;
    unsigned int _lp_interval_in_ms;

    uint32_t _my_ids[MAXMENTIONIDS];
    uint16_t _my_ids_next;           //_my_ids_next_index
    uint16_t _my_ids_base;           //_my_ids_base_index
    uint16_t _my_ids_last;           //_my_ids_last_index

    NeighbourInfo(EtherAddress &_new_addr, uint32_t _lp_id)
    {
      _addr = EtherAddress(_new_addr.data());
      memset(_ids,0, MAXIDS);
      _ids_base = 0;
      _ids[0] = _lp_id;
      _ids_next = 1;

      _last_seen = Timestamp::now();
      _last_lp = _lp_id;
      _lp_interval_in_ms = 0;

      memset(_my_ids,0, MAXMENTIONIDS);
      _my_ids_next = 0;
      _my_ids_base = 0;
      _my_ids_last = 0;
    }

    ~NeighbourInfo()
    {
    }

    unsigned int get_last_possible_rcv_lp()
    {
      Timestamp _time_now;
      Timestamp _time_diff;

      _time_now = Timestamp::now();
      _time_diff = _time_now - _last_seen;

      if ( _lp_interval_in_ms == 0 )
        return (_last_lp);
      else
        return(_last_lp + ( _time_diff.msec() / _lp_interval_in_ms ) );
    }

    void add_rcv_lp(unsigned int lp_id)
    {
      Timestamp _time_now;
      Timestamp _time_diff;

      _ids[_ids_next] = lp_id;

      _ids_next = ( _ids_next + 1 ) % MAXIDS;

      if ( _ids_next == _ids_base ) _ids_base = ( _ids_base + 1 ) % MAXIDS;

      if ( _lp_interval_in_ms == 0 ) {
        _time_now = Timestamp::now();
        _time_diff = _time_now - _last_seen;
        _lp_interval_in_ms = _time_diff.msec() / ( lp_id - _last_lp );
      }

      _last_seen = Timestamp::now();
      _last_lp = lp_id;

      return;
    }

    void add_mention_lp(unsigned int lp_id)
    {
      Timestamp _time_now;
      Timestamp _time_diff;

      if ( lp_id > _my_ids[_my_ids_last] ) {

        _my_ids[_my_ids_next] = lp_id;
        _my_ids_next = ( _my_ids_next + 1 ) % MAXMENTIONIDS;
        if ( _ids_next == _ids_base ) _ids_base = ( _ids_base + 1 ) % MAXIDS;
       // _id

        if ( _lp_interval_in_ms == 0 ) {
          _time_now = Timestamp::now();
          _time_diff = _time_now - _last_seen;
          _lp_interval_in_ms = _time_diff.msec() / ( lp_id - _last_lp );
        }

        _last_seen = Timestamp::now();
        _last_lp = lp_id;
      }
    }

  };

 public:
  BrnRXCorrelation();
  ~BrnRXCorrelation();
  //
  //methods
  //

  const char *class_name() const  { return "BrnRXCorrelation"; }
  const char *processing() const  { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  /**
   * Result: size of used payload
   * @param ptr 
   * @param dht_payload_size 
   */
  int lpWriteHandler(uint8_t *buffer, int size);
  void lpReadHandler(EtherAddress *ea, uint8_t *buffer, int size);

  int _debug;
  BRN2LinkStat *_linkstat;
 private:

  int add_rcv_lp(EtherAddress &_addr, unsigned int lp_id);
  int add_mention_lp(EtherAddress &node, unsigned int lp_id);

  Vector<NeighbourInfo*> _cand;

  unsigned int _linkprobe_id;           //last linkprobe id
  unsigned int _note_lp;

};

CLICK_ENDDECLS
#endif
