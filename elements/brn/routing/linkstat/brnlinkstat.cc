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

#include "brnlinkstat.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
CLICK_DECLS

// packet data should be 4 byte aligned
#define ASSERT_ALIGNED(p) assert(((unsigned long)(p) % 4) == 0)

enum {
  H_RESET,
  H_BCAST_STATS,
  H_BAD_VERSION,
  H_TAU,
  H_PERIOD,
  H_PROBES,
  H_DEBUG
};

//BRNNEW start
 int EtherAddressHashcode(const EtherAddress &ea)
{
  const uint16_t *d = ea.sdata();
  return (d[2] | (d[1] << 16)) ^ (d[0] << 9);
}
//BRNNEW end

static String 
BRNLinkStat_read_param(Element *e, void *thunk)
{
  BRNLinkStat *td = (BRNLinkStat *)e;
    switch ((uintptr_t) thunk) {
    case H_BCAST_STATS: return td->read_bcast_stats(false);
    case H_BAD_VERSION: return td->bad_nodes();
    case H_TAU: return String(td->_tau) + "\n";
    case H_PERIOD: return String(td->_period) + "\n";
    case H_PROBES: {
      StringAccum sa;
      for(int x = 0; x < td->_ads_rs.size(); x++) {
	sa << td->_ads_rs[x]._rate << " " << td->_ads_rs[x]._size << " ";
      }
      return sa.take_string() + "\n";
    }
    case H_DEBUG: return String(td->_debug) + "\n";
    default:
      return String() + "\n";
    }
}
static int 
BRNLinkStat_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  BRNLinkStat *f = (BRNLinkStat *)e;
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
  case H_DEBUG: {
    int debug;
    if (!cp_integer(s, &debug)) 
      return errh->error("debug parameter must be an integer value between 0 and 4");
    f->_debug = debug;
    break;
  }
  }
  return 0;
}

void
BRNLinkStat::add_handlers()
{
  add_read_handler("bcast_stats", BRNLinkStat_read_param, (void *) H_BCAST_STATS);
  add_read_handler("bad_version", BRNLinkStat_read_param, (void *) H_BAD_VERSION);
  add_read_handler("tau", BRNLinkStat_read_param, (void *) H_TAU);
  add_read_handler("period", BRNLinkStat_read_param, (void *) H_PERIOD);
  add_read_handler("probes", BRNLinkStat_read_param, (void *) H_PROBES);
  add_read_handler("debug", BRNLinkStat_read_param, (void *) H_DEBUG);

  add_write_handler("reset", BRNLinkStat_write_param, (void *) H_RESET);
  add_write_handler("tau", BRNLinkStat_write_param, (void *) H_TAU);
  add_write_handler("period", BRNLinkStat_write_param, (void *) H_PERIOD);
  add_write_handler("probes", BRNLinkStat_write_param, (void *) H_PROBES);
  add_write_handler("debug", BRNLinkStat_write_param, (void *) H_DEBUG);
}

// Log some stuff here
static void
log_timeout_hook(Timer *, void *thunk)
{
  BRNLinkStat *e = (BRNLinkStat*)thunk;
  e->run_log_timer();
}

BRNLinkStat::BRNLinkStat()
  : _tau(10000), 
    _period(1000),
    _seq(0), 
    _sent(0),
    _me(),
    _ett_metric(0),
    _etx_metric(0),
//BRNNEW    _dht(NULL),
//BRNNEW    _cansel(NULL),
    _debug(BrnLogger::DEFAULT),
    _next_neighbor_to_ad(0),
    _timer(0),
    _stale_timer(this),
    _ads_rs_index(0),
    _rtable(0),
//BRNNEW    _packetCnt(0),
    _log(false),
    _log_timeout_timer(log_timeout_hook, this)
//    _log_fp(0),
//    _log_id(0)
{
  //add_input();
}

