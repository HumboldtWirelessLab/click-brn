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

/*
 * brnlinkstat.{cc,hh} -- monitors link loss rates
 * A. Zubow
 * Adapted from wifi/sr/ettstatt.{cc,hh} to brn (many thanks to John Bicket).
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include <tinyxml/tinyxml.h>

#include "brn2_brnlinkstat.hh"

CLICK_DECLS

// packet data should be 4 byte aligned
#define ASSERT_ALIGNED(p) assert(((unsigned long)(p) % 4) == 0)

BRN2LinkStat::BRN2LinkStat()
  : _dev(),
    _tau(10000),
    _period(1000),
    _seq(0),
    _ett_metric(0),
    _etx_metric(0),
    _next_neighbor_to_add(0),
    _timer(0),
    _stale_timer(this),
    _ads_rs_index(0),
    _rtable(0),
    _stale(LINKSTAT_DEFAULT_STALE)
{
  BRNElement::init();
}

BRN2LinkStat::~BRN2LinkStat()
{
  if (_timer)
    delete _timer;
  _timer = NULL;

  _bcast_stats.clear();
  _neighbors.clear();
  _ads_rs.clear();
  _bad_table.clear();
}

int
BRN2LinkStat::configure(Vector<String> &conf, ErrorHandler* errh)
{
  String probes;
  int res = cp_va_kparse(conf, this, errh,
              "DEVICE", cpkP+cpkM, cpElement, &_dev,
              "PERIOD", cpkP+cpkM, cpUnsigned, &_period,
              "TAU", cpkP+cpkM, cpUnsigned, &_tau,
              "PROBES", cpkP+cpkM, cpString, &probes,
              "RT", cpkP+cpkM, cpElement, &_rtable,
              "ETT", cpkP, cpElement, &_ett_metric,
              "ETX", cpkP, cpElement, &_etx_metric,
              "STALE", cpkP, cpInteger, &_stale,
              "DEBUG", cpkP, cpInteger, &_debug,
            cpEnd);


  if (res < 0) return res;

  res = update_probes(probes, errh);
  if (res < 0) return res;

  if (!_dev || !_dev->cast("BRN2Device"))
    return errh->error("BRN2Device element is not provided or not a BRN2Device");

  if (_ett_metric && !_ett_metric->cast("BRNETTMetric"))
    return errh->error("BRNETTMetric element is not a BRNETTMetric");

  if (_etx_metric && !_etx_metric->cast("BRN2ETXMetric"))
    return errh->error("BRNETXMetric element is not a BRNETXMetric");

  if (!_rtable || !_rtable->cast("BrnAvailableRates"))
    return errh->error("RT element is not a BrnAvailableRates");

  return res;
}

/* Called by the Click framework */
int
BRN2LinkStat::initialize(ErrorHandler *errh)
{
  if (noutputs() > 0) {
    if (!_dev) return errh->error("Source Ethernet address (NodeIdentity) must be specified to send probes");

    click_srandom(_dev->getEtherAddress()->hashcode());

    _timer = new Timer(static_send_hook, this);
    _timer->initialize(this);

    _stale_timer.initialize(this);
    _stale_timer.schedule_now();

    _next = Timestamp::now().timeval();

    _next.tv_sec++;
    brn2add_jitter2( (_period / 10), &_next);

    BRN_DEBUG("next %s", Timestamp(_next).unparse().c_str());

    _timer->schedule_at(_next);
  }

  reset();

  return 0;
}

void
BRN2LinkStat::run_timer(Timer*)
{
  clear_stale();
  _stale_timer.schedule_after_msec(_stale);
}

void
BRN2LinkStat::brn2add_jitter2(unsigned int max_jitter, struct timeval *t)
{
  struct timeval jitter;
  unsigned j = (unsigned) (click_random() % (max_jitter + 1));
  unsigned int delta_us = 1000 * j;
  timerclear(&jitter);
  jitter.tv_usec += delta_us;
  jitter.tv_sec +=  jitter.tv_usec / 1000000;
  jitter.tv_usec = (jitter.tv_usec % 1000000);
  if (click_random() & 1) {
    timeradd(t, &jitter, t);
  } else {
    timersub(t, &jitter, t);
  }
  return;
    }

void
BRN2LinkStat::take_state(Element *e, ErrorHandler *errh)
{
  /*
   * take_state gets called after
   * --configure
   * --initialize
   * so we may need to unschedule probe timers
   * and sync them up so the rates don't get
   * screwed up.
  */
  BRN2LinkStat *q = (BRN2LinkStat *)e->cast("BRNLinkStat");
  if (!q) {
    errh->error("Couldn't cast old BRNLinkStat");
    return;
  }

  _neighbors = q->_neighbors;
  _bcast_stats = q->_bcast_stats;
  _start = q->_start;

  struct timeval now;
  now = Timestamp::now().timeval();

  if (timercmp(&now, &q->_next, <)) {
    _timer->unschedule();
    _timer->schedule_at(q->_next);
    _next = q->_next;
  }

}

