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
  uint16_t lp_id;
  uint8_t  src[6];
} __attribute__ ((packed));

struct rxc_neighbour_info {
  uint8_t mac[6];

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
    Timestamp _ids_times[MAXIDS];

    uint32_t _lp_interval_in_ms;

    uint32_t _my_ids[MAXMENTIONIDS];
    uint16_t _my_ids_base;           //_my_ids_base_index
    uint16_t _my_ids_next;           //_my_ids_next_index

    uint32_t _my_ids_highest;        //_my_ids_highest

    NeighbourInfo(EtherAddress &_new_addr, uint32_t _lp_id)
    {
      _addr = EtherAddress(_new_addr.data());
      memset(_ids, 0, sizeof(_ids));
      memset(_ids, 0, sizeof(_ids_times));
      _ids_base = 0;
      _ids[0] = _lp_id;
      _ids_times[0] = Timestamp::now();
      _ids_next = 1;

      _lp_interval_in_ms = 0;

      memset(_my_ids, 0, sizeof(_my_ids));
      _my_ids_next = 0;
      _my_ids_base = 0;
      _my_ids_highest = 0;
    }

    ~NeighbourInfo()
    {
    }

    unsigned int get_last_possible_rcv_lp()
    {
      int _ids_last;
      Timestamp _time_now;
      Timestamp _time_diff;

      _ids_last = ( _ids_next + MAXIDS - 1 ) % MAXIDS;
      _time_diff = Timestamp::now() - _ids_times[_ids_last];

      if ( _lp_interval_in_ms == 0 )
        return (_ids[_ids_last]);
      else
        return (_ids[_ids_last] + (( 1000 * _time_diff.sec() + _time_diff.msec() ) / _lp_interval_in_ms ));
    }

    void add_recv_linkprobe(unsigned int lp_id)
    {
      Timestamp _time_diff;

      _ids[_ids_next] = lp_id;
      _ids_times[_ids_next] = Timestamp::now();

      _time_diff = _ids_times[_ids_next] - _ids_times[_ids_base];
      _lp_interval_in_ms = ( 1000 * _time_diff.sec() + _time_diff.msec() ) / ( _ids[_ids_next] - _ids[_ids_base] );

      _ids_next = ( _ids_next + 1 ) % MAXIDS;
      if ( _ids_next == _ids_base ) _ids_base = ( _ids_base + 1 ) % MAXIDS;

      return;
    }

    int get_last_recv_linkprobe()
    {
      return (_ids[(_ids_next + MAXIDS - 1) % MAXIDS]);
    }

    int get_index_of(uint16_t id_first)
    {
      int index = _ids_base;

      while ( (_ids[index] < id_first) && (index != _ids_next) ) index = (index + 1 ) % MAXIDS;

      return ((_ids_next + MAXIDS - 1) % MAXIDS);
    }

    void add_mention_linkprobe(unsigned int lp_id)
    {
      if ( lp_id > _my_ids_highest ) {

        _my_ids[_my_ids_next] = lp_id;
        _my_ids_next = ( _my_ids_next + 1 ) % MAXMENTIONIDS;

        if ( _my_ids_next == _my_ids_base ) _my_ids_base = ( _my_ids_base + 1 ) % MAXMENTIONIDS;  //delete oldest if buffer is full

        _my_ids_highest = lp_id;
      }
    }

    /**Use ID*/
    int getPER()
    {
      int last_poss_lp = get_last_possible_rcv_lp();

      return ( (100 * (( MAXIDS  + _ids_next - _ids_base ) % MAXIDS)) / (last_poss_lp - _ids[_ids_base] + 1));
    }

    /** Use mention IDs*/
    int getBackwardPER(unsigned int lastid)
    {
      return ( (100 * (( MAXMENTIONIDS  + _my_ids_next - _my_ids_base ) % MAXMENTIONIDS) / (lastid - _my_ids[_my_ids_base] + 1)) );
    }

    int getMIDSLen() { return MAXMENTIONIDS;}
    int getIDSLen() { return MAXIDS;}
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

  String printNeighbourInfo();

  int lpSendHandler(uint8_t *buffer, int size);
  int lpReceiveHandler(uint8_t *buffer, int size);

  int _debug;

 private:

  BRN2LinkStat *_linkstat;

  NeighbourInfo *getNeighbourInfo(EtherAddress *ea);
  int add_recv_linkprobe(EtherAddress *_addr, unsigned int lp_id);

  Vector<NeighbourInfo*> _cand;

  unsigned int _linkprobe_id;           //last linkprobe id

 public:
  EtherAddress _me;
  int countNeighbours() { return _cand.size();}
  NeighbourInfo *getNeighbourInfo(int i) { return _cand[i];}
  unsigned int getLPID() {return _linkprobe_id;}
};

CLICK_ENDDECLS
#endif
