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
#include <click/dequeue.hh>
#include <click/vector.hh>
#include <click/element.hh>
#include <click/glue.hh>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
#include <click/timer.hh>
#include <click/string.hh>
#include <click/timestamp.hh>
#include <elements/wifi/availablerates.hh>
#include "elements/brn2/routing/linkstat/brn2_brnlinktable.hh"
#include "elements/brn2/routing/identity/brn2_device.hh"
#include "elements/brn2/brn2.h"
#include "metric/brn2_brnetxmetric.hh"

CLICK_DECLS

class Timer;

static const uint8_t _ett2_version = 0x02;

class BrnRateSize {
 public:
  int _rate;
  int _size;
  BrnRateSize(int r, int s): _rate(r), _size(s) { };

  inline bool operator==(BrnRateSize other)
  {
    return (other._rate == _rate && other._size == _size);
  }
};


class BRN2LinkStat : public Element {

public:

  // Packet formats:

  // BRN2LinkStat packets have a link_probe header immediately following
  // the ethernet header.  num_links link_entries follow the
  // link_probe header.

  enum {
    PROBE_AVAILABLE_RATES = (1<<0),
  };

  struct link_probe {
    uint8_t _version;
    uint8_t __pad; // bring size from 47 to 48 bytes, do not remove: otherwise click_in_cksum will not work on win32 (or fix alignment)
    uint16_t _cksum;     // internet checksum
    uint16_t _psz;       // total packet size, excluding brn header

    uint16_t _rate;
    uint16_t _size;
    uint8_t _ether[6];
    uint32_t _flags;
    uint32_t _seq;
    uint32_t _period;      // period of this node's probe broadcasts, in msecs
    uint32_t _tau;             // this node's loss-rate averaging period, in msecs
    uint32_t _sent;        // how many probes this node has sent
    uint32_t _num_probes;  // number of probes monitored by this node
    uint32_t _num_links;   // number of wifi_link_entry entries following

    link_probe() { memset(this, 0x0, sizeof(this)); }
  };

  struct link_info {
    uint16_t _size;
    uint8_t _rate;
    uint8_t _fwd;
    uint8_t _rev;
  };

  struct link_entry {
    uint8_t _ether[6];
    uint8_t _num_rates;
    uint32_t _seq;
    uint32_t _age;
    link_entry() { }
    link_entry(EtherAddress ether) {
      memcpy(_ether, ether.data(), 6);
    }
  };

public:
  uint32_t _tau;    // msecs
  unsigned int _period; // msecs

  unsigned int _seq;
  uint32_t _sent;
  BRN2Device *_me;

  void take_state(Element *, ErrorHandler *);
  void get_neighbors(Vector<EtherAddress> *ether_addrs) {
      for (ProbeMap::const_iterator i = _bcast_stats.begin(); i.live(); i++)
          ether_addrs->push_back(i.key());
  }

//  class BRNETTMetric *_ett_metric;
  class BRN2ETXMetric *_etx_metric;
  uint16_t _et;     // This protocol's ethertype

  struct timeval _start;
  // record probes received from other hosts
  struct probe_t {
    struct timeval _when;  
    uint32_t   _seq;
    uint8_t _rate;
    uint16_t _size;
    probe_t(const struct timeval &t,
      uint32_t s,
      uint8_t r,
      uint16_t sz) : _when(t), _seq(s), _rate(r), _size(sz) { }
  };


  struct probe_list_t {
    EtherAddress _ether;
    int _period;   // period of this node's probes, as reported by the node
    uint32_t _tau;      // this node's stats averaging period, as reported by the node
    int _sent;
    int _num_probes;
    uint32_t _seq;
    Vector<BrnRateSize> _probe_types;

    Vector<int> _fwd_rates;

    struct timeval _last_rx;
    DEQueue<probe_t> _probes;   // most recently received probes
    probe_list_t(const EtherAddress &et, unsigned int per, unsigned int t) : 
      _ether(et),
      _period(per),
      _tau(t),
      _sent(0)
    { }
    probe_list_t() : _period(0), _tau(0) { }