void
BRN2LinkStat::update_link(const EtherAddress from, EtherAddress to, Vector<BrnRateSize> rs, Vector<uint8_t> fwd, Vector<uint8_t> rev, uint32_t seq)
{
  if (_ett_metric) {
    BRN_DEBUG(" * update ett_metric.");
    _ett_metric->update_link(from, to, rs, fwd, rev, seq);
  }

  if (_etx_metric) {
    BRN_DEBUG(" * update etx_metric.");
    _etx_metric->update_link(from, to, rs, fwd, rev, seq);
  }
}

/* called at {@link _next} time */
void
BRN2LinkStat::send_probe_hook()
{
  BRN_DEBUG("send_probe_hook()");

  if (!_ads_rs.size()) {
    BRN_WARN(" no probes to send at.");
    return;
  }

  int p = _period / _ads_rs.size(); //period (msecs); _ads_rs.size(): probe count
  unsigned max_jitter = p / 10;

  struct timeval period;
  timerclear(&period);

  send_probe();

//  struct timeval period;
//  timerclear(&period);

  period.tv_usec += (p * 1000);
  period.tv_sec += period.tv_usec / 1000000;
  period.tv_usec = (period.tv_usec % 1000000);

  timeradd(&period, &_next, &_next);
  brn2add_jitter2(max_jitter, &_next);
  _timer->schedule_at(_next);


}

/*
 * This method is called periodically (_period / _ads_rs.size() +/- jitter msecs)
 * to inform neighbor nodes about his links.
 */
