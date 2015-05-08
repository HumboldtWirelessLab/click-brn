#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <click/packet.hh>
#include <click/timestamp.hh>

#include <clicknet/wifi.h>

#include "elements/brn/wifi/brnwifi.hh"
#include "elements/brn/brnelement.hh"
#include "elements/wifi/bitrate.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinkstat.hh"

#include "disttimesync.hh"
#include "disttimesyncprotocol.hh"

CLICK_DECLS


DistTimeSync::DistTimeSync() :
  _time_drift(DISTTIMESYNC_INIT),
  _pkt_buf_size(PKT_BUF_SIZE),
  _pkt_buf_idx(DISTTIMESYNC_INIT),
  node_id(-1)
{
  BRNElement::init();
}

DistTimeSync::~DistTimeSync()
{
}

int
DistTimeSync::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret = cp_va_kparse(conf, this, errh,
                     "LINKSTAT", cpkP+cpkM , cpElement, &_linkstat,
                     "TIMEDRIFT", cpkP, cpInteger, &_time_drift,
                     "OFFSET", cpkP, cpInteger, &_offset,
                     "DEBUG", cpkP, cpInteger, &_debug,
                     cpEnd);

  for (uint32_t i = 0; i < PKT_BUF_SIZE; ++i) {
    pkt_buf[i].seq_no    = DISTTIMESYNC_INIT;
    pkt_buf[i].host_time = DISTTIMESYNC_INIT;

    memset(pkt_buf[i].src,0,ETHER_ADDR_SIZE);
  }

  static int id = 1;
  //click_chatter("node %2d: td: %d off: %d\n", id, _time_drift, _offset);
  node_id = id;
  ++id;

  return ret;
}

static int
tx_handler(void *element, const EtherAddress * ea, char *buffer, int size)
{
  /* unused parameter */
  (void) ea;

  DistTimeSync *dts = (DistTimeSync *) element;
  return dts->lpSendHandler(buffer, size);
}

static int
rx_handler(void *element, EtherAddress *ea, char *buffer, int size,
            bool is_neighbour, uint8_t fwd_rate, uint8_t rev_rate)
{
  /* unused parameter */
  (void) is_neighbour;
  (void) fwd_rate;
  (void) rev_rate;

  DistTimeSync *dts = (DistTimeSync *) element;
  return dts->lpReceiveHandler(buffer, size, *ea);
}

int
DistTimeSync::initialize(ErrorHandler *)
{
#if CLICK_NS
  click_brn_srandom();

  if (_time_drift < 0)
    _time_drift = click_random(1, 20);

  if (_offset < 0)
    _offset = click_random() % DTS_MAX_OFFSET;
#endif /* CLICK_NS */

  _linkstat->registerHandler(this, BRN2_LINKSTAT_MINOR_TYPE_TIMESYNC,
                                                    &tx_handler, &rx_handler);
  return 0;
}

static EtherAddress get_sender_addr(struct click_wifi *wh);
static uint64_t apply_drift(uint64_t hst, int32_t off, int32_t td);

Packet *
DistTimeSync::simple_action(Packet *p)
{
  click_wifi *wh = (struct click_wifi *) p->data();
  click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

  /* Only use unicast without retries or broadcast packets. */
  if ((ceh->flags & WIFI_EXTRA_TX) ||
      (ceh->flags & WIFI_EXTRA_RX_ERR) ||
      (wh->i_fc[1] & WIFI_FC1_RETRY)) {
    BRN_DEBUG("no good");
    return p;
  }

  /* Use packet anno time stamp, if no driver timestamp is available. */
#ifdef CLICK_NS
  uint64_t host_time = p->timestamp_anno().usecval();
#else
  uint64_t host_time = BrnWifi::get_host_time(ceh);

  if (host_time == 0)
    host_time = p->timestamp_anno().usecval();
#endif /* CLICK_NS */

  if (_time_drift != 0 || _offset != 0)
    host_time = apply_drift(host_time, _offset, _time_drift);

  EtherAddress src = get_sender_addr(wh);
  u_int16_t seq_no = wh->i_seq;

  /*
   * Get the handle of the current packet buffer slot. This slot will be
   * overwritten by the current packet. So create the packet handle of this
   * slot to erase it from the packet index table (PIT).
   */
  uint32_t current_handle;
  current_handle = create_pkt_handle(pkt_buf[_pkt_buf_idx].seq_no,
                                        pkt_buf[_pkt_buf_idx].src);

  if (current_handle != 0)
    pit.erase(current_handle);

  /*
   * Write this packet to the packet buffer, create a new packet for this
   * packet and insert both, packet handle and buffer index into the PIT.
   */
  memcpy(pkt_buf[_pkt_buf_idx].src, src.data(), ETHER_ADDR_SIZE);
  pkt_buf[_pkt_buf_idx].seq_no = seq_no;
  pkt_buf[_pkt_buf_idx].host_time = host_time;

  uint32_t packet_handle;
  packet_handle = create_pkt_handle(seq_no, pkt_buf[_pkt_buf_idx].src);

  pit.insert(packet_handle, _pkt_buf_idx);

  _pkt_buf_idx = (_pkt_buf_idx + 1) % PKT_BUF_SIZE;

  return p;
}

