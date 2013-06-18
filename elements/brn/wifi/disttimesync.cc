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
    pkt_buf[i].src       = new u_int8_t[ETHER_ADDR_SIZE];
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
rx_handler(void *element, EtherAddress * /*ea*/, char *buffer, int size, bool /*is_neighbour*/, uint8_t /*fwd_rate*/, uint8_t /*rev_rate*/)
{
  click_chatter("rx\n");
  DistTimeSync *dts = (DistTimeSync *) element;
  return dts->lpReceiveHandler(buffer, size);
}

int DistTimeSync::initialize(ErrorHandler *)
{
  BRN_DEBUG("init");
  _linkstat->registerHandler(this, BRN2_LINKSTAT_MINOR_TYPE_TIMESYNC, &tx_handler, &rx_handler);
  return 0;
}

Packet *
DistTimeSync::simple_action(Packet *p)
{
  click_wifi *wh = (struct click_wifi *) p->data();
  click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

/*
  if ((ceh->flags & WIFI_EXTRA_TX) || (ceh->flags & WIFI_EXTRA_RX_ERR) || (wh->i_fc[1] & WIFI_FC1_RETRY)) {
    BRN_DEBUG("no good");
    return p;
  }
*/

  if (ceh->flags & WIFI_EXTRA_TX) {
    BRN_DEBUG("WIFI_EXTRA_TX");
    return p;
  } else if (ceh->flags & WIFI_EXTRA_RX_ERR) {
    BRN_DEBUG("WIFI_EXTRA_RX_ERR");
    return p;
  } else if (wh->i_fc[1] & WIFI_FC1_RETRY) {
    BRN_DEBUG("WIFI_FC1_RETRY");

    EtherAddress dst = EtherAddress(wh->i_addr1);

    if (!dst.is_broadcast())
    	BRN_DEBUG("no BC %s", dst.unparse().c_str());


    return p;
  }

  EtherAddress src    = get_sender_addr(wh);
  u_int16_t seq_no    = wh->i_seq;
  u_int64_t host_time = BrnWifi::get_host_time(ceh);

  /* get the handle of the current packet buffer slot. If empty -> handle = 0 */
  u_int32_t current_handle = create_packet_handle(pkt_buf[_pkt_buf_idx].seq_no, pkt_buf[_pkt_buf_idx].src);

  BRN_DEBUG("seqno: %d", pkt_buf[_pkt_buf_idx].seq_no);

  //for (int i = 0; i < 6; i++)
	 // BRN_DEBUG("src[%d]: %d", i, pkt_buf[_pkt_buf_idx].src[i]);

  //BRN_DEBUG("handle: %d", current_handle);

  /* if a pkt_buf/ring buffer entry is to be overwritten, delete the entry from the packet handle hash table */
  if (current_handle != 0)
    pit.erase(current_handle);

  /* write to packet buffer */
  memcpy(pkt_buf[_pkt_buf_idx].src, src.data(), ETHER_ADDR_SIZE);
  pkt_buf[_pkt_buf_idx].seq_no = seq_no;
  pkt_buf[_pkt_buf_idx].host_time = host_time;

  /* create new packet key/handle [seq_no (16) + last 2 bytes MAC] = 32 */
  u_int32_t packet_handle = create_packet_handle(seq_no, pkt_buf[_pkt_buf_idx].src);

  /* insert into packet index hash table */
  pit.insert(packet_handle, _pkt_buf_idx);

  _pkt_buf_idx = (_pkt_buf_idx + 1) & (_pkt_buf_size - 1);

  return p;
}

u_int32_t
DistTimeSync::create_packet_handle(u_int16_t seq_no, u_int8_t *src_addr)
{
  u_int32_t packet_handle = 0;
  mempcpy(mempcpy(&packet_handle, &seq_no, sizeof(u_int16_t)), src_addr + 4, sizeof(u_int16_t));

  return packet_handle;
}

int
DistTimeSync::lpSendHandler(char *buf, int size)
{
  struct PacketSyncInfo *bundle = (struct PacketSyncInfo *) buf;
  u_int32_t current_handle;
  u_int32_t max_pkts;
  u_int32_t p = 0;
  int32_t idx = 0;

  /* check buf size */
  max_pkts = size / (int) sizeof(struct PacketSyncInfo);
  if (max_pkts < BUNDLE_SIZE) {
    BRN_DEBUG("buf size too small");
    return 0;
  }

  /* create new index used to select the pkts we want to send */
  idx = _pkt_buf_idx - BUNDLE_SIZE;
  if (idx < 0)
    idx = PKT_BUF_SIZE + idx;

  while (idx != ((int32_t) _pkt_buf_idx)) {
    current_handle = create_packet_handle(pkt_buf[idx].seq_no, pkt_buf[idx].src);

    if (current_handle != 0) { /* valid entry */
      memcpy(bundle + p, pkt_buf + idx, sizeof(struct PacketSyncInfo));
      p++;
    }

    idx = (idx + 1) & (PKT_BUF_SIZE - 1);
  }

  BRN_DEBUG("p: %d\n", p);
  if (p > 0)
    return p * sizeof(struct PacketSyncInfo);
  else
    return 1;
}

int
DistTimeSync::lpReceiveHandler(char *buf, int size)
{
  struct PacketSyncInfo *bundle = (struct PacketSyncInfo *) buf;
  u_int32_t len;

  /* check buf size */
  len = BUNDLE_SIZE * (int) sizeof(struct PacketSyncInfo);

  if (size < (int) len)
    return 0;

  int64_t hst_diff;
  u_int32_t packet_handle;
  u_int32_t idx;
  HostTimeBuf *hst_buf;

  for (int i = 0; i < BUNDLE_SIZE; i++) { /* for each packet */

    packet_handle = create_packet_handle(bundle[i].seq_no, bundle[i].src);
    if (packet_handle && pit.findp(packet_handle)) { /* valid packet + buffered */

      EtherAddress src(bundle[i].src);
      if (tdt.findp(src)) { /* hst_diff for this pkt already buffered */
        hst_buf = tdt.findp(src);

        if (!hst_buf_has_pkt(hst_buf, packet_handle)) {

        }

      }


      idx = pit.find(packet_handle);
      hst_diff = pkt_buf[idx].host_time - bundle[i].host_time;

      if (hst_diff < 0)
        hst_diff *= -1;



    }


  }

  return len;
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

bool hst_buf_has_pkt(HostTimeBuf */*hst_buf*/, u_int32_t /*packet_handle*/)
{
  for (int i = 0; i < HST_BUF_SIZE; i++) {
    //if ()
  }

  return true;
}

void print_info(Packet *p)
{
  StringAccum sa;

  click_wifi *wh;
  click_wifi_extra *ceh;

  EtherAddress src;
  //EtherAddress dst;
  //EtherAddress bssid;

  u_int16_t seq_no;
  u_int64_t host_time;

  wh  = (struct click_wifi *) p->data();
  ceh = WIFI_EXTRA_ANNO(p);

  /* get sender address */
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
    click_chatter("error!\n");
  }

  /* get sequence number and host time (k_time) */
  seq_no = wh->i_seq;
  host_time = BrnWifi::get_host_time(ceh);

  sa << "src: " << src << " seq: " << seq_no << " k_time: "  << host_time;
  click_chatter("%s\n", sa.c_str());
}

CLICK_ENDDECLS

EXPORT_ELEMENT(DistTimeSync)