void
BRN2LinkStat::send_probe()
{
  if (!_ads_rs.size()) {
    BRN_WARN(" no probes to send at.");
    return;
  }

  uint16_t size = _ads_rs[_ads_rs_index]._size;   // size of the probe packet (e.g. 1000 byte)
  uint16_t rate = _ads_rs[_ads_rs_index]._rate;   // probe packet's transmission rate
  uint32_t seq = ++_seq;

  if ( size < (sizeof(click_brn) + sizeof(struct link_probe))) {
    BRN_ERROR(" cannot send packet size %d: min is %d\n", size, (sizeof(click_brn) + sizeof(struct link_probe)));
    return;
  }

  // construct probe packet
  //WritablePacket *p = Packet::make(128 /*headroom*/,NULL /* *data*/, size + 2, 32); //alignment
  WritablePacket *p = BRNElement::packet_new(128 /*headroom*/,NULL /* *data*/, size + 2, 32); //alignment
  if (p == 0) {
    BRN_ERROR(" cannot make packet!");
    return;
  }

  ASSERT_ALIGNED(p->data());
  p->pull(2);
  memset(p->data(), 0, p->length());

  // each probe packet is annotated with a timestamp
  struct timeval now = Timestamp::now().timeval();
  p->set_timestamp_anno(now);

  /*
   * fill brn header header // think about this; probe packets only available for one hop
   */
  BRNProtocol::set_brn_header(p->data(), BRN_PORT_LINK_PROBE, BRN_PORT_LINK_PROBE, p->length(), 1, BRN_TOS_BE );
  BRNPacketAnno::set_tos_anno(p, BRN_TOS_BE);

  /*
   * Fill linkprobe header
   */
  link_probe *lp = (struct link_probe *) (p->data() + sizeof(click_brn));
  lp->_version = _ett2_version;
  lp->_flags = 0;

  lp->_cksum = 0;

  lp->_rate = htons(rate);
  lp->_size = htons(size);

  lp->_seq = htonl(seq);
  lp->_tau = htonl(_tau);

  lp->_period = htons(_period);
  lp->_num_probes = (uint8_t)_ads_rs.size(); // number of probes monitored by this node (e.g. 2 in case of (2 60, 22 1400))

  uint8_t *ptr =  (uint8_t *)(lp + 1); // points to the start of the payload area (array of wifi_link_entry entries)
  uint8_t *end = (uint8_t *)p->data() + p->length(); // indicates end of payload area; neccessary to preserve max packet size

  BRN_DEBUG("Pack: Start: %x End: %x",ptr,end);

  /*
   * Add Available rates
   */
  if (_rtable) {
    // my wireless device is capable to transmit packets at rates (e.g. 2 4 11 12 18 22 24 36 48 72 96 108)
    Vector<MCS> rates = _rtable->lookup(*(_dev->getEtherAddress()));

    if (rates.size() && 1 + ptr + rates.size() < end) { // put my available rates into the packet
      ptr[0] = rates.size(); // available rates count
      ptr++;
      int x = 0;
      while (ptr < end && x < rates.size()) {
        ptr[x] = rates[x].get_packed_8();
        x++;
      }
      ptr += rates.size();
      lp->_flags |= PROBE_AVAILABLE_RATES; // indicate that rates are available
    }
  }

  /*
   * add fwd information for each neighbour and rate/sizeof
   */
  uint8_t num_entries = 0;   // iterate over my neighbor list and append link information to packet

  while (ptr < end && num_entries < _neighbors.size()) {

    _next_neighbor_to_add = (_next_neighbor_to_add + 1) % _neighbors.size();

    probe_list_t *probe = _bcast_stats.findp(_neighbors[_next_neighbor_to_add]);

    if (!probe) {
      BRN_DEBUG(" lookup for %s, %d failed in ad", _neighbors[_next_neighbor_to_add].unparse().c_str(), _next_neighbor_to_add);
    } else {
      int size = sizeof(link_entry) + probe->_probe_types.size() * sizeof(link_info);
      if (ptr + size > end) break;

      num_entries++;
      link_entry *entry = (struct link_entry *)(ptr); // append link_entry
      memcpy(entry->_ether, probe->_ether.data(), 6);
      entry->_num_rates = probe->_probe_types.size(); // probe type count
      entry->_flags = 0;                              // TODO: host is missing ??
      entry->_seq = htonl(probe->_seq);               // append last known seq
      entry->_age = htons(0);                         // time since last rx

      ptr += sizeof(link_entry);

      Vector<BrnRateSize> rates;
      Vector<uint8_t> fwd;
      Vector<uint8_t> rev;

      for (int x = 0; x < probe->_probe_types.size(); x++) {      // append link info
        BrnRateSize rs = probe->_probe_types[x];
        link_info *lnfo = (struct link_info *) (ptr + x * sizeof(link_info));
        lnfo->_size = htons(rs._size);                            // probe packet size
        lnfo->_rate = htons(rs._rate);                            // probe packet transmission rate
        lnfo->_fwd = probe->_fwd_rates[x];                        // forward delivery ratio d_f
        lnfo->_rev = probe->rev_rate(_start, rs._rate, rs._size); // reverse delivery ratio

        rates.push_back(rs);
        fwd.push_back(lnfo->_fwd);
        rev.push_back(lnfo->_rev);
      }
      // update my own link table
      update_link(*(_dev->getEtherAddress()), probe->_ether, rates, fwd, rev, probe->_seq);

      ptr += probe->_probe_types.size() * sizeof(link_info);
    }
  }

  if ( num_entries > 0 ) lp->_flags |= PROBE_REV_FWD_INFO; // indicate that info are available
  lp->_num_links = num_entries; // number of transmitted link_entry elements

  uint16_t space_left = (end-ptr);
  BRN_DEBUG("Handler pre Ptr: %x End: %x space left: %d",ptr,end,space_left);

  if ( _reg_handler.size() > 0 && ((space_left-4) > 0) ) {

    lp->_flags |= PROBE_HANDLER_PAYLOAD; // indicate that handler_info are available

    for ( int h = 0; h < _reg_handler.size(); h++ ) {
      int res = _reg_handler[h]._tx_handler(_reg_handler[h]._element, _dev->getEtherAddress(), (char*)&(ptr[3]), (space_left-4));
      if ( res > (space_left-4) ) {
        BRN_WARN("Handler %d want %d but %d was left",_reg_handler[h]._protocol,res,space_left-4);
        res = 0;
      }
      if ( res > 0 ) {
        *ptr = _reg_handler[h]._protocol;
        ptr++;
        uint16_t *s = (uint16_t *)ptr;
        *s = htons(res);
        ptr += 2 + res;
        space_left -= 3 + res;
      }
    }

    *ptr = 255; // indicate handler end
  }

  BRN_DEBUG("Handler post Ptr: %x End: %x Value: %d",ptr,end,*ptr);

  lp->_cksum = click_in_cksum((unsigned char *)lp, p->length() - sizeof(click_brn));

  /*
   * WIFI Stuff
   */
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
  ceh->magic = WIFI_EXTRA_MAGIC;
  MCS mcs;
  mcs.set_packed_16(rate);
  mcs.setWifiRate(ceh, 0);                                    // this packet should be transmitted at the given rate
  ceh->max_tries = 1;
  ceh->max_tries1 = 0;
  ceh->max_tries2 = 0;
  ceh->max_tries3 = 0;

  /*
   * Prepare evereything for the next round
   */
  _ads_rs_index = (_ads_rs_index + 1) % _ads_rs.size(); // points to the next probe packet type

  checked_output_push(0, p);
}

/*
 * Processes an incoming probe packet.
 */