/*
 * Creates a unique packet identifier consisting the sequence number and
 * the last 2 bytes of the sender MAC address thus creating a 4 byte handle:
 * [seq no | last 2 bytes MAC]
 * 0       15                31
 */
uint32_t
DistTimeSync::create_pkt_handle(u_int16_t seq_no, u_int8_t *src_addr)
{
  uint32_t packet_handle = 0;
  mempcpy(mempcpy(&packet_handle, &seq_no, sizeof(u_int16_t)),
          src_addr + 4, sizeof(u_int16_t));

  return packet_handle;
}

/*
 * Link probe handler function. Sends the last BUNDLE_SIZE number of 'packets'
 * from our PacketSyncInfo buffer to our neighbours.
 */
int
DistTimeSync::lpSendHandler(char *buf, int size)
{
  struct PacketSyncInfo *bundle = (struct PacketSyncInfo *) buf;

  uint32_t max_pkts; /* max num of pkts that fit into buf. */

  max_pkts = size / sizeof(struct PacketSyncInfo);
  if (max_pkts > BUNDLE_SIZE)
    max_pkts = BUNDLE_SIZE;
  else if (max_pkts < BUNDLE_SIZE) {
    BRN_DEBUG("txh: buffer too small!");
    return 0;
  }

  BRN_DEBUG("txh: size=%d max_pkts=%d\n", size, max_pkts);

  int32_t idx; /* new index used to select the pkts we want to send */

  idx = _pkt_buf_idx - BUNDLE_SIZE;
  if (idx < 0)
    idx = PKT_BUF_SIZE + idx;

  BRN_DEBUG("idx: %d buf_idx: %d\n", idx, _pkt_buf_idx);

  uint32_t current_handle;
  uint32_t p; /* packet indicator for manouvering in bundle/buf */

  p = 0;
  while (idx != ((int32_t) _pkt_buf_idx)) {

    if (max_pkts == 0)
      break;

    current_handle = create_pkt_handle(pkt_buf[idx].seq_no,pkt_buf[idx].src);
    BRN_DEBUG("txh: idx=%d, pkt_h=%d\n", idx, current_handle);
    BRN_DEBUG("txh: copy pkt with hst %lu to buf\n", pkt_buf[idx].host_time);

    if (current_handle != 0) { /* valid entry */
      memcpy(bundle + p, pkt_buf + idx, sizeof(struct PacketSyncInfo));
      ++p;
      --max_pkts;
    }
    idx = (idx + 1) % PKT_BUF_SIZE;
  }

  BRN_DEBUG("txh: size: %d idx: %d pkt_idx: %d p: %d\n", size, idx,
                                                         _pkt_buf_idx, p);
  return p * sizeof(struct PacketSyncInfo);
}

/*
 * Link probe handler function. Receives PacketSyncInfos from our neighbours.
 * If a packet for which we received sync info currently resides in our
 * PacketSyncInfo buffer, this function calculates a host time diff for this
 * packet and saves this diff into a host time diff buffer.
 */
