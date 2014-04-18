/* Copyright (C) 2005 BerlinRoofNet Lab
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA. 
 *
 * For additional licensing options, consult http://www.BerlinRoofNet.de 
 * or contact brn@informatik.hu-berlin.de. 
 */

#ifndef BRN2BRNLINKSTATELEMENT_HH
#define BRN2BRNLINKSTATELEMENT_HH

/*
=c
BRN2LinkStat([I<KEYWORDS>])

=s Wifi, Wireless Routing

Track broadcast loss rates at different bitrates.

=d

Expects probe packets as input.  Records the last WINDOW unique
(not neccessarily sequential) sequence numbers of link probes from
each host, and calculates loss rates over the last TAU milliseconds
for each host.  If the output is connected, sends probe
packets every PERIOD milliseconds.  The source Ethernet 
address ETH must be specified if the second output is
connected.

Keyword arguments are:

=over 8

=item ETH
Ethernet address of this node; required if output is connected.

=item PERIOD
Unsigned integer.  Millisecond period between sending link probes
if second output is connected.  Defaults to 1000 (1 second).

=item WINDOW
Unsigned integer.  Number of most recent sequence numbers to remember
for each host.  Defaults to 100.

=item TAU
Unsigned integer.  Millisecond period over which to calculate loss
rate for each host.  Defaults to 10,000 (10 seconds).

 =back

=a LinkStat
*/

#include <click/bighashmap.hh>
#include <click/deque.hh>
#include <click/vector.hh>
#include <click/element.hh>
#include <click/glue.hh>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
#include <click/timer.hh>
#include <click/string.hh>
#include <click/timestamp.hh>
#include "elements/brn/brnelement.hh"
#include "elements/brn/wifi/brnavailablerates.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"
#include "elements/brn/routing/identity/brn2_device.hh"
#include "elements/brn/brn2.h"

#include "metric/brn2_genericmetric.hh"

CLICK_DECLS

#define LINKSTAT_EXTRA_DEBUG

#define LINKPROBE_VERSION (uint8_t)0x02

#define LINKSTAT_DEFAULT_STALE 10000


class BRN2LinkStat : public BRNElement {

public:

  typedef HashMap<BrnRateSize, int> BrnRateSizeIndexMap;

  // Packet formats:

  // BRN2LinkStat packets have a link_probe header immediately following
  // the ethernet header.  num_links link_entries follow the
  // link_probe header.

  enum {
    PROBE_AVAILABLE_RATES = (1<<0),
    PROBE_REV_FWD_INFO = (1<<1),
    PROBE_HANDLER_PAYLOAD = (1<<2),
  };

  struct link_probe {
    uint8_t _version;
    uint8_t _flags;

    uint16_t _cksum;       // internet checksum

    uint32_t _seq;         // sequence number
    uint32_t _tau;         // this node's loss-rate averaging period, in msecs
    uint16_t _period;      // period of this node's probe broadcasts, in msecs
    uint8_t  _num_probes;  // number of probes monitored by this node
    uint8_t  _num_links;   // number of wifi_link_entry entries following

    uint8_t _brn_rate_size_index;
    uint8_t _num_incl_probes;

    link_probe() { memset(this, 0x0, sizeof(struct link_probe)); }
  } CLICK_SIZE_PACKED_ATTRIBUTE;

  struct probe_params {
    uint16_t _rate; //for n use packed_16
    uint16_t _size;
    uint8_t _power;
  } CLICK_SIZE_PACKED_ATTRIBUTE;

  struct link_entry {
    uint8_t _ether[6];
    uint8_t _num_rates;
    uint8_t _flags;
    uint32_t _seq;
    uint16_t _age;         //age in 100 ms -> 65535 * 10ms = 655350ms = 6553s

    link_entry() { }

    link_entry(EtherAddress ether) {
      memcpy(_ether, ether.data(), 6);
    }
  } CLICK_SIZE_PACKED_ATTRIBUTE;

  struct link_info {
    uint8_t _rate_index; //linkprobe size
    uint8_t _min_power;
    uint8_t _fwd;        //fwd ratio
    uint8_t _rev;        //rev ratio
  } CLICK_SIZE_PACKED_ATTRIBUTE;

  /**************** PROBELIST ********************/
  /* Stores information about the probes of neighbouring nodes */

  struct probe_t {
    Timestamp _when;
    uint32_t _seq;
    uint16_t _rate; //for n use packed_16
    uint16_t _size;
    uint8_t _power;
    uint8_t _rx_power;
    uint8_t _brn_rate_size_index;
    uint8_t _reserved;

    probe_t(const Timestamp &t,
            uint32_t s,
            uint16_t r,
            uint16_t sz,
            uint8_t p,
            uint8_t rx_p) : _when(t), _seq(s), _rate(r), _size(sz), _power(p), _rx_power(rx_p) { }
  };