BRNLinkStat::~BRNLinkStat()
{
  if (_timer)
    delete _timer;
  _timer = NULL;
/*
  if (_log_fp && _log_fp != stdout)
      fclose(_log_fp);
  _log_fp = 0;
*/
}

int
BRNLinkStat::configure(Vector<String> &conf, ErrorHandler* errh)
{
  String probes;
  int res = cp_va_parse(conf, this, errh,
			cpKeywords,
			"ETHTYPE", cpUnsigned, "Ethernet encapsulation type", &_et,
      "NODEIDENTITY", cpElement, "NodeIdentity", &_me,
			"PERIOD", cpUnsigned, "Probe broadcast period (msecs)", &_period,
			"TAU", cpUnsigned, "Loss-rate averaging period (msecs)", &_tau,
			"ETT", cpElement, "ETT Metric element", &_ett_metric,
			"ETX", cpElement, "ETX Metric element", &_etx_metric,
			"PROBES", cpString, "PROBES", &probes,
			"RT", cpElement, "AvailabeRates", &_rtable,
			"LOGGING", cpBool, "Logging", &_log,
//BRNNEW			"BRNAVGCNT", cpElement, "BrnAvgCnt", &_packetCnt,
//BRNNEW			"DHT", cpElement, "DHT", &_dht,
//BRNNEW			"CANSELECTOR", cpElement, "CANSEL" , &_cansel,
  //                      "LOG_INTERVALL", cpInteger, "Logging Interval (in msecs)", &_log_interval,
  //                      "LOG_FILENAME", cpFilename, "log filename", &_log_filename,
			cpEnd);


  if (res < 0)
    return res;

  res = BRNLinkStat_write_param(probes, this, (void *) H_PROBES, errh);
  if (res < 0)
    return res;

  if (!_et)
    return errh->error("Must specify ETHTYPE");
    
  if (!_me || !_me->cast("NodeIdentity")) 
    return errh->error("NodeIdentity element is not provided or not a NodeIdentity");

  if (_ett_metric && !_ett_metric->cast("BRNETTMetric"))
    return errh->error("BRNETTMetric element is not a BRNETTMetric");

  if (_etx_metric && !_etx_metric->cast("BRNETXMetric"))
    return errh->error("BRNETXMetric element is not a BRNETXMetric");

  if (!_rtable || !_rtable->cast("AvailableRates"))
    return errh->error("RT element is not a AvailableRates");

//BRNNEW
/*  if (_dht) {
    if (!_dht->cast("FalconDHT"))
      return errh->error("DHT element is not a FalconDHT");
  }

  if (_cansel) {
    if (!_cansel->cast("BRNCandidateSelector"))
      return errh->error("Cansel element is not a Cansel");
  }
*/
/*
  if (_packetCnt && _packetCnt->cast("BrnAvgCnt") == 0) {
    return errh->error("RT element is not a BrnAvgCnt");
  }
*/
  if (_log) {
    BRN_INFO(" * Logging activated.");
  }
/*
  if (_log) {
    click_chatter(" * Logging activated.\n");
    if (!_log_interval)
      return errh->error("Logging Interval not specified\n");
    if (!_log_filename)
      return errh->error("Logging Filename not specified\n");
  }
*/
  return res;
}

void
BRNLinkStat::run_timer(Timer*)
{
  clear_stale();
  _stale_timer.schedule_after_msec(6000);
}

void
BRNLinkStat::take_state(Element *e, ErrorHandler *errh)
{
  /* 
   * take_state gets called after 
   * --configure
   * --initialize
   * so we may need to unschedule probe timers
   * and sync them up so the rates don't get 
   * screwed up.
  */
  BRNLinkStat *q = (BRNLinkStat *)e->cast("BRNLinkStat");
  if (!q) {
    errh->error("Couldn't cast old BRNLinkStat");
    return;
  }

  _neighbors = q->_neighbors;
  _bcast_stats = q->_bcast_stats;
  _sent = q->_sent;
  _start = q->_start;

  struct timeval now;
  now = Timestamp::now().timeval();

  if (timercmp(&now, &q->_next, <)) {
    _timer->unschedule();
    _timer->schedule_at(q->_next);
    _next = q->_next;
  }

  //_log_fp = q->_log_fp;
  //q->_log_fp = 0;
}