Packet *
BRN2LinkStat::simple_action(Packet *p)
{
  BRN_DEBUG(" * simple_action()");

  struct timeval now = Timestamp::now().timeval();
  click_ether *eh = (click_ether *) p->ether_header();
  click_brn *brn = (click_brn *) p->data();

  /* best_xxx used to set fwd and rev for rx_handler (other elements) */
  uint8_t best_fwd = 0;
  uint8_t best_rev = 0;

  EtherAddress src_ea = EtherAddress(eh->ether_shost);
  if (p->length() < (sizeof(click_brn) + sizeof(link_probe))) {
    BRN_ERROR("packet is too small");
    BRNElement::packet_kill(p);
    //p->kill();
    return 0;
  }

  if (brn->dst_port != BRN_PORT_LINK_PROBE) { // wrong packet type
    BRN_ERROR("got non-BRNLinkStat packet type");
    BRNElement::packet_kill(p);
    //p->kill();
    return 0;
  }

  link_probe *lp = (link_probe *)(p->data() + sizeof(click_brn));
  if (lp->_version != _ett2_version) {
    static bool version_warning = false;
    _bad_table.insert(EtherAddress(eh->ether_shost), lp->_version);
    if (!version_warning) {
      version_warning = true;
      BRN_WARN(" unknown lp version %x from %s",
        lp->_version, EtherAddress(eh->ether_shost).unparse().c_str());
    }

    BRNElement::packet_kill(p);
    //p->kill();
    return 0;
  }

  if (click_in_cksum((unsigned char *) lp, p->length() - sizeof(click_brn)) != 0) {
    BRN_WARN("failed checksum. Src: %s",src_ea.unparse().c_str());
    checked_output_push(1, p);
    return 0;
  }

  if (p->length() < ntohs(lp->_size)) {
    BRN_WARN("packet is smaller (%d) than it claims (%u)", p->length(), ntohs(lp->_size));
  }

  if (src_ea == *(_dev->getEtherAddress())) {
    BRN_WARN("got own packet; drop it. %s", src_ea.unparse().c_str());
    BRNElement::packet_kill(p);
    //p->kill();
    return 0;
  }

  // Default to the rate given in the packet.
  uint16_t rate = ntohs(lp->_rate);
  struct click_wifi_extra *ceh = (struct click_wifi_extra *)WIFI_EXTRA_ANNO(p);

  if (WIFI_EXTRA_MAGIC == ceh->magic)  // check if extra header is present !!!
  {
    MCS mcs = MCS(ceh,0);
    if (mcs.get_packed_16() != rate) {
      BRN_WARN("packet says rate %d is %d; drop it.", rate, mcs.get_packed_16());
      BRNElement::packet_kill(p);
      //p->kill();
      return 0;
    }
  }
  else
  {
    BRN_FATAL("extra header not set (Forgotten {Extra|RadioTap|AthDesc|Prism2}Decap?).");
  }

  probe_t probe(now, ntohl(lp->_seq), rate, ntohs(lp->_size));
  uint32_t new_period = ntohs(lp->_period);
  probe_list_t *l = _bcast_stats.findp(src_ea); // fetch sender's probe list

  if (!l) { // new neighbor
    _bcast_stats.insert(src_ea, probe_list_t(src_ea, new_period, ntohl(lp->_tau)));
    l = _bcast_stats.findp(src_ea);
    _neighbors.push_back(src_ea);    // add into the neighbors vector
  } else if (l->_period != new_period) {
    BRN_INFO("%s has changed its link probe period from %u to %u; clearing probe info", src_ea.unparse().c_str(),
                                                                                        l->_period, new_period);
    l->clear();
  } else if (l->_tau != ntohl(lp->_tau)) {
    BRN_INFO("%s has changed its link tau from %u to %u; clearing probe info", src_ea.unparse().c_str(), l->_tau,
                                                                                                  ntohl(lp->_tau));
    l->clear();
  }

  if ( l->_seq >= ntohl(lp->_seq) ) {
    BRN_INFO("%s has reset ( old: %d  new: %d ); clearing probe info", src_ea.unparse().c_str(), l->_seq, ntohl(lp->_seq));
    l->clear();
  }

  BrnRateSize rs = BrnRateSize(rate, ntohs(lp->_size));

  int x = 0;
  for (x = 0; x < l->_probe_types.size(); x++) {
    if (rs == l->_probe_types[x]) break;
  }

  if (x == l->_probe_types.size()) { // entry not found
    l->_probe_types.push_back(rs);
    l->_fwd_rates.push_back(0);
  }

  l->_period = new_period;
  l->_tau = ntohl(lp->_tau);
  l->_last_rx = now;
  l->_seq = ntohl(lp->_seq);
  l->_num_probes = lp->_num_probes;
  l->_probes.push_back(probe);

  // keep stats for at least the averaging period; kick old probes
  while ((unsigned) l->_probes.size() &&
        now.tv_sec - l->_probes[0]._when.tv_sec > (signed) (1 + (l->_tau / 1000)))
    l->_probes.pop_front();

  // pointer points to start and end of packet payload
  uint8_t *ptr =  (uint8_t *) (lp + 1);
  uint8_t *end  = (uint8_t *) p->data() + p->length();

  BRN_DEBUG("Start: %x End: %x",ptr,end);

  if (lp->_flags & PROBE_AVAILABLE_RATES) { // available rates where transmitted
    int num_rates = ptr[0];
    Vector<MCS> rates;
    ptr++;
    int x = 0;
    while (ptr < end && x < num_rates) {
      MCS rate;
      rate.set_packed_8(ptr[x]);
      rates.push_back(rate);
      x++;
    }
    ptr += num_rates;
    if(_rtable) { // store neighbor node's available rates
      _rtable->insert(EtherAddress(eh->ether_shost), rates);
    }
  }

  if (lp->_flags & PROBE_REV_FWD_INFO) { // linkprobe info where transmitted
    uint8_t link_number = 0;
    // fetch link entries
    while (ptr < end && link_number < lp->_num_links) {
      link_number++;
      link_entry *entry = (struct link_entry *)(ptr);
      EtherAddress neighbor = EtherAddress(entry->_ether);
      int num_rates = entry->_num_rates;

      BRN_DEBUG("on link number %d / %d: neighbor %s, num_rates %d",
        link_number, lp->_num_links, neighbor.unparse().c_str(), num_rates);

      ptr += sizeof(struct link_entry);
      Vector<BrnRateSize> rates;
      Vector<uint8_t> fwd;
      Vector<uint8_t> rev;

      for (int x = 0; x < num_rates; x++) {
        struct link_info *nfo = (struct link_info *) (ptr + x * (sizeof(struct link_info)));

        BRN_DEBUG(" %s neighbor %s: size %d rate %d fwd %d rev %d",
          src_ea.unparse().c_str(), neighbor.unparse().c_str(), ntohs(nfo->_size), ntohs(nfo->_rate), nfo->_fwd, nfo->_rev);

        BrnRateSize rs = BrnRateSize(ntohs(nfo->_rate), ntohs(nfo->_size));
        // update other link stuff
        rates.push_back(rs);

        fwd.push_back(nfo->_fwd); // forward delivery ratio

        if (neighbor == *(_dev->getEtherAddress())) { // reverse delivery ratio is available -> use it.
          if ( nfo->_fwd > best_fwd ) best_fwd = nfo->_fwd;
          uint8_t rev_rate = l->rev_rate(_start, rates[x]._rate, rates[x]._size);
          if ( rev_rate > best_rev ) best_rev = rev_rate;
          rev.push_back(rev_rate);
        } else {
          rev.push_back(nfo->_rev);
        }

        if (neighbor == *(_dev->getEtherAddress())) {
          // set the fwd rate
          for (int x = 0; x < l->_probe_types.size(); x++) {
            if (rs == l->_probe_types[x]) {
              l->_fwd_rates[x] = nfo->_rev;
              break;
            }
          }
        }
      }
      int seq = ntohl(entry->_seq);

#ifdef LINKSTAT_EXTRA_DEBUG
#warning "Enable Extra Linstat debug"
      uint8_t zero_mac[] = { 0,0,0,0,0,0 }; 
         if ( (memcmp(src_ea.data(), zero_mac, 6) == 0) || (memcmp(neighbor.data(), zero_mac, 6) == 0) ) {
        BRN_WARN("Found zero mac");
        checked_output_push(1, p);
        return 0;
      }
#endif

      update_link(src_ea, neighbor, rates, fwd, rev, seq);
      ptr += num_rates * sizeof(struct link_info);
    }
  }

  BRN_DEBUG("Ptr: %x",ptr);

  if (lp->_flags & PROBE_HANDLER_PAYLOAD) { // handler info where transmitted
    while ( *ptr != 255 ) {
      uint8_t proto = *ptr;
      ptr++;
      uint16_t *sp = (uint16_t*)ptr;
      uint16_t s = ntohs(*sp);
      ptr += 2;

      for ( int h = 0; h < _reg_handler.size(); h++ ) {
        if ( proto == _reg_handler[h]._protocol )
          /*int res =*/ _reg_handler[h]._rx_handler(_reg_handler[h]._element, &src_ea, (char*)ptr, s,
           /*is_neighbour*/(best_fwd > 80) && (best_rev > 80), /*fwd_rate*/best_fwd , /*rev_rate*/best_rev);
      }

      ptr += s;
    }
  }

  //p->kill();
  BRNElement::packet_kill(p);

  return 0;
}


