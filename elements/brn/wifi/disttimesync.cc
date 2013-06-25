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

#include "elements/brn/wifi/disttimesync.hh"
#include "elements/brn/wifi/disttimesyncprotocol.hh"

CLICK_DECLS


DistTimeSync::DistTimeSync() :
  _time_drift(DISTTIMESYNC_INIT),
  _pkt_buf_size(PKT_BUF_SIZE),
  _pkt_buf_idx(DISTTIMESYNC_INIT),
  pkt_buf(NULL)
{
  BRNElement::init();
}

DistTimeSync::~DistTimeSync()
{
  if (pkt_buf != NULL)
    delete pkt_buf;
}

int
DistTimeSync::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret = cp_va_kparse(conf, this, errh,
                     "LINKSTAT", cpkP+cpkM , cpElement, &_linkstat,
                     "TIMEDRIFT", cpkP, cpInteger, &_time_drift,
                     "PKT_BUF", cpkP, cpInteger, &_pkt_buf_size,
                     "DEBUG", cpkP, cpInteger, &_debug,
                     cpEnd);

  if (_pkt_buf_size <= 0) {
    click_chatter("Error @ DistTimeSync::configure():\n");
    click_chatter("Given packet buffer size was <= 0\n");
  }

  pkt_buf = new struct PacketSyncInfo[_pkt_buf_size];

  for (u_int32_t i = 0; i < _pkt_buf_size; i++) {
    pkt_buf[i].seq_no    = DISTTIMESYNC_INIT;
    pkt_buf[i].host_time = DISTTIMESYNC_INIT;

    for (int j = 0; j < ETHER_ADDR_SIZE; j++)
      pkt_buf[i].src[j] = 0;
  }

/* TODO: set artificial time drift when simulating
  if (_time_drift == TD_RANDOM) {
    ...
  }
*/

  return ret;
}

static int
tx_handler(void *element, const EtherAddress * /*ea*/, char *buffer, int size)
{
  click_chatter("tx\n");
  DistTimeSync *dts = (DistTimeSync *) element;
  return dts->lpSendHandler(buffer, size);
}

static int
rx_handler(void *element, EtherAddress * /*ea*/, char *buffer, int size,
            bool /*is_neighbour*/, uint8_t /*fwd_rate*/, uint8_t /*rev_rate*/)
{
  click_chatter("rx\n");
  DistTimeSync *dts = (DistTimeSync *) element;
  return dts->lpReceiveHandler(buffer, size);
}

int DistTimeSync::initialize(ErrorHandler *)
{
  BRN_DEBUG("init");
  _linkstat->registerHandler(this, BRN2_LINKSTAT_MINOR_TYPE_TIMESYNC,
                                                    &tx_handler, &rx_handler);
  return 0;
}

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
#ifdef NS_CLICK
  u_int64_t host_time = p->timestamp_anno().usecval();
#else
  u_int64_t host_time = BrnWifi::get_host_time(ceh);
#endif

  if (!host_time)
    host_time = p->timestamp_anno().usecval();

  EtherAddress src = get_sender_addr(wh);
  u_int16_t seq_no = wh->i_seq;

  /* Get the handle of the current packet buffer slot.If empty -> handle = 0 */
  u_int32_t current_handle;
  current_handle = create_packet_handle(pkt_buf[_pkt_buf_idx].seq_no,
                                        pkt_buf[_pkt_buf_idx].src);

  /*
   * If a ring buffer entry is to be overwritten, delete the entry
   * from the packet handle hash table.
   */
  if (current_handle != 0)
    pit.erase(current_handle);

  /* write to packet buffer */
  memcpy(pkt_buf[_pkt_buf_idx].src, src.data(), ETHER_ADDR_SIZE);
  pkt_buf[_pkt_buf_idx].seq_no = seq_no;
  pkt_buf[_pkt_buf_idx].host_time = host_time;

  /* create new packet handle: [seq_no (16) + last 2 bytes MAC] = 32 bit */
  u_int32_t packet_handle;
  packet_handle = create_packet_handle(seq_no, pkt_buf[_pkt_buf_idx].src);

  /* insert into packet index hash table */
  pit.insert(packet_handle, _pkt_buf_idx);

  //BRN_DEBUG("sa: seqno: %d hst: %lu src %s at idx %d\n",
  //seq_no, host_time, src.unparse().c_str(), _pkt_buf_idx);

  _pkt_buf_idx = (_pkt_buf_idx + 1) & (_pkt_buf_size - 1);

  return p;
}