void
add_jitter2(unsigned int max_jitter, struct timeval *t) {
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
  BRNLinkStat::update_link(EtherAddress from, EtherAddress to, Vector<BrnRateSize> rs, Vector<int> fwd, Vector<int> rev, uint32_t seq)
{

/*
  if (_log) {
    click_chatter("update link called:\n");
    // print broadcast statistics
    click_chatter("%s\n", read_bcast_stats(false).c_str());
  }
*/
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
BRNLinkStat::send_probe_hook()
{
  BRN_DEBUG("send_probe_hook()");

  int p = _period / _ads_rs.size(); //period (msecs); _ads_rs.size(): probe count
  unsigned max_jitter = p / 10;

  send_probe();

  struct timeval period;
  timerclear(&period);

  period.tv_usec += (p * 1000);
  period.tv_sec += period.tv_usec / 1000000;
  period.tv_usec = (period.tv_usec % 1000000);

  timeradd(&period, &_next, &_next);
  add_jitter2(max_jitter, &_next);
  _timer->schedule_at(_next);
}

/*
 * This method is called periodically (_period / _ads_rs.size() +/- jitter msecs)
 * to inform neighbor nodes about his links.
 */
void
BRNLinkStat::send_probe()
{
  if (!_ads_rs.size()) {
    BRN_WARN(" no probes to send at.");
    return;
  }

  // size of the probe packet (e.g. 1000 byte)
  int size = _ads_rs[_ads_rs_index]._size;
  // probe packet's transmission rate
  int rate = _ads_rs[_ads_rs_index]._rate;

  // points to the next probe packet type
  _ads_rs_index = (_ads_rs_index + 1) % _ads_rs.size();
  // how many probes this node has sent
  _sent++;
  unsigned min_packet_sz = sizeof(click_brn) + sizeof(struct link_probe);

  if ((unsigned) size < min_packet_sz) {
    BRN_ERROR(" cannot send packet size %d: min is %d\n", size, min_packet_sz);
    return;
  }

  // construct probe packet
  WritablePacket *p = Packet::make(size + 2); //alignment
  if (p == 0) {
    BRN_ERROR(" cannot make packet!");
    return;
  }

  ASSERT_ALIGNED(p->data());
  p->pull(2);
  memset(p->data(), 0, p->length());

  // each probe packet is annotated with a timestamp
  struct timeval now;
  now = Timestamp::now().timeval();
  p->set_timestamp_anno(now);

  // fill brn header header
  click_brn *brn = (click_brn *) p->data();
  brn->dst_port = BRN_PORT_LINK_PROBE;
  brn->src_port = BRN_PORT_LINK_PROBE;
  brn->ttl = 1; // think about this; probe packets only available for one hop
  brn->tos = BRN_TOS_BE;
/*
  memset(eh->ether_dhost, 0xff, 6); // broadcast
  eh->ether_type = htons(_et);
  memcpy(eh->ether_shost, _eth.data(), 6);
*/
  link_probe *lp = (struct link_probe *) (p->data() + sizeof(click_brn));
  lp->_version = _ett2_version;
  memcpy(lp->_ether, _me->getMyWirelessAddress()->data(), 6); // only wireless links will be measured
  lp->_seq = now.tv_sec;
  lp->_period = _period;
  lp->_tau = _tau;
  lp->_sent = _sent;
  lp->_flags = 0;
  lp->_rate = rate;
  lp->_size = size;
  lp->_num_probes = _ads_rs.size(); // number of probes monitored by this node (e.g. 2 in case of (2 60, 22 1400))

  uint8_t *ptr =  (uint8_t *) (lp + 1); // points to the start of the payload area (array of wifi_link_entry entries)
  uint8_t *end  = (uint8_t *) p->data() + p->length(); // indicates the end of the payload area; neccessary to preserve max packet size


  Vector<int> rates;
  if (_rtable) {
    // my wireless device is capable to transmit packets at rates (e.g. 2 4 11 12 18 22 24 36 48 72 96 108)
    rates = _rtable->lookup(*(_me->getMyWirelessAddress()));
  }
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

  // Piggybag dht stuff
  int dht_payload_size = 0;
//BRNNEW
  /*
  if (_dht) {
    uint8_t *dht_values;
    dht_payload_size = _dht->getNextDhtEntries(&dht_values);
    // additional dht stuff
    if (ptr + dht_payload_size <= end) {

      char buffer[250];
      for ( int index = 0;  index < dht_payload_size; index ++ )
        sprintf(&buffer[ index * 2 ],"%02x",dht_values[index]);

//      BRN_DEBUG("BRNLinkStat: got and send Linkprop-Info: %s", buffer);
      memcpy(ptr, dht_values, dht_payload_size);
    } else {
      dht_payload_size = 0;
    }
  }
*/
  ptr += dht_payload_size;

  // dht stuff size
  lp->_dht_payload_size = dht_payload_size;

  // Piggybag dht stuff
  int cs_payload_size = 0;

//BRNNEW
/*
  if (_cansel)
  {
    cs_payload_size = _cansel->writeCsPayloadToLinkProbe(ptr, (unsigned int)(end - ptr));
    // additional dht stuff
    if (ptr + cs_payload_size <= end) {

     
      char buffer[250];

//    BRN_DEBUG("BRNLinkStat: got and send Linkprop-Info: %s", buffer);
//      memcpy(ptr, cs_values, cs_payload_size);
    } else {
      cs_payload_size = 0;
    }
  }
*/
  ptr += cs_payload_size;

  // dht stuff size
  lp->_cs_payload_size = cs_payload_size;

  int num_entries = 0;
  // iterate over my neighbor list and append link information to packet
  while (ptr < end && num_entries < _neighbors.size()) {
    _next_neighbor_to_ad = (_next_neighbor_to_ad + 1) % _neighbors.size();
    if (_next_neighbor_to_ad >= _neighbors.size()) {
      break;
    }
    probe_list_t *probe = _bcast_stats.findp(_neighbors[_next_neighbor_to_ad]);
    if (!probe) {
      BRN_DEBUG(" lookup for %s, %d failed in ad",
        _neighbors[_next_neighbor_to_ad].unparse().c_str(), _next_neighbor_to_ad);
    } else {
      // size = (probe type count) x ...
      int size = probe->_probe_types.size() * sizeof(link_info) + sizeof(link_entry);
      if (ptr + size > end) {
	     break;
      }
      num_entries++;
      link_entry *entry = (struct link_entry *)(ptr); // append link_entry
      memcpy(entry->_ether, probe->_ether.data(), 6);
      entry->_seq = probe->_seq;

      if ((uint32_t) EtherAddressHashcode(probe->_ether) > (uint32_t) EtherAddressHashcode(*(_me->getMyWirelessAddress()))) {
        entry->_seq = lp->_seq;
      }

      entry->_num_rates = probe->_probe_types.size(); // probe type count
      ptr += sizeof(link_entry);

      Vector<BrnRateSize> rates;
      Vector<int> fwd;
      Vector<int> rev;

      for (int x = 0; x < probe->_probe_types.size(); x++) { // append link info
        BrnRateSize rs = probe->_probe_types[x];
        link_info *lnfo = (struct link_info *) (ptr + x * sizeof(link_info));
        lnfo->_size = rs._size; // probe packet size
        lnfo->_rate = rs._rate; // probe packet transmission rate
        lnfo->_fwd = probe->_fwd_rates[x]; // forward delivery ratio d_f
        lnfo->_rev = probe->rev_rate(_start, rs._rate, rs._size); // reverse delivery ratio

        rates.push_back(rs);
        fwd.push_back(lnfo->_fwd);
        rev.push_back(lnfo->_rev);
      }
      // update my own link table
      update_link(*(_me->getMyWirelessAddress()), EtherAddress(entry->_ether),
        rates, fwd, rev, entry->_seq);

      ptr += probe->_probe_types.size() * sizeof(link_info);
    }
  }

  lp->_num_links = num_entries; // number of transmitted link_entry elements
  lp->_psz = sizeof(link_probe) + lp->_num_links * sizeof(link_entry);
  lp->_cksum = 0;
  lp->_cksum = click_in_cksum((unsigned char *) lp, lp->_psz);

  struct click_wifi_extra *ceh = (struct click_wifi_extra *) p->all_user_anno();
  ceh->magic = WIFI_EXTRA_MAGIC;
  ceh->rate = rate; // this packet should be transmitted at the given rate
  checked_output_push(0, p);
}

/* Called by the Click framework */
int
BRNLinkStat::initialize(ErrorHandler *errh)
{
  if (noutputs() > 0) {
    if (!_me)
      return errh->error("Source Ethernet address (NodeIdentity) must be specified to send probes");

    _timer = new Timer(static_send_hook, this);
    _timer->initialize(this);

    _stale_timer.initialize(this);
    _stale_timer.schedule_now();
    struct timeval now;
    now = Timestamp::now().timeval();

    now.tv_sec = now.tv_sec + 1;

    _next = now;

    unsigned max_jitter = _period / 10;
    add_jitter2(max_jitter, &_next);

    Timestamp nn = Timestamp(_next);
    BRN_DEBUG("_next %s", nn.unparse().c_str());

    _timer->schedule_at(_next);
  }

  reset();

  // Logging enabled
/*
  if (_log) {
    assert(!_log_fp);
    if (_log_filename != "-") {
        _log_fp = fopen(_log_filename.c_str(), "wb");
        if (!_log_fp)
            return errh->error("%s: %s", _log_filename.c_str(), strerror(errno));
    } else {
        _log_fp = stdout;
        _log_filename = "<stdout>";
    }


    click_chatter(" * Logging activated.\n");
    _log_timeout_timer.initialize(this);
    _log_timeout_timer.schedule_after_ms(_log_interval);
  }
*/
  return 0;
}

/*
 * Processes an incoming probe packet.
 */
Packet *
BRNLinkStat::simple_action(Packet *p)
{
  BRN_DEBUG(" * simple_action()");

  struct timeval now;
  now = Timestamp::now().timeval();
  click_ether *eh = (click_ether *) p->ether_header();
  click_brn *brn = (click_brn *) p->data();

  unsigned min_sz = sizeof(click_brn) + sizeof(link_probe);
  if (p->length() < min_sz) {
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
      BRN_WARN(" unknown sr version %x from %s", 
        lp->_version, EtherAddress(eh->ether_shost).unparse().c_str());
    }

    p->kill();
    return 0;
  }

  if (click_in_cksum((unsigned char *) lp, lp->_psz) != 0) {
    BRN_WARN("failed checksum");
    p->kill();
    return 0;
  }

  if (p->length() < lp->_psz + sizeof(click_brn)) {
    BRN_WARN("packet is smaller (%d) than it claims (%u)", p->length(), lp->_psz);
  }

  // sender address (neighbor node)
  EtherAddress ether = EtherAddress(lp->_ether);

  if (ether == *(_me->getMyWirelessAddress())) {
    BRN_WARN("got own packet; drop it. %s", ether.unparse().c_str());
    p->kill();
    return 0;
  }

  // Default to the rate given in the packet.
  uint16_t rate = lp->_rate;

#ifndef CLICK_NS
  struct click_wifi_extra *ceh = (struct click_wifi_extra *) p->all_user_anno();

  // check if extra header is present !!!
  if (WIFI_EXTRA_MAGIC == ceh->magic) 
  {
    if (ceh->rate != lp->_rate) {
      BRN_WARN("packet says rate %d is %d; drop it.", lp->_rate,  ceh->rate);
      p->kill();
      return 0;
    }
  }
  else
  {
    BRN_FATAL("extra header not set (Forgotten {Extra|RadioTap|AthDesc|Prism2}Decap?).");
  }
#endif

  probe_t probe(now, lp->_seq, lp->_rate, lp->_size);
  int new_period = lp->_period;
  probe_list_t *l = _bcast_stats.findp(ether); // fetch sender's probe list
  int x = 0;
  if (!l) { // new neighbor
    _bcast_stats.insert(ether, probe_list_t(ether, new_period, lp->_tau));
    l = _bcast_stats.findp(ether);
    l->_sent = 0;
    // add into the neighbors vector
    _neighbors.push_back(ether);
  } else if (l->_period != new_period) {
    BRN_INFO("%s has changed its link probe period from %u to %u; clearing probe info",
      ether.unparse().c_str(), l->_period, new_period);
    l->_probes.clear();
  } else if (l->_tau != lp->_tau) {
    BRN_INFO("%s has changed its link tau from %u to %u; clearing probe info",
      ether.unparse().c_str(), l->_tau, lp->_tau);
    l->_probes.clear();
  }

  if (lp->_sent < (unsigned)l->_sent) {
    BRN_INFO("%s has reset; clearing probe info", ether.unparse().c_str());
    l->_probes.clear();
  }

  BrnRateSize rs = BrnRateSize(rate, lp->_size);
  l->_period = new_period;
  l->_tau = lp->_tau;
  l->_sent = lp->_sent;
  l->_last_rx = now;
  l->_num_probes = lp->_num_probes;
  l->_probes.push_back(probe);
  l->_seq = probe._seq;

  // keep stats for at least the averaging period; kick old probes
  while ((unsigned) l->_probes.size() &&
        now.tv_sec - l->_probes[0]._when.tv_sec > (signed) (1 + (l->_tau / 1000)))
    l->_probes.pop_front();

  for (x = 0; x < l->_probe_types.size(); x++) {
    if (rs == l->_probe_types[x]) {
      break;
    }
  }

  if (x == l->_probe_types.size()) { // entry not found
    l->_probe_types.push_back(rs);
    l->_fwd_rates.push_back(0);
  }

  // pointer points to start and end of packet payload
  uint8_t *ptr =  (uint8_t *) (lp + 1);
  uint8_t *end  = (uint8_t *) p->data() + p->length();


  if (lp->_flags &= PROBE_AVAILABLE_RATES) { // available rates where transmitted
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

  // dht stuff size

  int dht_payload_size = lp->_dht_payload_size;

//BRNNEW
  /*if (_dht && (dht_payload_size > 0) ) {
    char buffer[250];

    for ( int index = 0;  index < dht_payload_size; index ++ )
      sprintf(&buffer[ index * 2 ],"%02x",ptr[index]);

    BRN_DEBUG("receive Linkprop-Info: %s", buffer);

    _dht->processDhtPayloadFromLinkProbe(ptr, dht_payload_size);
    ptr += dht_payload_size * sizeof(uint8_t);
  }
*/
  // cs stuff size

  int cs_payload_size = lp->_cs_payload_size;

//BRNNEW  if (_cansel && (cs_payload_size > 0) ) {
//BRNNEW The next was already comment out
 /*
    char buffer[250];

    for ( int index = 0;  index < cs_payload_size; index ++ )
      sprintf(&buffer[ index * 2 ],"%02x",ptr[index]);

    BRN_DEBUG("receive Linkprop-Info: %s", buffer);
 */
//BRNNEW
  /*    EtherAddress tmp_add(eh->ether_shost);
    _cansel->readCsPayloadFromLinkProbe(tmp_add, ptr, cs_payload_size);
    ptr += cs_payload_size * sizeof(uint8_t);
  }
*/
  int link_number = 0;
  // fetch link entries
  while (ptr < end 
    && (unsigned) link_number < lp->_num_links) {
    link_number++;
    link_entry *entry = (struct link_entry *)(ptr); 
    EtherAddress neighbor = EtherAddress(entry->_ether);
    int num_rates = entry->_num_rates;

    BRN_DEBUG("on link number %d / %d: neighbor %s, num_rates %d",
      link_number, lp->_num_links, neighbor.unparse().c_str(), num_rates);

    ptr += sizeof(struct link_entry);
    Vector<BrnRateSize> rates;
    Vector<int> fwd;
    Vector<int> rev;
    for (int x = 0; x < num_rates; x++) {
      struct link_info *nfo = (struct link_info *) (ptr + x * (sizeof(struct link_info)));

      BRN_DEBUG(" %s neighbor %s: size %d rate %d fwd %d rev %d",
        ether.unparse().c_str(), neighbor.unparse().c_str(), nfo->_size, nfo->_rate, nfo->_fwd, nfo->_rev);

      BrnRateSize rs = BrnRateSize(nfo->_rate, nfo->_size);
      // update other link stuff
      rates.push_back(rs);
      fwd.push_back(nfo->_fwd); // forward delivery ratio
      if (neighbor == *(_me->getMyWirelessAddress())) { // reverse delivery ratio is available -> use it.
        rev.push_back(l->rev_rate(_start, rates[x]._rate, rates[x]._size));
      } else {
        rev.push_back(nfo->_rev);
      }

      if (neighbor == *(_me->getMyWirelessAddress())) {
        // set the fwd rate
        for (int x = 0; x < l->_probe_types.size(); x++) {
          if (rs == l->_probe_types[x]) {
            l->_fwd_rates[x] = nfo->_rev;
            break;
          }
        }
      }
    }
    int seq = entry->_seq;
    if (neighbor == ether && 
       ((uint32_t) EtherAddressHashcode(neighbor) > (uint32_t) EtherAddressHashcode(*(_me->getMyWirelessAddress())))) {
        seq = now.tv_sec;
    }
    update_link(ether, neighbor, rates, fwd, rev, seq);
    ptr += num_rates * sizeof(struct link_info);
  }

  p->kill();
  return 0;
}

/* Returns some information about the nodes world model */
String
BRNLinkStat::read_bcast_stats(bool with_pos)
{

  Timestamp now = Timestamp::now();

  Vector<EtherAddress> ether_addrs;

  for (ProbeMap::const_iterator i = _bcast_stats.begin(); i.live(); i++) 
    ether_addrs.push_back(i.key());

  //sort
  click_qsort(ether_addrs.begin(), ether_addrs.size(), sizeof(EtherAddress), etheraddr_sorter);

  StringAccum sa;

  //sa << "<entries>\n";
  sa << "<entry from='" << *(_me->getMyWirelessAddress()) << "'";
  if (with_pos)
    sa << " pos_x='&x;' pos_y='&y;' pos_z='&z;'";
  sa << " time_sec='" << now.sec() << "' time_subsec='" << now.subsec() << "'>\n";

  for (int i = 0; i < ether_addrs.size(); i++) {
    EtherAddress ether  = ether_addrs[i];
    probe_list_t *pl = _bcast_stats.findp(ether);

    sa << "\t<link to='" << ether << "'";
    if (with_pos)
      sa << " pos_x='&x;' pos_y='&y;' pos_z='&z;' ";
    sa << ">\n";

    int delta = -1;
    uint32_t traffic_cnt = 0;
//BRNNEW
    /*
    if (_packetCnt) {
      traffic_cnt = _packetCnt->get_traffic_count(ether, delta);
      //_packetCnt->reset();
    }
*/
    for (int x = 0; x < pl->_probe_types.size(); x++) {
      sa << "\t\t<link_info size='" << pl->_probe_types[x]._size << "'";
      sa << " rate='" << pl->_probe_types[x]._rate << "'";
      int rev_rate = pl->rev_rate(_start, pl->_probe_types[x]._rate,
                                 pl->_probe_types[x]._size);
      sa << " fwd = '" << pl->_fwd_rates[x] << "'";
      sa << " rev = '" << rev_rate << "'";
      sa << " traffic_period = '" << delta << "'";
      sa << " traffic_cnt = '" << traffic_cnt << "' />\n";
    }
    sa << "\t</link>\n";
/*
    sa << " seq " << pl->_seq;
    sa << " period " << pl->_period;
    sa << " tau " << pl->_tau;
    sa << " sent " << pl->_sent;
    sa << " last_rx " << now - pl->_last_rx;
    sa << "\n";
*/
  }

  sa << "</entry>\n";
  //sa << "</entries>";
//BRNNEW
  /*
  if (_packetCnt) {
    _packetCnt->reset();
  }

*/
  return sa.take_string();
}


/* Returns the nodes operating on wrong protocol. */
String 
BRNLinkStat::bad_nodes() {

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
BRNLinkStat::clear_stale() 
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
BRNLinkStat::reset()
{
  _neighbors.clear();
  _bcast_stats.clear();
  _seq = 0;
  _sent = 0;
  _start = Timestamp::now().timeval();
}

void
BRNLinkStat::run_log_timer()
{

//  _log_id++;
  Timestamp now = Timestamp::now();
  Vector<EtherAddress> ether_addrs;

  for (ProbeMap::const_iterator i = _bcast_stats.begin(); i.live(); i++) 
    ether_addrs.push_back(i.key());

  if (ether_addrs.size() == 0) {
    // reschedule
    _log_timeout_timer.schedule_after_msec(_log_interval);
    return;
  }

  //sort
  click_qsort(ether_addrs.begin(), ether_addrs.size(), sizeof(EtherAddress), etheraddr_sorter);

  StringAccum res;
  StringAccum sa;
  sa /*<< _log_id*/ << " " << *(_me->getMyWirelessAddress()) << " ";
  sa << now.sec() << " " << now.subsec() << " ";

  StringAccum sb;
  for (int i = 0; i < ether_addrs.size(); i++) {
    sb.clear();
    res << sa;
    EtherAddress ether  = ether_addrs[i];
    probe_list_t *pl = _bcast_stats.findp(ether);

    for (int x = 0; x < pl->_probe_types.size(); x++) {
      sb << ether << " " << pl->_probe_types[x]._size << " ";
      sb << pl->_probe_types[x]._rate << " ";
      int rev_rate = pl->rev_rate(_start, pl->_probe_types[x]._rate,
                                  pl->_probe_types[x]._size);
      sb << pl->_fwd_rates[x] << " " << rev_rate << "\n";
    }
    res << sb;
  }

  String res_s = res.take_string();

//  click_chatter("to write: %s\n", res_s.c_str());
/*
  int wrote = fwrite(res_s.c_str(), res_s.length(), 1, _log_fp);

  if (wrote == 0)
    click_chatter("BRNLinkStat(%s): dump2 %s", _log_filename.c_str(), strerror(errno));
*/
  // reschedule
  _log_timeout_timer.schedule_after_msec(_log_interval);
}


EXPORT_ELEMENT(BRNLinkStat)

#include <click/bighashmap.cc>
#include <click/vector.cc>
#include <click/dequeue.cc>

CLICK_ENDDECLS