bool
BRN2LinkStat::is_neighbour(EtherAddress *n)
{
  for (int x = 0; x < _neighbors.size(); x++) {
    if ( _neighbors[x] == *n ) return true;
  }

  return false;
}

/* Returns some information about the nodes world model */
String
BRN2LinkStat::read_schema()
{
  Timestamp now = Timestamp::now();

  StringAccum sa;

//sa << "<?xml version='1.0' encoding='UTF-8'?>";
sa << "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'>";
sa << "<xs:element name='tau'>";
sa << "<xs:complexType>";
sa << "<xs:attribute name='value' type='xs:int' use='required'/>";
sa << "</xs:complexType>";
sa << "</xs:element>";
sa << "<xs:element name='probes'>";
sa << "<xs:complexType>";
sa << "<xs:sequence>";
sa << "<xs:element ref='probe' maxOccurs='unbounded'/>";
sa << "</xs:sequence>";
sa << "<xs:attribute name='time' type='xs:decimal' use='required'/>";
sa << "<xs:attribute name='id' type='xs:string' use='required'/>";
sa << "</xs:complexType>";
sa << "</xs:element>";
sa << "<xs:element name='probe'>";
sa << "<xs:complexType>";
sa << "<xs:attribute name='size' type='xs:short' use='required'/>";
sa << "<xs:attribute name='rate' type='xs:byte' use='required'/>";
sa << "</xs:complexType>";
sa << "</xs:element>";
sa << "<xs:element name='period'>";
sa << "<xs:complexType>";
sa << "<xs:attribute name='value' type='xs:int' use='required'/>";
sa << "</xs:complexType>";
sa << "</xs:element>";
sa << "<xs:element name='link_info'>";
sa << "<xs:complexType>";
sa << "<xs:attribute name='size' type='xs:short' use='required'/>";
sa << "<xs:attribute name='rev' type='xs:byte' use='required'/>";
sa << "<xs:attribute name='rate' type='xs:byte' use='required'/>";
sa << "<xs:attribute name='fwd' type='xs:byte' use='required'/>";
sa << "</xs:complexType>";
sa << "</xs:element>";
sa << "<xs:element name='link'>";
sa << "<xs:complexType>";
sa << "<xs:sequence>";
sa << "<xs:element ref='link_info' maxOccurs='unbounded'/>";
sa << "</xs:sequence>";
sa << "<xs:attribute name='to' type='xs:string' use='required'/>";
sa << "<xs:attribute name='tau' type='xs:int' use='required'/>";
sa << "<xs:attribute name='seq' type='xs:int' use='required'/>";
sa << "<xs:attribute name='period' type='xs:int' use='required'/>";
sa << "<xs:attribute name='last_rx' type='xs:decimal' use='required'/>";
sa << "</xs:complexType>";
sa << "</xs:element>";
sa << "<xs:element name='handlers'>";
sa << "<xs:complexType>";
sa << "<xs:sequence>";
sa << "<xs:element ref='handler' maxOccurs='unbounded'/>";
sa << "</xs:sequence>";
sa << "<xs:attribute name='elem_name' use='required'>";
sa << "<xs:simpleType>";
sa << "<xs:restriction base='xs:string'>";
sa << "<xs:enumeration value='BRN2LinkStat'/>";
sa << "</xs:restriction>";
sa << "</xs:simpleType>";
sa << "</xs:attribute>";
sa << "</xs:complexType>";
sa << "</xs:element>";
sa << "<xs:element name='handler'>";
sa << "<xs:complexType>";
sa << "<xs:choice>";
sa << "<xs:element ref='badnodes'/>";
sa << "<xs:element ref='entry'/>";
sa << "<xs:element ref='period'/>";
sa << "<xs:element ref='probes'/>";
sa << "<xs:element ref='tau'/>";
sa << "</xs:choice>";
sa << "<xs:attribute name='name' use='required'>";
sa << "<xs:simpleType>";
sa << "<xs:restriction base='xs:string'>";
sa << "<xs:enumeration value='bad_version'/>";
sa << "<xs:enumeration value='bcast_stats'/>";
sa << "<xs:enumeration value='period'/>";
sa << "<xs:enumeration value='probes'/>";
sa << "<xs:enumeration value='tau'/>";
sa << "</xs:restriction>";
sa << "</xs:simpleType>";
sa << "</xs:attribute>";
sa << "</xs:complexType>";
sa << "</xs:element>";
sa << "<xs:element name='entry'>";
sa << "<xs:complexType>";
sa << "<xs:sequence>";
sa << "<xs:element ref='link' maxOccurs='unbounded'/>";
sa << "</xs:sequence>";
sa << "<xs:attribute name='time' type='xs:decimal' use='required'/>";
sa << "<xs:attribute name='tau' type='xs:int' use='required'/>";
sa << "<xs:attribute name='seq' type='xs:int' use='required'/>";
sa << "<xs:attribute name='period' type='xs:int' use='required'/>";
sa << "<xs:attribute name='from' type='xs:string' use='required'/>";
sa << "</xs:complexType>";
sa << "</xs:element>";
sa << "<xs:element name='badnodes'>";
sa << "<xs:complexType>";
sa << "<xs:sequence>";
sa << "<xs:element ref='badnode' maxOccurs='unbounded'/>";
sa << "</xs:sequence>";
sa << "<xs:attribute name='time' type='xs:decimal' use='required'/>";
sa << "<xs:attribute name='id' type='xs:string' use='required'/>";
sa << "</xs:complexType>";
sa << "</xs:element>";
sa << "<xs:element name='badnode'>";
sa << "<xs:complexType>";
sa << "<xs:attribute name='version' type='xs:byte' use='required'/>";
sa << "<xs:attribute name='id' type='xs:string' use='required'/>";
sa << "</xs:complexType>";
sa << "</xs:element>";
sa << "</xs:schema>";

  return sa.take_string();
}

