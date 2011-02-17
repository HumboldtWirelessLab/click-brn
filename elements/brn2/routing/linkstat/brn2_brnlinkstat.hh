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
#include "metric/brnettmetric.hh"

#include "elements/brn2/brnelement.hh"

CLICK_DECLS

static const uint8_t _ett2_version = 0x02;

#define LINKSTAT_DEFAULT_STALE 10000

class BrnRateSize {
 public:
  uint16_t _rate;
  uint16_t _size;
  BrnRateSize(uint16_t r, uint16_t s): _rate(r), _size(s) {};

  inline bool operator==(BrnRateSize other)
  {
    return (other._rate == _rate && other._size == _size);
  }
};


class BRN2LinkStat : public BRNElement {

public:

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

    uint16_t _rate;        // rate
    uint16_t _size;        // size
    uint32_t _seq;         // sequence number
    uint32_t _tau;         // this node's loss-rate averaging period, in msecs
    uint16_t _period;      // period of this node's probe broadcasts, in msecs
    uint8_t _num_probes;   // number of probes monitored by this node
    uint8_t _num_links;    // number of wifi_link_entry entries following

    link_probe() { memset(this, 0x0, sizeof(this)); }
  };

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
  };

  struct link_info {
    uint16_t _size;   //linkprobe size
    uint16_t _rate;   //linkprobe rate
    uint8_t _fwd;     //fwd ratio
    uint8_t _rev;     //rev ratio
  };

  /**************** PROBELIST ********************/
  /* Stores information about the probes of neighbouring nodes */

  struct probe_t {
    struct timeval _when;
    uint32_t _seq;
    uint16_t _rate;
    uint16_t _size;

    probe_t(const struct timeval &t,
            uint32_t s,
            uint8_t r,
            uint16_t sz) : _when(t), _seq(s), _rate(r), _size(sz) { }
  };


  struct probe_list_t {
    EtherAddress _ether;
    uint32_t _period;     // period of this node's probes, as reported by the node
    uint32_t _tau;        // this node's stats averaging period, as reported by the node
    uint8_t  _num_probes; // number of probes_types sent by the node
    uint32_t _seq;        //highest sequence numbers

    /*
     * Information about probes
     */
    Vector<BrnRateSize> _probe_types;

    Vector<uint8_t> _fwd_rates;

    struct timeval _last_rx;
    DEQueue<probe_t> _probes;          // most recently received probes

    probe_list_t(const EtherAddress &et, unsigned int per, unsigned int t) :
        _ether(et),
        _period(per),
        _tau(t),
        _seq(0)
    { }

    probe_list_t() : _period(0), _tau(0), _seq(0) { }

    uint8_t rev_rate(struct timeval start, int rate, int size) {
      struct timeval now = Timestamp::now().timeval();
      struct timeval p = { _tau / 1000, 1000 * (_tau % 1000) };
      struct timeval earliest;

      timersub(&now, &p, &earliest);

      if (_period == 0) {
        click_chatter("period is 0\n");
        return 0;
      }

      uint32_t num = 0;
      for (int i = _probes.size() - 1; i >= 0; i--) {
        if (timercmp(&earliest, &(_probes[i]._when), >)) break;
        if ( _probes[i]._size == size && _probes[i]._rate == rate) num++;
      }

      struct timeval since_start;
      timersub(&now, &start, &since_start);

      uint32_t ms_since_start = max(0, since_start.tv_sec * 1000 + since_start.tv_usec / 1000);
      uint32_t fake_tau = min(_tau, ms_since_start);
      assert(_probe_types.size());
      uint32_t num_expected = max(1,min((fake_tau / _period),(_seq / _num_probes)));

      return (uint8_t)(min(100, 100 * num / num_expected));
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
    int _protocol;
    int (*_handler)(void *element, EtherAddress *ea, char *buffer, int size, bool direction);

    HandlerInfo(void *element,int protocol, int (*handler)(void *element , EtherAddress *ea,
                char *buffer, int size, bool direction)) {
      _element = element;
      _protocol = protocol;
      _handler = handler;
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
  void brn2add_jitter2(unsigned int max_jitter, struct timeval *t);

  void add_handlers();

  String read_schema();
  String read_bcast_stats();
  String bad_nodes();
  int update_probes(String probes, ErrorHandler *errh);

  BRN2Device *_dev;

  uint32_t _tau;       // msecs
  uint32_t _period;    // msecs (time between 2 linkprobes
  uint32_t _seq;       // sequence number

  class BRNETTMetric *_ett_metric;
  class BRN2ETXMetric *_etx_metric;

  // record probes received from other hosts
  Vector <EtherAddress> _neighbors;
  // sometimes it is not possible to put the complete information of all my neighbors into the probe packets;
  // so we point to the next neighbor
  int _next_neighbor_to_add;

  void get_neighbors(Vector<EtherAddress> *ether_addrs) {
    for (ProbeMap::const_iterator i = _bcast_stats.begin(); i.live(); i++)
      ether_addrs->push_back(i.key());
  }

  // map contains information about the link quality to all my neighbors
  ProbeMap _bcast_stats;
  void add_bcast_stat(EtherAddress, const link_probe &);

  void update_link(const EtherAddress from, EtherAddress to, Vector<BrnRateSize> rs,
                         Vector<uint8_t> fwd, Vector<uint8_t> rev, uint32_t seq);

  void send_probe_hook();
  void send_probe();
  static void static_send_hook(Timer *, void *e) { ((BRN2LinkStat *) e)->send_probe_hook(); }

  Timer *_timer;
  Timer _stale_timer;

  struct timeval _next;
  struct timeval _start;

  Vector <BrnRateSize> _ads_rs;
  int _ads_rs_index;

  class AvailableRates *_rtable;

  /* keeps track of neighbors nodes with wrong protocol */
  BadTable _bad_table;

  Vector <HandlerInfo> _reg_handler;
  int registerHandler(void *element, int protocolId, int (*handler)(void* element, EtherAddress *src, char *buffer, int size, bool direction));
  int deregisterHandler(int handler, int protocolId);

  int get_rev_rate(EtherAddress *ea);

  uint32_t _stale;
  void reset();
  void clear_stale();

};

CLICK_ENDDECLS
#endif