  struct probe_list_t {
    EtherAddress _ether;
    uint32_t _period;     // period of this node's probes, as reported by the node
    uint32_t _tau;        // this node's stats averaging period, as reported by the node
    Timestamp _tau_ts;    // timestamp of tau
    uint8_t  _num_probes; // number of probes_types sent by the node
    uint32_t _seq;        //highest sequence numbers

    /*
     * Information about probes
     */
    BrnRateSizeIndexMap _probe_types_map;
    Vector<BrnRateSize> _probe_types;

    Vector<uint8_t> _fwd_rates;          //psr (packet success rate)
    Vector<uint8_t> _fwd_min_rx_powers;  //min rssi

    Vector<uint16_t> _rev_no_probes;     //psr (packet success rate)
    Vector<uint16_t> _rev_min_rssi;
    Vector<int16_t> _rev_min_rssi_index;

    Timestamp _last_rx;
    Deque<probe_t> _probes;          // most recently received probes
    //char *_probes;

    probe_list_t(const EtherAddress &et, unsigned int per, unsigned int t) :
        _ether(et),
        _period(per),
        _tau(t),
        _seq(0)
    {
      _tau_ts = Timestamp::make_msec(_tau);
    }

    probe_list_t() : _period(0), _tau(0), _seq(0) {
      _tau_ts = Timestamp::make_msec(_tau);
    }

    inline void remove_old_probes(Timestamp &earliest) {
      // keep stats for at least the averaging period; kick old probes
      int no_rm_probes = 0;
      while ((unsigned)_probes.size() && (_probes[0]._when < earliest)) {
        _rev_no_probes[_probes[0]._brn_rate_size_index]--;
        _probes.pop_front();
        no_rm_probes++;
      }

      if ( no_rm_probes != 0 ) {
        for ( int i = _rev_min_rssi_index.size()-1; i >= 0; i--)
          if ( _rev_min_rssi_index[i] >= 0 )
            _rev_min_rssi_index[i] -= no_rm_probes;
      }
    }

    uint8_t rev_rate(Timestamp start, BrnRateSize &rs) {
      int *i_p = _probe_types_map.findp(rs);
      if (i_p == NULL) return 0;
      uint8_t rs_index = (uint8_t)*i_p;

      Timestamp now = Timestamp::now();
      Timestamp earliest = now - _tau_ts;

      if (_period == 0) {
        click_chatter("period is 0\n");
        return 0;
      }

      remove_old_probes(earliest);

      /*
       * uint32_t num = 0;
       * for (int i = _probes.size() - 1; i >= 0; i--)
       *   if ( _probes[i]._brn_rate_size_index == rs_index ) num++;
       *
       * click_chatter("calc: %d sum: %d diff: %d", num, _rev_no_probes[rs_index], (num==_rev_no_probes[rs_index])?(uint32_t)0:(uint32_t)1);
       */

      Timestamp since_start = now - start;

      uint32_t ms_since_start = MAX(0, since_start.msecval());
      uint32_t fake_tau = MIN(_tau, ms_since_start);
      assert(_probe_types.size());
      uint32_t num_expected = MAX(1,MIN((fake_tau / _period),(_seq / _num_probes)));

      return (uint8_t)(MIN(100, (100 * _rev_no_probes[rs_index]/*num*/) / num_expected));
    }

    uint8_t rev_rate(Timestamp start, int rate, int size, int power) {
      BrnRateSize rs = BrnRateSize(rate, size, power);
      return rev_rate(start, rs);
    }

    uint8_t get_min_rx_power(BrnRateSize &rs) {
      int *i_p = _probe_types_map.findp(rs);
      if (i_p == NULL) return 0;
      uint8_t rs_index = (uint8_t)*i_p;

      if ( _rev_min_rssi_index[rs_index] >= 0 ) return _rev_min_rssi[rs_index];

      Timestamp earliest = Timestamp::now() - _tau_ts;

      remove_old_probes(earliest);

      uint32_t min_rx_power = 255;
      uint16_t min_rx_power_index = -1;
      for (int i = _probes.size() - 1; i >= 0; i--)
        if ((_probes[i]._brn_rate_size_index == rs_index) && (_probes[i]._rx_power < min_rx_power)) {
          min_rx_power = _probes[i]._rx_power;
          min_rx_power_index = i;
        }

      if ( min_rx_power == 255 ) return 0;

      _rev_min_rssi[rs_index] = min_rx_power;
      _rev_min_rssi_index[rs_index] = min_rx_power_index;

      return min_rx_power;
    }

    uint8_t get_min_rx_power(int rate, int size, int power) {
      BrnRateSize rs = BrnRateSize(rate, size, power);
      return get_min_rx_power(rs);
    }

    void push_back_probe(const Timestamp &now, uint32_t s, BrnRateSize &rs, uint8_t rx_p) {
      assert(_probe_types_map.findp(rs) != NULL);
      probe_t probe(now, s, rs._rate, rs._size, rs._power, rx_p);
      probe._brn_rate_size_index = _probe_types_map.find(rs);

      if ( (rx_p <= _rev_min_rssi[probe._brn_rate_size_index]) || (_rev_min_rssi_index[probe._brn_rate_size_index] < 0) ) {
        _rev_min_rssi[probe._brn_rate_size_index] = rx_p;
        _rev_min_rssi_index[probe._brn_rate_size_index] = _probes.size();
      }

      _probes.push_back(probe);
      _rev_no_probes[probe._brn_rate_size_index]++; //new probe

      Timestamp earliest = now - _tau_ts;
      remove_old_probes(earliest);
    }