/* Returns some information about the nodes world model */
String
BRN2LinkStat::read_bcast_stats()
{
  Timestamp now = Timestamp::now();
  MCS rate;

  Vector<EtherAddress> ether_addrs;

  for (ProbeMap::const_iterator i = _bcast_stats.begin(); i.live(); i++) 
    ether_addrs.push_back(i.key());

  //sort
  click_qsort(ether_addrs.begin(), ether_addrs.size(), sizeof(EtherAddress), etheraddr_sorter);

  StringAccum sa;

  sa << "<entry from='" << *(_dev->getEtherAddress()) << "'";
  sa << " time='" << now.unparse() << "' seq='" << _seq << "' period='" << _period << "' tau='" << _tau << "' >\n";

  for (int i = 0; i < ether_addrs.size(); i++) {
    EtherAddress ether  = ether_addrs[i];
    probe_list_t *pl = _bcast_stats.findp(ether);

    sa << "\t<link to='" << ether << "'";
    sa << " seq='" << pl->_seq << "'";
    sa << " period='" << pl->_period << "'";
    sa << " tau='" << pl->_tau << "'";
    sa << " last_rx='" << now - pl->_last_rx << "'";
    sa << " >\n";

    for (int x = 0; x < pl->_probe_types.size(); x++) {
      sa << "\t\t<link_info size='" << pl->_probe_types[x]._size << "'";

      rate.set_packed_16(pl->_probe_types[x]._rate);
      uint32_t is_ht = (rate._is_ht)?(uint32_t)1:(uint32_t)0;
      sa << " rate='" << rate.to_string() << "' n='" << is_ht;
      sa <<  "' mcsindex='" << (uint32_t)rate._ridx << "' ht40='" << (uint32_t)rate._ht40 << "' sgi='" << (uint32_t)rate._sgi << "'";

      int rev_rate = pl->rev_rate(_start, pl->_probe_types[x]._rate,
                                 pl->_probe_types[x]._size);
      sa << " fwd='" << (uint32_t)pl->_fwd_rates[x] << "'";
      sa << " rev='" << (uint32_t)rev_rate << "'";
      sa << "/>\n";
    }
    sa << "\t</link>\n";
  }

  sa << "</entry>\n";

  return sa.take_string();
}