int
DistTimeSync::lpReceiveHandler(char *buf, int size, EtherAddress probe_src)
{
  BRN_DEBUG("rxh: now: %lu\n", Timestamp::now().usecval());
  BRN_DEBUG("FROM: %s\n", probe_src.unparse().c_str());

  struct PacketSyncInfo *bundle = (struct PacketSyncInfo *) buf;

  uint32_t len;     /* max. bytes we will read from the received buffer */
  uint32_t rx_pkts; /* complete packets contained in the received buffer */

  rx_pkts = size / sizeof(struct PacketSyncInfo);

  if (rx_pkts >= BUNDLE_SIZE) {
    rx_pkts = BUNDLE_SIZE;
    len = BUNDLE_SIZE * sizeof(struct PacketSyncInfo);
  } else if (rx_pkts < BUNDLE_SIZE) {
    len = rx_pkts * sizeof(struct PacketSyncInfo);
  }

  BRN_DEBUG("rxh: bundle content:\n");
  for (u_int8_t i = 0; i < rx_pkts; i++) {
    BRN_DEBUG("\trxh: src: %s seqno: %d hst: %lu\n",
        EtherAddress(bundle[i].src).unparse().c_str(),
        bundle[i].seq_no,
        bundle[i].host_time
    );
  }

  uint32_t packet_handle;
  uint32_t idx;

  for (u_int8_t i = 0; i < rx_pkts; i++) { /* for each received packet */
    packet_handle  = create_pkt_handle(bundle[i].seq_no, bundle[i].src);

    BRN_DEBUG("rxh: rx'd pkt info:");
    BRN_DEBUG("rxh:\tsrc: %s seqno: %d hst: %lu\n",
        EtherAddress(bundle[i].src).unparse().c_str(),
        bundle[i].seq_no,
        bundle[i].host_time
    );

    if (!packet_handle) /* no valid packet */
      continue;

    if (pit.findp(packet_handle)) { /* valid packet + buffered */
      idx = pit.find(packet_handle);
      BRN_DEBUG("rxh: buffered at index %d\n", idx);

      if (tbt.findp(probe_src)) { /* there's a tpl buf for the probing node */
        HostTimeBuf *hst_buf = tbt.find(probe_src);
        BRN_DEBUG("rxh: tuple buf found for %s", probe_src.unparse().c_str());

        /* No tuple for this pkt -> create one.
         * Otherwise do nothing, since we already got a tuple */
        if (hst_buf->has_pkt(packet_handle) < 0) {
          BRN_DEBUG("rxh: tuple buf doesn't have pkt");
          struct HostTimeTuple *tpl = &(hst_buf->hst_tpls[hst_buf->hst_idx]);

          tpl->hst_me = pkt_buf[idx].host_time;
          tpl->hst_nb = bundle[i].host_time;
          tpl->pkt_handle = packet_handle;

          BRN_DEBUG("rxh: tuple for this new pkt: (%lu, %lu) at idx %d\n",
                    tpl->hst_me,
                    tpl->hst_nb,
                    hst_buf->hst_idx
          );

          hst_buf->hst_idx = (hst_buf->hst_idx + 1) % HST_BUF_SIZE;
          BRN_DEBUG("rxh: new idx at %d\n", hst_buf->hst_idx);

          if (hst_buf->entries < HST_BUF_SIZE)
            ++(hst_buf->entries);

        } else { /* already got a tuple */
          BRN_DEBUG("rxh: hst tuple for this pkt already buff'd");
          BRN_DEBUG("rxh: tuple is (%lu, %lu)\n",
            hst_buf->hst_tpls[i].hst_me,
            hst_buf->hst_tpls[i].hst_nb
          );
        }

       /* No hst_buf for this node yet.
        * Create a new tbt entry for this node */
      } else {
        BRN_DEBUG("rxh: no diff buf");
        HostTimeBuf *hst_buf = new HostTimeBuf();
        struct HostTimeTuple *tpl = &(hst_buf->hst_tpls[hst_buf->hst_idx]);

        tpl->hst_me = pkt_buf[idx].host_time;
        tpl->hst_nb = bundle[i].host_time;
        tpl->pkt_handle = packet_handle;

        BRN_DEBUG("rxh: new tuple (%lu, %lu)\n",
          pkt_buf[idx].host_time,
          bundle[i].host_time
        );

        BRN_DEBUG("rxh: new buf allocated. new tuple at idx %d inserted",
          hst_buf->hst_idx
        );

        hst_buf->hst_idx = (hst_buf->hst_idx + 1) % HST_BUF_SIZE;

        if (hst_buf->entries < HST_BUF_SIZE)
          ++(hst_buf->entries);

        BRN_DEBUG("rxh: insert hst buf for node %s\n",
          probe_src.unparse().c_str()
        );
        tbt.insert(probe_src, hst_buf);
      }
    } else {
      BRN_DEBUG("rxh: not buffered");
    }
  }

  BRN_DEBUG("rxh: len: %d - size: %d\n\n", len, size);

  return len;
}

static uint32_t get_abs(int n);

