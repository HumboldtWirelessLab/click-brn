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

#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "brn2_brnlinkstat.hh"

CLICK_DECLS

// packet data should be 4 byte aligned
#define ASSERT_ALIGNED(p) assert(((unsigned long)(p) % 4) == 0)

BRN2LinkStat::BRN2LinkStat()
  : _dev(),
    _tau(10000),
    _period(1000),
    _seq(0),
    _metrics(NULL),
    _metrics_size(0),
    _next_neighbor_to_add(0),
    _timer(static_send_hook,this),
    _stale_timer(this),
    _ads_rs_index(0),
    _brn_rsp_packet_buf(NULL),
    _brn_rsp_packet_buf_size(0),
    _rtable(0),
    _stale(LINKSTAT_DEFAULT_STALE)
{
  BRNElement::init();
}

BRN2LinkStat::~BRN2LinkStat()
{
  _timer.unschedule();
  _stale_timer.unschedule();

  _bcast_stats.clear();
  _neighbors.clear();
  _ads_rs.clear();
  _bad_table.clear();

  delete[] _metrics;
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
              "METRIC", cpkP, cpString, &_metric_str,
              "STALE", cpkP, cpInteger, &_stale,
              "DEBUG", cpkP, cpInteger, &_debug,
            cpEnd);

  if (res < 0) return res;

  res = update_probes(probes);
  if (res < 0) return res;

  if (!_dev || !_dev->cast("BRN2Device"))
    return errh->error("BRN2Device element is not provided or not a BRN2Device");

  if (!_rtable || !_rtable->cast("BrnAvailableRates"))
    return errh->error("RT element is not a BrnAvailableRates");

  return res;
}