    void clear() {
      _probes.clear();
      _seq = 0;
    }
  };

  // Per-sender map of received probes._sent
  typedef HashMap<EtherAddress, probe_list_t> ProbeMap;

  /************** HANDERINFO ***********************/
  /* Other element can register handlers which are called for each send
   * and received linkprobe. The elements can store information in the unused
   * space of the linkprobe
   */

  class HandlerInfo {
   public:
    void *_element;
    int32_t _protocol;
    int32_t (*_tx_handler)(void* element, const EtherAddress *src, char *buffer, int32_t size);
    int32_t (*_rx_handler)(void* element, EtherAddress *src, char *buffer, int32_t size, bool is_neighbour,
                           uint8_t fwd_rate, uint8_t rev_rate);

    HandlerInfo(void *element,int protocol,
                int32_t (*tx_handler)(void* element, const EtherAddress *src, char *buffer, int32_t size),
                int32_t (*rx_handler)(void* element, EtherAddress *src, char *buffer, int32_t size, bool is_neighbour,
                                                                            uint8_t fwd_rate, uint8_t rev_rate)) {

      _element = element;
      _protocol = protocol;
      _tx_handler = tx_handler;
      _rx_handler = rx_handler;
    }
  };

  typedef HashMap<EtherAddress, uint8_t> BadTable;
  typedef BadTable::const_iterator BTIter;

 public:

  BRN2LinkStat();
  ~BRN2LinkStat();

  const char *class_name() const    { return "BRN2LinkStat"; }
  const char *processing() const    { return PUSH; }
  const char *port_count() const    { return "1/0-2"; }
  const char *flow_code() const     { return "x/y"; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void take_state(Element *, ErrorHandler *);

  Packet *simple_action(Packet *);

  void run_timer(Timer*);

  void add_handlers();

  String read_schema();
  String read_bcast_stats();
  String bad_nodes();
  int update_probes(String probes);

  BRN2Device *_dev;

  uint32_t _tau;       // msecs
  uint32_t _period;    // msecs (time between 2 linkprobes
  uint32_t _seq;       // sequence number

  BRN2GenericMetric **_metrics;
  int _metrics_size;
  /* just for metrics during configure/initialize*/
  String _metric_str;

  // record probes received from other hosts
  Vector <EtherAddress> _neighbors;
  // sometimes it is not possible to put the complete information of all my neighbors into the probe packets;
  // so we point to the next neighbor
  int _next_neighbor_to_add;

  void get_neighbors(Vector<EtherAddress> *ether_addrs) {
    for (ProbeMap::const_iterator i = _bcast_stats.begin(); i.live(); i++)
      ether_addrs->push_back(i.key());
  }

  bool is_neighbour(EtherAddress *n);
  // map contains information about the link quality to all my neighbors
  ProbeMap _bcast_stats;
  void add_bcast_stat(EtherAddress, const link_probe &);

  void update_link(const EtherAddress &from, EtherAddress &to, Vector<BrnRateSize> &rs,
                         Vector<uint8_t> &fwd, Vector<uint8_t> &rev, uint32_t seq,
                         uint8_t update_mode);

  void send_probe_hook();
  void send_probe();
  static void static_send_hook(Timer *, void *e) { ((BRN2LinkStat *) e)->send_probe_hook(); }

  Timer _timer;
  Timer _stale_timer;

  Timestamp _next;
  Timestamp _start;

  Vector <BrnRateSize> _ads_rs;
  int _ads_rs_index;

  uint8_t *_brn_rsp_packet_buf;
  uint32_t _brn_rsp_packet_buf_size;
  BrnRateSizeIndexMap _brn_rsp_imap;

  /* supported rates */
  BrnAvailableRates *_rtable;

  /* keeps track of neighbors nodes with wrong protocol */
  BadTable _bad_table;

  Vector <HandlerInfo> _reg_handler;
  int32_t registerHandler(void *element, int protocolId,
                          int (*tx_handler)(void* element, const EtherAddress *src, char *buffer, int size),
                          int (*rx_handler)(void* element, EtherAddress *src, char *buffer, int size, bool is_neighbour,
                                            uint8_t fwd_rate, uint8_t rev_rate));

  int32_t deregisterHandler(int32_t handle, int protocolId);

  int get_rev_rate(EtherAddress *ea);

  uint32_t _stale;
  void reset();
  void clear_stale();

 private:
  Vector<BrnRateSize> _rates_v;
  Vector<uint8_t> _fwd_v;
  Vector<uint8_t> _rev_v;


};

CLICK_ENDDECLS
#endif