/*
 * Creates a unique packet identifier consisting the sequence number and
 * the last 2 bytes of the sender MAC address thus creating a 4 byte handle:
 * [0 seq no 15 | last 2 bytes MAC 31]
 */
u_int32_t
DistTimeSync::create_packet_handle(u_int16_t seq_no, u_int8_t *src_addr)
{
  u_int32_t packet_handle = 0;
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

  u_int32_t max_pkts; /* max num of pkts that fit into buf. */

  max_pkts = size / sizeof(struct PacketSyncInfo);
  if (max_pkts > BUNDLE_SIZE)
    max_pkts = BUNDLE_SIZE;

  BRN_DEBUG("txh: size=%d max_pkts=%d\n", size, max_pkts);

  int32_t idx; /* new index used to select the pkts we want to send */

  idx = _pkt_buf_idx - BUNDLE_SIZE;
  if (idx < 0)
    idx = PKT_BUF_SIZE + idx;

  BRN_DEBUG("idx: %d buf_idx: %d\n", idx, _pkt_buf_idx);

  u_int32_t current_handle;
  u_int32_t p; /* packet indicator for manouvering in bundle/buf */

  p = 0;
  while (idx != ((int32_t) _pkt_buf_idx)) {

    if (max_pkts == 0)
      break;

    current_handle = create_packet_handle(pkt_buf[idx].seq_no,pkt_buf[idx].src);
    BRN_DEBUG("txh: idx=%d, pkt_h=%d\n", idx, current_handle);

    if (current_handle != 0) { /* valid entry */
      memcpy(bundle + p, pkt_buf + idx, sizeof(struct PacketSyncInfo));
      ++p;
      --max_pkts;
    }
    idx = (idx + 1) & (PKT_BUF_SIZE - 1);
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
DistTimeSync::lpReceiveHandler(char *buf, int size)
{
  struct PacketSyncInfo *bundle = (struct PacketSyncInfo *) buf;

  u_int32_t len;     /* max. bytes we will from the received buffer */
  u_int32_t rx_pkts; /* complete packets contained in the received buffer */

  rx_pkts = size / sizeof(struct PacketSyncInfo);

  if (rx_pkts >= BUNDLE_SIZE) {
    rx_pkts = BUNDLE_SIZE;
    len = BUNDLE_SIZE * sizeof(struct PacketSyncInfo);
  } else if (rx_pkts < BUNDLE_SIZE) {
    len = rx_pkts * sizeof(struct PacketSyncInfo);
  }

  u_int32_t packet_handle;
  u_int32_t idx;

  for (u_int8_t i = 0; i < rx_pkts; i++) { /* for each received packet */
    packet_handle = create_packet_handle(bundle[i].seq_no, bundle[i].src);
    BRN_DEBUG("rxh: pkt_h: %d node: %s\n", packet_handle,
                                EtherAddress(bundle[i].src).unparse().c_str());

    if (!packet_handle) /* no valid packet */
      continue;

    if (pit.findp(packet_handle)) { /* valid packet + buffered */
      EtherAddress src(bundle[i].src);
      idx = pit.find(packet_handle);
      BRN_DEBUG("rxh: buffered at index %d\n", idx);

      if (tdt.findp(src)) { /* there is a hst_diff buf for this node */
        HostTimeBuf *hst_buf = tdt.find(src);
        BRN_DEBUG("rxh: Diff Buffer found for %s", src.unparse().c_str());

        /* No diff for this pkt -> create one.
         * Otherwise do nothing, since we already got a diff */
        if (hst_buf->has_pkt(packet_handle) < 0) {
          BRN_DEBUG("rxh: diff buf doesn't have pkt");
          struct HostTimeDiff *diff = &(hst_buf->hst_diffs[hst_buf->hst_idx]);

          diff->hst_diff = pkt_buf[idx].host_time - bundle[i].host_time;
          if (diff->hst_diff < 0)
            diff->hst_diff *= -1;

          diff->pkt_handle = packet_handle;
          diff->ts = Timestamp::now();

          hst_buf->hst_idx = (hst_buf->hst_idx + 1) & (HST_BUF_SIZE - 1);
          BRN_DEBUG("rxh: diff buf entry used, hst_idx now at %d\n",
                                                            hst_buf->hst_idx);
        }

       /* No hst_diff for this node yet.
        * Create a new tdt entry for this node */
      } else {
        BRN_DEBUG("rxh: no diff buf");
        HostTimeBuf *hst_buf = new HostTimeBuf();
        struct HostTimeDiff *diff = &(hst_buf->hst_diffs[hst_buf->hst_idx]);

        BRN_DEBUG("rxh: hst from buf: %lu hst from pkt: %lu\n",
                                  pkt_buf[idx].host_time, bundle[i].host_time);

        diff->hst_diff = pkt_buf[idx].host_time - bundle[i].host_time;
        if (diff->hst_diff < 0)
          diff->hst_diff *= -1;

        diff->pkt_handle = packet_handle;
        diff->ts = Timestamp::now();
        BRN_DEBUG("rxh: new buf allocated. diff %d at idx %d inserted",
                                            diff->hst_diff, hst_buf->hst_idx);

        hst_buf->hst_idx = (hst_buf->hst_idx + 1) & (HST_BUF_SIZE - 1);

        BRN_DEBUG("rxh: insert hst buf for node %s\n", src.unparse().c_str());
        tdt.insert(src, hst_buf);
      }
    } else {
      BRN_DEBUG("rxh: not buffered");
    }

  }

  BRN_DEBUG("rxh: len: %d - size: %d\n", len, size);
  return len;
}

String DistTimeSync::stats_handler()
{
  StringAccum sa;
  sa << "hi\n";

  return sa.take_string();
}

static String DistTimeSync_read_stats(Element *e, void *)
{
  DistTimeSync *dstTs = (DistTimeSync *) e;
  return dstTs->stats_handler();
}

void DistTimeSync::add_handlers()
{
  BRNElement::add_handlers();
  add_read_handler("stats", DistTimeSync_read_stats, (void *) 0);
}

HostTimeBuf::HostTimeBuf() :
  hst_idx(0)
{
  hst_diffs = new HostTimeDiff[HST_BUF_SIZE];

  for (int i = 0; i < HST_BUF_SIZE; i++) {
    hst_diffs[i].hst_diff = 0;
    hst_diffs[i].ts = Timestamp();
    hst_diffs[i].pkt_handle = 0;
  }
}

HostTimeBuf::~HostTimeBuf()
{
  delete hst_diffs;
}

int HostTimeBuf::has_pkt(u_int32_t packet_handle)
{
  for (int i = 0; i < HST_BUF_SIZE; i++) {
    if (hst_diffs[i].pkt_handle == packet_handle)
      return i;
  }

  return -1;
}

EtherAddress get_sender_addr(struct click_wifi *wh)
{
  EtherAddress src;

  switch (wh->i_fc[1] & WIFI_FC1_DIR_MASK) {
  case WIFI_FC1_DIR_NODS:
    //dst = EtherAddress(wh->i_addr1);
    src = EtherAddress(wh->i_addr2);
    //bssid = EtherAddress(wh->i_addr3);
    break;
  case WIFI_FC1_DIR_TODS:
    //bssid = EtherAddress(wh->i_addr1);
    src = EtherAddress(wh->i_addr2);
    //dst = EtherAddress(wh->i_addr3);
    break;
  case WIFI_FC1_DIR_FROMDS:
    //dst = EtherAddress(wh->i_addr1);
    //bssid = EtherAddress(wh->i_addr2);
    src = EtherAddress(wh->i_addr3);
    break;
  case WIFI_FC1_DIR_DSTODS:
    //dst = EtherAddress(wh->i_addr1);
    src = EtherAddress(wh->i_addr2);
    //bssid = EtherAddress(wh->i_addr3);
    break;
  default:
    click_chatter("Error @ DistTimeSync::get_sender_addr():\n");
    click_chatter("Default case.\n");
  }

  return src;
}



CLICK_ENDDECLS

EXPORT_ELEMENT(DistTimeSync)