/* Returns the nodes operating on wrong protocol. */
String 
BRN2LinkStat::bad_nodes() {

  Timestamp now = Timestamp::now();
  StringAccum sa;

  sa << "<badnodes id='" << *(_dev->getEtherAddress()) << "'";
  sa << " time='" << now.unparse() << "'>\n";

  for (BadTable::const_iterator i = _bad_table.begin(); i.live(); i++) {
    uint8_t version = i.value();
    EtherAddress dst = i.key();
    sa << "<badnode id='" << dst.unparse().c_str() << "' version='" << (int) version << "' />\n";
  }

  sa << "</badnodes>\n";

  return sa.take_string();
}

// ----------------------------------------------------------------------------
// Helper methods
// ----------------------------------------------------------------------------
void
BRN2LinkStat::clear_stale()
{
  Vector<EtherAddress> new_neighbors;

  struct timeval now;

  now = Timestamp::now().timeval();
  for (int x = 0; x < _neighbors.size(); x++) {
    EtherAddress n = _neighbors[x];
    probe_list_t *l = _bcast_stats.findp(n);
    if (!l || (unsigned) now.tv_sec - l->_last_rx.tv_sec > 2 * l->_tau/1000) {
      BRN_DEBUG(" clearing stale neighbor %s age %d(brn_0)",
        n.unparse().c_str(), now.tv_sec - l->_last_rx.tv_sec);
      _bcast_stats.remove(n);
    } else {
      new_neighbors.push_back(n);
    }
  }

  _neighbors.clear();
  for (int x = 0; x < new_neighbors.size(); x++) {
    _neighbors.push_back(new_neighbors[x]);
  }

}
void
BRN2LinkStat::reset()
{
  _neighbors.clear();
  _bcast_stats.clear();
  _seq = 0;
  _start = Timestamp::now().timeval();
}

int
BRN2LinkStat::get_rev_rate(EtherAddress *ea)
{
  probe_list_t *probe = _bcast_stats.findp(*ea);

  if ( ! probe )  return 0;

  BrnRateSize rs = probe->_probe_types[0];

  return ( probe->rev_rate(_start, rs._rate, rs._size) ); // reverse delivery ratio
}

int32_t
BRN2LinkStat::registerHandler(void *element, int protocolId, int32_t (*tx_handler)(void* element, const EtherAddress *src, char *buffer, int32_t size),
                              int32_t (*rx_handler)(void* element, EtherAddress *src, char *buffer, int32_t size, bool is_neighbour, uint8_t fwd_rate, uint8_t rev_rate))
{
  _reg_handler.push_back(HandlerInfo(element, protocolId, tx_handler, rx_handler));
  return 0;
}

int
BRN2LinkStat::deregisterHandler(int32_t /*handle*/, int /*protocolId*/) {
  //TODO
  return 0;
}

/*************************************************************************/
/************************ H A N D L E R **********************************/
/*************************************************************************/

enum {
  H_RESET,
  H_BCAST_STATS,
  H_BAD_VERSION,
  H_TAU,
  H_PERIOD,
  H_PROBES,
  H_SCHEMA
};