String DistTimeSync::stats_handler()
{
  BRN_INFO("");
  StringAccum sa;

  HostTimeBuf *hst_buf;
  EtherAddress neighbor;

  double drift;
  int32_t offset;

  uint64_t now;
  uint64_t last_update;

  for (TBTIter iter = tbt.begin(); iter.live(); ++iter) {
    neighbor = iter.key();
    hst_buf  = iter.value();

    sa << "Nb: " << neighbor.unparse().c_str() << "\n";

    if (hst_buf->entries < 2) {
      sa << "not enough hst tuples\n";
      sa << "\n";
      continue;
    }

    now = Timestamp::now().usecval();
    last_update = hst_buf->di.ts;

    if ((get_abs(now - last_update)) < DTS_TD_CACHE_DURATION) {
      drift  = hst_buf->di.drift;
      offset = hst_buf->di.offset;
    } else {
      drift  = hst_buf->get_timedrift();
      offset = hst_buf->get_offset();
      hst_buf->di.ts = now;
    }

    sa << "td:  " << drift  << "\n";
    sa << "off: " << offset << "\n";
    sa << "\n";
  }
  sa << "\n\n";

  return sa.take_string();
}

static String DistTimeSync_read_stats(Element *e, void *thunk);

void DistTimeSync::add_handlers()
{
  BRNElement::add_handlers();
  add_read_handler("stats", DistTimeSync_read_stats, (void *) 0);
}

HostTimeBuf::HostTimeBuf() :
  hst_idx(0),
  entries(0)
{
  hst_tpls = new HostTimeTuple[HST_BUF_SIZE];

  for (int i = 0; i < HST_BUF_SIZE; i++) {
    hst_tpls[i].hst_me = 0;
    hst_tpls[i].hst_nb = 0;
    hst_tpls[i].pkt_handle = 0;
  }

  di.drift  = 0;
  di.offset = 0;
  di.ts     = 0;
}

HostTimeBuf::~HostTimeBuf()
{
  delete hst_tpls;
}

int HostTimeBuf::has_pkt(uint32_t packet_handle)
{
  for (int i = 0; i < HST_BUF_SIZE; i++) {
    if (hst_tpls[i].pkt_handle == packet_handle)
      return i;
  }

  return -1;
}

/*
 * Calculates the time drift, which is essentially the slope of a
 * straight line, based on the first and last host time tuple buffered in the
 * host time tuple buffer.
 */
double HostTimeBuf::get_timedrift()
{
  if (entries < 2) /* 2 points on a line are needed to calculate the slope. */
    return -1;

  double drift;
  uint64_t delta_x;
  uint64_t delta_y;

  delta_x = hst_tpls[entries - 1].hst_me - hst_tpls[0].hst_me;
  delta_y = hst_tpls[entries - 1].hst_nb - hst_tpls[0].hst_nb;

  if (delta_x != 0)
    drift = (double) delta_y / (double) delta_x;
  else
    drift = -1;

  return drift;
}

int64_t HostTimeBuf::get_offset()
{
  if (hst_idx == 0)
    return -1;

  return hst_tpls[0].hst_nb - hst_tpls[0].hst_me;
}

static EtherAddress get_sender_addr(struct click_wifi *wh)
{
  EtherAddress src;

  switch (wh->i_fc[1] & WIFI_FC1_DIR_MASK) {
  case WIFI_FC1_DIR_NODS:
  case WIFI_FC1_DIR_TODS:
  case WIFI_FC1_DIR_DSTODS:
    src = EtherAddress(wh->i_addr2);
    break;
  case WIFI_FC1_DIR_FROMDS:
    src = EtherAddress(wh->i_addr3);
    break;
  default:
    click_chatter("Error @ DistTimeSync::get_sender_addr():\n");
    click_chatter("Default case.\n");
  }

  return src;
}

static uint64_t apply_drift(uint64_t hst, int32_t off, int32_t td)
{
  if (td > 0)
    hst *= td;
  hst += off;

  return hst;
}

static String DistTimeSync_read_stats(Element *e, void *thunk)
{
  static int i = 0;
  ++i;
  click_chatter("\nreadstats: %d\n\n", i);

  /* unused parameter */
  (void) thunk;

  DistTimeSync *dstTs = (DistTimeSync *) e;
  return dstTs->stats_handler();
}

static uint32_t get_abs(int n)
{
  return (n < 0) ? -(unsigned)n : n;
}


CLICK_ENDDECLS

EXPORT_ELEMENT(DistTimeSync)
