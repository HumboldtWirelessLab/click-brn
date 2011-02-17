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

  if (!_rtable || !_rtable->cast("AvailableRates"))
    return errh->error("RT element is not a AvailableRates");

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

  send_probe();

  struct timeval period;
  timerclear(&period);

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
  WritablePacket *p = Packet::make(128 /*headroom*/,NULL /* *data*/, size + 2, 32); //alignment
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
    Vector<int> rates = _rtable->lookup(*(_dev->getEtherAddress()));

    if (rates.size() && 1 + ptr + rates.size() < end) { // put my available rates into the packet
      ptr[0] = rates.size(); // available rates count
      ptr++;
      int x = 0;
      while (ptr < end && x < rates.size()) {
        ptr[x] = rates[x];
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
      int res = _reg_handler[h]._handler(_reg_handler[h]._element, NULL, (char*)&(ptr[3]), (space_left-4), true);
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
  ceh->rate = rate;                                     // this packet should be transmitted at the given rate

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

  EtherAddress src_ea = EtherAddress(eh->ether_shost);
  if (p->length() < (sizeof(click_brn) + sizeof(link_probe))) {
    BRN_ERROR("packet is too small");
    p->kill(); 
    return 0;
  }

  if (brn->dst_port != BRN_PORT_LINK_PROBE) { // wrong packet type
    BRN_ERROR("got non-BRNLinkStat packet type");
    p->kill();
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

    p->kill();
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
    p->kill();
    return 0;
  }

  // Default to the rate given in the packet.
  uint16_t rate = ntohs(lp->_rate);
  struct click_wifi_extra *ceh = (struct click_wifi_extra *)WIFI_EXTRA_ANNO(p);

  if (WIFI_EXTRA_MAGIC == ceh->magic)  // check if extra header is present !!!
  {
    if (ceh->rate != rate) {
      BRN_WARN("packet says rate %d is %d; drop it.", rate,  ceh->rate);
      p->kill();
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
    Vector<int> rates;
    ptr++;
    int x = 0;
    while (ptr < end && x < num_rates) {
      int rate = ptr[x];
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
          rev.push_back(l->rev_rate(_start, rates[x]._rate, rates[x]._size));
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
          /*int res =*/ _reg_handler[h]._handler(_reg_handler[h]._element, &src_ea, (char*)ptr, s, false);
      }

      ptr += s;
    }
  }

  p->kill();

  return 0;
}

/* Returns some information about the nodes world model */
String
BRN2LinkStat::read_bcast_stats()
{

  Timestamp now = Timestamp::now();

  Vector<EtherAddress> ether_addrs;

  for (ProbeMap::const_iterator i = _bcast_stats.begin(); i.live(); i++) 
    ether_addrs.push_back(i.key());

  //sort
  click_qsort(ether_addrs.begin(), ether_addrs.size(), sizeof(EtherAddress), etheraddr_sorter);

  StringAccum sa;

  //sa << "<entries>\n";
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
      sa << " rate='" << pl->_probe_types[x]._rate << "'";
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

  StringAccum sa;
  for (BadTable::const_iterator i = _bad_table.begin(); i.live(); i++) {
    uint8_t version = i.value();
    EtherAddress dst = i.key();
    sa << this << " eth " << dst.unparse().c_str() << " version " << (int) version << "\n";
  }

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

int
BRN2LinkStat::registerHandler(void *element, int protocolId, int (*handler)(void *element, EtherAddress *ea, char *buffer, int size, bool direction)) {
  _reg_handler.push_back(HandlerInfo(element, protocolId, handler));
  return 0;
}

int
BRN2LinkStat::deregisterHandler(int /*handler*/, int /*protocolId*/) {
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
  H_PROBES
};

static String
BRNLinkStat_read_param(Element *e, void *thunk)
{
  BRN2LinkStat *td = (BRN2LinkStat *)e;
  switch ((uintptr_t) thunk) {
    case H_BCAST_STATS: return td->read_bcast_stats();
    case H_BAD_VERSION: return td->bad_nodes();
    case H_TAU: return String(td->_tau) + "\n";
    case H_PERIOD: return String(td->_period) + "\n";
    case H_PROBES: {
      StringAccum sa;
      TiXmlDocument doc("demotest.xml");
      TiXmlElement meeting1( "Meeting" );
      meeting1.SetAttribute( "where", "School" );

      for(int x = 0; x < td->_ads_rs.size(); x++) {
        sa << td->_ads_rs[x]._rate << " " << td->_ads_rs[x]._size << " ";
      }
      return sa.take_string() + "\n";
    }
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
      Vector<BrnRateSize> ads_rs;
      Vector<String> a;
      cp_spacevec(s, a);
      if (a.size() % 2 != 0) {
        return errh->error("must provide even number of numbers\n");
      }
      for (int x = 0; x < a.size() - 1; x += 2) {
        int rate;
        int size;
        if (!cp_integer(a[x], &rate)) {
          return errh->error("invalid PROBES rate value\n");
        }
        if (!cp_integer(a[x + 1], &size)) {
          return errh->error("invalid PROBES size value\n");
        }
        ads_rs.push_back(BrnRateSize(rate, size));
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