/* Called by the Click framework */
int
BRN2LinkStat::initialize(ErrorHandler *errh)
{
  add_metric(_metric_str, errh);

  if (noutputs() > 0) {
    if (!_dev) return errh->error("Source Ethernet address (NodeIdentity) must be specified to send probes");

    click_brn_srandom();

    _timer.initialize(this);

    _stale_timer.initialize(this);
    _stale_timer.schedule_now();

    int jitter = (click_random() % ((_period / 5) + 1));

    _next = Timestamp::now() + Timestamp::make_msec(jitter);

    BRN_DEBUG("next %s", _next.unparse().c_str());

    _timer.schedule_at(_next);
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
  BRN2LinkStat *q = reinterpret_cast<BRN2LinkStat *>(e->cast("BRNLinkStat"));
  if (!q) {
    errh->error("Couldn't cast old BRNLinkStat");
    return;
  }

  _neighbors = q->_neighbors;
  _bcast_stats = q->_bcast_stats;
  _start = q->_start;

  Timestamp now;
  now = Timestamp::now();

  if ( now < q->_next ) {
    _timer.unschedule();
    _timer.schedule_at(q->_next);
    _next = q->_next;
  }

}

void
BRN2LinkStat::update_link(const EtherAddress &from, EtherAddress &to, Vector<BrnRateSize> &rs, Vector<uint8_t> &fwd, Vector<uint8_t> &rev, uint32_t seq, uint8_t update_mode)
{
  for (int i = 0; i < _metrics_size; i++) {
    _metrics[i]->update_link(from, to, rs, fwd, rev, seq, update_mode);
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

  int p = _period / _ads_rs.size();
  _next += Timestamp::make_msec(p);

  int jitter = (click_random() % ((p / 5) + 1)) - (p / 10);

  Timestamp _next_with_jitter = _next;

  if ( jitter >= 0 ) _next_with_jitter += Timestamp::make_msec(jitter);
  else _next_with_jitter -= Timestamp::make_msec(-1*jitter);

  _timer.schedule_at(_next_with_jitter);

  BRN_DEBUG("Schedule next at %s",_next_with_jitter.unparse().c_str());

  send_probe();

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

  EtherAddress dev_address = *(_dev->getEtherAddress());

  uint16_t size = _ads_rs[_ads_rs_index]._size;   // size of the probe packet (e.g. 1000 byte)
  uint16_t rate = _ads_rs[_ads_rs_index]._rate;   // probe packet's transmission rate
  uint16_t power = _ads_rs[_ads_rs_index]._power;
  uint32_t seq = ++_seq;

  if ( size < (sizeof(click_brn) + sizeof(struct link_probe) + sizeof(struct probe_params))) {
    BRN_ERROR(" cannot send packet size %d: min is %d\n", size, (sizeof(click_brn) + sizeof(struct link_probe) + sizeof(struct probe_params)));
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

  uint8_t *p_data = p->data();
  uint32_t p_length = p->length();
  memset(p_data, 0, p_length);

  // each probe packet is annotated with a timestamp
  struct timeval now = Timestamp::now().timeval();
  p->set_timestamp_anno(now);

  /*
   * fill brn header header // think about this; probe packets only available for one hop
   */
  BRNProtocol::set_brn_header(p_data, BRN_PORT_LINK_PROBE, BRN_PORT_LINK_PROBE, p_length, 1, BRN_TOS_BE );
  BRNPacketAnno::set_tos_anno(p, BRN_TOS_BE);

  /*
   * Fill linkprobe header
   */
  link_probe *lp = (struct link_probe *) (p_data + sizeof(click_brn));
  lp->_version = LINKPROBE_VERSION;
  lp->_flags = 0;

  lp->_cksum = 0;

  lp->_seq = htonl(seq);
  lp->_tau = htonl(_tau);

  lp->_period = htons(_period);
  lp->_num_probes = (uint8_t)_ads_rs.size(); // number of probes monitored by this node (e.g. 2 in case of (2 60, 22 1400))

  lp->_brn_rate_size_index = (uint8_t)_ads_rs_index;

  uint8_t *ptr =  (uint8_t *)(lp + 1); // points to the start of the payload area (array of wifi_link_entry entries)
  uint8_t *end = (uint8_t *)p_data + p_length; // indicates end of payload area; neccessary to preserve max packet size

  BRN_DEBUG("Pack: Start: %x End: %x",ptr,end);
  BRN_DEBUG("Copy ProbeList");

  if ( (ptr + _brn_rsp_packet_buf_size) <= end ) {
    memcpy(ptr, _brn_rsp_packet_buf, _brn_rsp_packet_buf_size);
    ptr += _brn_rsp_packet_buf_size;
    lp->_num_incl_probes = (uint8_t)_ads_rs.size();
  } else {
    click_chatter("Just rate");
    lp->_brn_rate_size_index = 0;
    memcpy(ptr, (uint8_t*)&(((struct probe_params*)_brn_rsp_packet_buf)[_ads_rs_index]), sizeof(struct probe_params));
    ptr += sizeof(struct probe_params);
    lp->_num_incl_probes = 1;
  }

  /*
   * Add Available rates
   */
  if (_rtable) {
    // my wireless device is capable to transmit packets at rates (e.g. 2 4 11 12 18 22 24 36 48 72 96 108)
    Vector<MCS>* rates = _rtable->lookup_p(dev_address);

    if (rates->size() && 1 + ptr + rates->size() < end) { // put my available rates into the packet
      ptr[0] = rates->size(); // available rates count
      ptr++;
      int x = 0;
      while (ptr < end && x < rates->size()) {
        ptr[x] = (*rates)[x].get_packed_8();
        x++;
      }
      ptr += rates->size();
      lp->_flags |= PROBE_AVAILABLE_RATES; // indicate that rates are available
    }
  }

  /*
   * add fwd information for each neighbour and rate/sizeof
   */
  uint8_t num_entries = 0;   // iterate over my neighbor list and append link information to packet

  _rates_v.reserve(_ads_rs.size());
  _fwd_v.reserve(_ads_rs.size());
  _rev_v.reserve(_ads_rs.size());

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

      _rates_v.resize(probe->_probe_types.size());
      _fwd_v.resize(probe->_probe_types.size());
      _rev_v.resize(probe->_probe_types.size());

      for (int x = 0; x < probe->_probe_types.size(); x++) {      // append link info
        BrnRateSize rs = probe->_probe_types[x];
        link_info *lnfo = (struct link_info *) (ptr + x * sizeof(link_info));
        lnfo->_rate_index = _brn_rsp_imap.find(rs);                          // probe packet size & transmission rate
        lnfo->_fwd = probe->_fwd_rates[x];                                   // forward delivery ratio d_f
        lnfo->_rev = probe->rev_rate(_start, rs); // reverse delivery ratio
        lnfo->_min_power = probe->get_min_rx_power(rs._rate, rs._size, rs._power);                        // rev_rate calcs min rx power

        _rates_v[x] = rs;
        _fwd_v[x] = lnfo->_fwd;
        _rev_v[x] = lnfo->_rev;
      }
      // update my own link table
      update_link(dev_address, probe->_ether, _rates_v, _fwd_v, _rev_v, probe->_seq, METRIC_UPDATE_PASSIVE );

      ptr += probe->_probe_types.size() * sizeof(link_info);
    }
  }

  if ( num_entries > 0 ) lp->_flags |= PROBE_REV_FWD_INFO; // indicate that info are available
  lp->_num_links = num_entries; // number of transmitted link_entry elements

  //BRN_DEBUG("Spaceleft: %d links: %d probec: %d",(end-ptr), (uint32_t)num_entries,_brn_rsp_packet_buf_size );

  uint16_t space_left = (end-ptr);
  BRN_DEBUG("Handler pre Ptr: %x End: %x space left: %d",ptr,end,space_left);

  if ( _reg_handler.size() > 0 && (space_left > 4) ) {

    lp->_flags |= PROBE_HANDLER_PAYLOAD; // indicate that handler_info are available

    for ( int h = 0; h < _reg_handler.size(); h++ ) {
      int32_t res = _reg_handler[h]._tx_handler(_reg_handler[h]._element, &dev_address, (char*)&(ptr[3]), (space_left-4));
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

  lp->_cksum = click_in_cksum((unsigned char *)lp, p_length - sizeof(click_brn));

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

  ceh->power = power;
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

  Timestamp now = Timestamp::now();
  const click_ether *eh = reinterpret_cast<const click_ether *>( p->ether_header());
  const click_brn *brn = reinterpret_cast<const click_brn *>( p->data());

  /* best_xxx used to set fwd and rev for rx_handler (other elements) */
  uint8_t best_fwd = 0;
  uint8_t best_rev = 0;

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

  const link_probe *lp = reinterpret_cast<const link_probe *>((p->data() + sizeof(click_brn)));
  if (lp->_version != LINKPROBE_VERSION) {
    static bool version_warning = false;
    _bad_table.insert(src_ea, lp->_version);
    if (!version_warning) {
      version_warning = true;
      BRN_WARN(" unknown lp version %x from %s",
        lp->_version, src_ea.unparse().c_str());
    }

    p->kill();
    return 0;
  }

  if (click_in_cksum((unsigned char *) lp, p->length() - sizeof(click_brn)) != 0) {
    BRN_WARN("failed checksum. Src: %s",src_ea.unparse().c_str());
    checked_output_push(1, p);
    return 0;
  }

  struct probe_params *probe_params_rsp = (struct probe_params *)&lp[1];

  for ( int i = 0; i < lp->_num_incl_probes; i++ ) {
    probe_params_rsp[i]._size = ntohs(probe_params_rsp[i]._size);
    probe_params_rsp[i]._rate = ntohs(probe_params_rsp[i]._rate);
  }

  uint16_t size = probe_params_rsp[lp->_brn_rate_size_index]._size;   // size of the probe packet (e.g. 1000 byte)
  uint16_t rate = probe_params_rsp[lp->_brn_rate_size_index]._rate;   // probe packet's transmission rate
  uint16_t power = probe_params_rsp[lp->_brn_rate_size_index]._power;

  if (p->length() < size) {
    BRN_WARN("packet is smaller (%d) than it claims (%u)", p->length(), size);
  }

  if (src_ea == *(_dev->getEtherAddress())) {
    BRN_WARN("got own packet; drop it. %s", src_ea.unparse().c_str());
    p->kill();
    return 0;
  }

  // Default to the rate given in the packet.
  struct click_wifi_extra *ceh = (struct click_wifi_extra *)WIFI_EXTRA_ANNO(p);

  if (WIFI_EXTRA_MAGIC == ceh->magic)  // check if extra header is present !!!
  {
    MCS mcs = MCS(ceh,0);
    if (mcs.get_packed_16() != rate) {
      BRN_WARN("packet says rate %d is %d; drop it.", rate, mcs.get_packed_16());
      p->kill();
      return 0;
    }
  }
  else
  {
    BRN_FATAL("extra header not set (Forgotten {Extra|RadioTap|AthDesc|Prism2}Decap?).");
  }

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

  BrnRateSize rs = BrnRateSize(rate, size, power);

  if ( l->_probe_types_map.findp(rs) == NULL ) {
    l->_probe_types_map.insert(rs, l->_probe_types.size());
    l->_probe_types.push_back(rs);
    l->_fwd_rates.push_back(0);
    l->_fwd_min_rx_powers.push_back(0);
    l->_rev_no_probes.push_back(0);
    l->_rev_min_rssi.push_back(0);
    l->_rev_min_rssi_index.push_back(-1);
  }

  l->_period = new_period;
  l->_tau = ntohl(lp->_tau);
  l->_last_rx = now;
  l->_seq = ntohl(lp->_seq);
  l->_num_probes = lp->_num_probes;
  l->push_back_probe(now, ntohl(lp->_seq), rs, ceh->rssi);

  // pointer points to start and end of packet payload
  uint8_t *ptr =  (uint8_t *) (lp + 1);
  ptr += sizeof(struct probe_params) * (uint32_t)lp->_num_incl_probes;

  uint8_t *end  = (uint8_t *) p->data() + p->length();

  //BRN_DEBUG("Start: %x End: %x",ptr,end);

  if (lp->_flags & PROBE_AVAILABLE_RATES) { // available rates where transmitted
    int num_rates = ptr[0];
    ptr++;

    if(_rtable) { // store neighbor node's available rates
      if ( ! _rtable->includes_node(src_ea) ) {
        Vector<MCS> rates;
        rates.resize(num_rates);

        int x = 0;
        while (ptr < end && x < num_rates) {
          MCS rate;
          rate.set_packed_8(ptr[x]);
          rates[x] = rate;
          x++;
        }
        _rtable->insert(src_ea, rates);
      }
    }

    ptr += num_rates;
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

      _rates_v.resize(num_rates);
      _fwd_v.resize(num_rates);
      _rev_v.resize(num_rates);

      for (int x = 0; x < num_rates; x++) {
        struct link_info *nfo = (struct link_info *) (ptr + x * (sizeof(struct link_info)));
        struct probe_params *nfo_pp = (struct probe_params *)&probe_params_rsp[nfo->_rate_index];

        BRN_DEBUG(" %s neighbor %s: size %d rate %d fwd %d rev %d",
          src_ea.unparse().c_str(), neighbor.unparse().c_str(), nfo_pp->_size, nfo_pp->_rate, nfo->_fwd, nfo->_rev);

        BrnRateSize rs = BrnRateSize(nfo_pp->_rate, nfo_pp->_size, nfo_pp->_power);
        // update other link stuff
        _rates_v[x] = rs;

        _fwd_v[x] = nfo->_fwd; // forward delivery ratio

        if (neighbor == *(_dev->getEtherAddress())) { // reverse delivery ratio is available -> use it.
          if ( nfo->_fwd > best_fwd ) best_fwd = nfo->_fwd; //just used to determinate whether node is a neighbour or not
          uint8_t rev_rate = l->rev_rate(_start, rs);
          if ( rev_rate > best_rev ) best_rev = rev_rate;  //just used to determinate whether node is a neighbour or not
          _rev_v[x] = rev_rate;
        } else {
          _rev_v[x] = nfo->_rev;
        }

        if (neighbor == *(_dev->getEtherAddress())) {
          // set the fwd rate
          int *rs_index = l->_probe_types_map.findp(rs);
          if ( rs_index != NULL ) {
            l->_fwd_rates[*rs_index] = nfo->_rev;
            l->_fwd_min_rx_powers[*rs_index] = nfo->_min_power;
          }
        }
      }
      int seq = ntohl(entry->_seq);

#ifdef LINKSTAT_EXTRA_DEBUG
      //TODO "Enable Extra Linkstat debug"
      uint8_t zero_mac[] = { 0,0,0,0,0,0 }; 
      if ( (memcmp(src_ea.data(), zero_mac, 6) == 0) || (memcmp(neighbor.data(), zero_mac, 6) == 0) ) {
        BRN_WARN("Found zero mac");
        checked_output_push(1, p);
        return 0;
      }
#endif

      update_link(src_ea, neighbor, _rates_v, _fwd_v, _rev_v, seq, METRIC_UPDATE_ACTIVE);
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
        if ( proto == _reg_handler[h]._protocol ){
           BRN_DEBUG("quality fwd:%d,quality rev:%d",best_fwd,best_rev);
          /*int res =*/ _reg_handler[h]._rx_handler(_reg_handler[h]._element, &src_ea, (char*)ptr, s,
           /*is_neighbour*/(best_fwd > 80) && (best_rev > 80), /*fwd_rate*/best_fwd , /*rev_rate*/best_rev);
      }}

      ptr += s;
    }
  }

  p->kill();

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
BRN2LinkStat::read_bcast_stats()
{
  Timestamp now = Timestamp::now();
  MCS rate;

  Vector<EtherAddress> ether_addrs;

  for (ProbeMap::const_iterator i = _bcast_stats.begin(); i.live();++i)
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
      sa << " power='" << (uint32_t)(pl->_probe_types[x]._power) << "'";

      int rev_rate = pl->rev_rate(_start, pl->_probe_types[x]._rate, pl->_probe_types[x]._size, pl->_probe_types[x]._power);
      sa << " fwd='" << (uint32_t)pl->_fwd_rates[x] << "'";
      sa << " rev='" << (uint32_t)rev_rate << "'";
      sa << " fwd_min_rssi='" << (uint32_t)pl->_fwd_min_rx_powers[x] << "'";
      sa << " rev_min_rssi='" << (uint32_t)pl->get_min_rx_power(pl->_probe_types[x]._rate, pl->_probe_types[x]._size, pl->_probe_types[x]._power) << "'";
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

  for (BadTable::const_iterator i = _bad_table.begin(); i.live();++i) {
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

  new_neighbors.reserve(_neighbors.size() + 1);
  Timestamp now = Timestamp::now();

  for (int x = 0; x < _neighbors.size(); x++) {
    EtherAddress n = _neighbors[x];
    probe_list_t *l = _bcast_stats.findp(n);

    if (l == NULL) {
      BRN_DEBUG(" clearing stale neighbor %s age n/a(brn_0)", n.unparse().c_str());
      _bcast_stats.remove(n);
    } else if ((now - l->_last_rx).msecval() > (2 * l->_tau)) {
      BRN_DEBUG(" clearing stale neighbor %s age %d(brn_0)",
        n.unparse().c_str(), (int)((now - l->_last_rx).msecval() / 1000 ));
      _bcast_stats.remove(n);
    } else {
      new_neighbors.push_back(n);
    }
  }

  _neighbors.reserve(new_neighbors.size() + 1);
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
BRN2LinkStat::get_rev_rate(const EtherAddress *ea)
{
  probe_list_t *probe = _bcast_stats.findp(*ea);

  if ( ! probe )  return 0;

  BrnRateSize rs = probe->_probe_types[0];

  return ( probe->rev_rate(_start, rs._rate, rs._size, rs._power) ); // reverse delivery ratio
}

int
BRN2LinkStat::get_fwd_rate(const EtherAddress *ea)
{
  probe_list_t *probe = _bcast_stats.findp(*ea);

  if ( ! probe )  return 0;
  return probe->_fwd_rates[0];
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

int
BRN2LinkStat::update_probes(String probes)
{
  MCS new_mcs;
  Vector<String> args;
  cp_spacevec(probes, args);

  _ads_rs.clear();
  _brn_rsp_imap.clear();

  int no_probes = 0;

  for (int x = 0; x < args.size() - 1;) {
    int rate;
    int size;
    int power;
    int8_t ht_rate = RATE_HT_NONE;
    uint8_t sgi = 0;
    uint8_t ht = 0;

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
      BRN_ERROR("invalid PROBES rate value\n");
      return -1;
    }

    if (!cp_integer(args[x + 1], &size)) {
      BRN_ERROR("invalid PROBES size value\n");
      return -1;
    }

    if (!cp_integer(args[x + 2], &power)) {
      BRN_ERROR("invalid PROBES power value\n");
      return -1;
    }

    x+=3;

    if ( ht_rate == RATE_HT_NONE ) {
      _ads_rs.push_back(BrnRateSize(rate, size, power));
    } else {
      _ads_rs.push_back(BrnRateSize(MCS(rate, ht, sgi).get_packed_16(), size, power));
    }

    _brn_rsp_imap.insert(_ads_rs[no_probes], no_probes);
    no_probes++;
  }

  if (!_ads_rs.size()) {
    BRN_ERROR("no PROBES provided\n");
    return -1;
  }

  if ( _brn_rsp_packet_buf != NULL ) delete _brn_rsp_packet_buf;

  _brn_rsp_packet_buf_size = _ads_rs.size() * sizeof(struct probe_params);
  _brn_rsp_packet_buf = new uint8_t[_brn_rsp_packet_buf_size];

  struct probe_params* pp = (struct probe_params*)_brn_rsp_packet_buf;
  for ( int i = 0; i < _ads_rs.size(); i++) {
    pp[i]._size = htons(_ads_rs[i]._size);
    pp[i]._rate = htons(_ads_rs[i]._rate);
    pp[i]._power = (uint8_t)_ads_rs[i]._power;
  }

  return 0;
}

int
BRN2LinkStat::add_metric(String metric_str, ErrorHandler *errh)
{
  String s = cp_uncomment(metric_str);

  Vector<String> metric_vec;
  cp_spacevec(metric_str, metric_vec);

  BRN2GenericMetric **new_metrics = new BRN2GenericMetric*[_metrics_size + metric_vec.size()];

  //memcpy(new_metrics, _metrics, _metrics_size * sizeof(BRN2GenericMetric*));
  for ( int i = 0; i  < _metrics_size; i++) new_metrics[i] = _metrics[i];

  if (_metrics) delete[] _metrics;

  _metrics = new_metrics;

  for (int i = 0; i < metric_vec.size(); i++) {
    Element *new_element = cp_element(metric_vec[i] , this, errh, NULL);
    if ( new_element != NULL ) {
      //click_chatter("El-Name: %s", new_element->class_name());
      BRN2GenericMetric *gm = reinterpret_cast<BRN2GenericMetric *>(new_element->cast("BRN2GenericMetric"));
      if ( gm != NULL ) {
        _metrics[_metrics_size] = gm;
        _metrics_size++;
      }
    } else {
      click_chatter("Error: Can't find Element with name: %s", new_element->class_name());
    }
  }

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
  H_METRIC
};

static String
BRNLinkStat_read_param(Element *e, void *thunk)
{
  BRN2LinkStat *td = reinterpret_cast<BRN2LinkStat *>(e);
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

      sa << "<probes id='" << *(td->_dev->getEtherAddress()) << "'";
      sa << " time='" << now.unparse() << "'>\n";

      MCS rate;
      for(int x = 0; x < td->_ads_rs.size(); x++) {
        rate.set_packed_16(td->_ads_rs[x]._rate);
        uint32_t is_ht = (rate._is_ht)?(uint32_t)1:(uint32_t)0;
        sa << "\t<probe rate='" << rate.to_string() << "' n='" << is_ht << "' mcsindex='" << (uint32_t)rate._ridx;
        sa << "' ht40='" << (uint32_t)rate._ht40 << "' sgi='" << (uint32_t)rate._sgi;
        sa << "' size='" << td->_ads_rs[x]._size << "' power='" <<  (uint32_t)td->_ads_rs[x]._power << "' />\n";
      }
      sa << "</probes>\n";
      return sa.take_string() + "\n";
    }
    default:
      return String() + "\n";
  }
}


static int
BRNLinkStat_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  BRN2LinkStat *f = reinterpret_cast<BRN2LinkStat *>(e);
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
      f->update_probes(s);
      break;
    }
    case H_METRIC: {
      f->add_metric(in_s, errh);
      break;
    }
  }
  return 0;
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

  add_write_handler("add_metric", BRNLinkStat_write_param, (void *) H_METRIC);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2LinkStat)