    int rev_rate(struct timeval start, int rate, int size) {
      struct timeval now;
      struct timeval p = { _tau / 1000, 1000 * (_tau % 1000) };
      struct timeval earliest;
      now = Timestamp::now().timeval();
      timersub(&now, &p, &earliest);

      if (_period == 0) {
        click_chatter("period is 0\n");
        return 0;
      }
      int num = 0;
      for (int i = _probes.size() - 1; i >= 0; i--) {
        if (timercmp(&earliest, &(_probes[i]._when), >)) {
          break;
        }
        if ( _probes[i]._size == size &&
            _probes[i]._rate == rate) {
          num++;
        }
      }

      struct timeval since_start;
      timersub(&now, &start, &since_start);

//      uint32_t ms_since_start = MAX(0, since_start.tv_sec * 1000 + since_start.tv_usec / 1000);
//      uint32_t fake_tau = MIN(_tau, ms_since_start);
      uint32_t ms_since_start = max(0, since_start.tv_sec * 1000 + since_start.tv_usec / 1000);
      uint32_t fake_tau = min(_tau, ms_since_start);
      assert(_probe_types.size());
      int num_expected = fake_tau / _period;

      if (_sent / _num_probes < num_expected) {
      	num_expected = _sent / _num_probes;
      }
      if (!num_expected) {
        num_expected = 1;
      }

//      return MIN(100, 100 * num / num_expected);
      return min(100, 100 * num / num_expected);

    }
  };

  class HandlerInfo {
   public:
    void *_element;
    int _protocol;
    int (*_handler)(void *element, EtherAddress *ea, char *buffer, int size, bool direction);

    HandlerInfo(void *element,int protocol, int (*handler)(void *element , EtherAddress *ea, char *buffer, int size, bool direction)) {
      _element = element;
      _protocol = protocol;
      _handler = handler;
    }
  };

public:
  int _debug;
  // Per-sender map of received probes.
  typedef HashMap<EtherAddress, probe_list_t> ProbeMap;
  // map contains information about the link quality to all my neighbors
  ProbeMap _bcast_stats;

  Vector <EtherAddress> _neighbors;
  // sometimes it is not possible to put the complete information of all my neighbors into the probe packets;
  // so we point to the next neighbor
  int _next_neighbor_to_ad;

  void add_bcast_stat(EtherAddress, const link_probe &);

  void update_link(EtherAddress from, EtherAddress to, Vector<BrnRateSize> rs, Vector<int> fwd, Vector<int> rev, uint32_t seq);
  void send_probe_hook();
  void send_probe();
  static void static_send_hook(Timer *, void *e) { ((BRN2LinkStat *) e)->send_probe_hook(); }

  Timer *_timer;
  Timer _stale_timer;

  void run_timer(Timer*);
  struct timeval _next;

  Vector <BrnRateSize> _ads_rs;
  int _ads_rs_index;

  class AvailableRates *_rtable;

  typedef HashMap<EtherAddress, uint8_t> BadTable;
  typedef BadTable::const_iterator BTIter;

  /* keeps track of neighbors nodes with wrong protocol */
  BadTable _bad_table;

  /* Logging */
  bool _log;
  Timer _log_timeout_timer;
  int _log_interval; // in secs

  Vector <HandlerInfo> _reg_handler;

 public:
  String bad_nodes();
  String read_bcast_stats(bool with_pos);

  int get_etx(EtherAddress);
  int get_etx(int, int);

  void update_links(EtherAddress ip);
  BRN2LinkStat();
  ~BRN2LinkStat();

  const char *class_name() const    { return "BRN2LinkStat"; }
  const char *processing() const    { return PUSH; }
  const char *port_count() const    { return "1/0-1"; }
  const char *flow_code() const     { return "x/y"; }

  void add_handlers();

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  int registerHandler(void *element, int protocolId, int (*handler)(void* element, EtherAddress *src, char *buffer, int size, bool direction));
  int deregisterHandler(int handler, int protocolId);

  int get_rev_rate(EtherAddress *ea);

  void reset();
  void clear_stale();
  Packet *simple_action(Packet *);

  void run_log_timer();
};

CLICK_ENDDECLS
#endif