static String
BRNLinkStat_read_param(Element *e, void *thunk)
{
  BRN2LinkStat *td = (BRN2LinkStat *)e;
  Timestamp now = Timestamp::now();
  switch ((uintptr_t) thunk) {
    case H_BCAST_STATS: return td->read_bcast_stats(); //xml
    case H_BAD_VERSION: return td->bad_nodes(); //xml
    case H_TAU: { //xml
	StringAccum sa;
	sa << "<tau value='" << String(td->_tau) << "' />\n";
	return sa.take_string();
    }
    case H_PERIOD: { //xml
	StringAccum sa;
	sa << "<period value='" << String(td->_period) << "' />\n";
	return sa.take_string();
    }
    case H_PROBES: { //xml
      StringAccum sa;
      TiXmlDocument doc("demotest.xml");
      TiXmlElement meeting1( "Meeting" );
      meeting1.SetAttribute( "where", "School" );

      sa << "<probes id='" << *(td->_dev->getEtherAddress()) << "'";
      sa << " time='" << now.unparse() << "'>\n";

      MCS rate;
      for(int x = 0; x < td->_ads_rs.size(); x++) {
        rate.set_packed_16(td->_ads_rs[x]._rate);
        uint32_t is_ht = (rate._is_ht)?(uint32_t)1:(uint32_t)0;
        sa << "<probe rate='" << rate.to_string() << "' n='" << is_ht << "' mcsindex='" << (uint32_t)rate._ridx;
        sa << "' ht40='" << (uint32_t)rate._ht40 << "' sgi='" << (uint32_t)rate._sgi << "' size='" << td->_ads_rs[x]._size << "' />\n";
      }
      sa << "</probes>\n";
      return sa.take_string() + "\n";
    }
    case H_SCHEMA: return td->read_schema(); //xml schema
    default:
      return String() + "\n";
  }
}

static int
BRNLinkStat_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  BRN2LinkStat *f = (BRN2LinkStat *)e;
  String s = cp_uncomment(in_s);
  switch((long)vparam) {
    case H_RESET: {    //reset
      f->reset();
      break;
    }
    case H_TAU: {
      unsigned m;
      if (!cp_unsigned(s, &m))
        return errh->error("tau parameter must be unsigned");
      f->_tau = m;
      f->reset();
      break;
    }

    case H_PERIOD: {
      unsigned m;
      if (!cp_unsigned(s, &m))
        return errh->error("period parameter must be unsigned");
      f->_period = m;
      f->reset();
      break;
    }
    case H_PROBES: {
      MCS new_mcs;
      Vector<BrnRateSize> ads_rs;
      Vector<String> args;
      cp_spacevec(s, args);
//    if (args.size() % 2 != 0) {
//      return errh->error("must provide even number of numbers\n");
//    }
      for (int x = 0; x < args.size() - 1;) {
        int rate;
        int size;
        int8_t ht_rate = RATE_HT_NONE;
        uint8_t sgi = 0;
        uint8_t ht = 0;

        ht_rate = RATE_HT_NONE;

        if (args[x] == "HT20") {
          ht_rate = RATE_HT20;
          sgi = 0;
          ht = 0;
        } else if (args[x] == "HT20_SGI") {
          ht_rate = RATE_HT20_SGI;
          sgi = 1;
          ht = 0;
        } else if (args[x] == "HT40") {
          ht_rate = RATE_HT40;
          sgi = 0;
          ht = 1;
        } else if (args[x] == "HT40_SGI") {
          ht_rate = RATE_HT40_SGI;
          sgi = 1;
          ht = 1;
        }

        if ( ht_rate != RATE_HT_NONE ) { //we have a ht rate so rate is next index now
          x++;
        }

        if (!cp_integer(args[x], &rate)) {
          return errh->error("invalid PROBES rate value\n");
        }

        if (!cp_integer(args[x + 1], &size)) {
          return errh->error("invalid PROBES size value\n");
        }

        x+=2;

        if ( ht_rate == RATE_HT_NONE )
          ads_rs.push_back(BrnRateSize(rate, size));
        else {
          ads_rs.push_back(BrnRateSize(MCS(rate, ht, sgi).get_packed_16(), size));
        }
      }
      if (!ads_rs.size()) {
        return errh->error("no PROBES provided\n");
      }
      f->_ads_rs = ads_rs;
      break;
    }
  }
  return 0;
}

int
BRN2LinkStat::update_probes(String probes, ErrorHandler *errh)
{
  return(BRNLinkStat_write_param(probes, this, (void *) H_PROBES, errh));
}


void
BRN2LinkStat::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("bcast_stats", BRNLinkStat_read_param, (void *) H_BCAST_STATS);
  add_read_handler("bad_version", BRNLinkStat_read_param, (void *) H_BAD_VERSION);
  add_read_handler("tau",    BRNLinkStat_read_param, (void *) H_TAU);
  add_read_handler("period", BRNLinkStat_read_param, (void *) H_PERIOD);
  add_read_handler("probes", BRNLinkStat_read_param, (void *) H_PROBES);
  add_read_handler("schema", BRNLinkStat_read_param, (void *) H_SCHEMA);

  add_write_handler("reset",  BRNLinkStat_write_param, (void *) H_RESET);
  add_write_handler("tau",    BRNLinkStat_write_param, (void *) H_TAU);
  add_write_handler("period", BRNLinkStat_write_param, (void *) H_PERIOD);
  add_write_handler("probes", BRNLinkStat_write_param, (void *) H_PROBES);
}

#include <click/bighashmap.cc>
#include <click/vector.cc>
#include <click/dequeue.cc>

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2LinkStat)
ELEMENT_LIBS(-L./ -ltinyxml)
