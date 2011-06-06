#include <click/config.h>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <clicknet/wifi.h>
#include <click/etheraddress.hh>
#include "brn2_printpacketstat.hh"
CLICK_DECLS


#define min(x,y)      ((x)<(y) ? (x) : (y))
#define max(x,y)      ((x)>(y) ? (x) : (y))

BRN2PrintPacketStat::BRN2PrintPacketStat()
  : _print_anno(false),
    _print_checksum(false)
{
  _label = "";
}

BRN2PrintPacketStat::~BRN2PrintPacketStat()
{
}

int
BRN2PrintPacketStat::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;
  _timestamp = false;
  ret = cp_va_kparse(conf, this, errh,
      "LABEL", cpkP, cpString, &_label,
      "TIMESTAMP", cpkP, cpBool, &_timestamp,
      cpEnd);

  return ret;
}

String
BRN2PrintPacketStat::status_string(int status) {
  switch (status) {

  case WIFI_STATUS_SUCCESS: return "success";
  case WIFI_STATUS_UNSPECIFIED: return "unspecified";
  case WIFI_STATUS_CAPINFO: return "capinfo";
  case WIFI_STATUS_NOT_ASSOCED: return "not_assoced";
  case WIFI_STATUS_OTHER: return "other";
  case WIFI_STATUS_ALG: return "alg";
  case WIFI_STATUS_SEQUENCE: return "seq";
  case WIFI_STATUS_CHALLENGE: return "challenge";
  case WIFI_STATUS_TIMEOUT: return "timeout";
  case WIFI_STATUS_BASIC_RATES: return "basic_rates";
  case WIFI_STATUS_TOO_MANY_STATIONS: return "too_many_stations";
  case WIFI_STATUS_RATES: return "rates";
  case WIFI_STATUS_SHORTSLOT_REQUIRED: return "shortslot_required";
  default: return "unknown status " + String(status);    
  }
}

Packet *
BRN2PrintPacketStat::simple_action(Packet *p)
{
  struct click_wifi *wh = (struct click_wifi *) p->data();
  struct click_wifi_extra *ceh = (struct click_wifi_extra *) p->anno();
  int type = wh->i_fc[0] & WIFI_FC0_TYPE_MASK;
//  int subtype = wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;
//  int duration = cpu_to_le16(wh->i_dur);
  EtherAddress src;
  EtherAddress dst;
  EtherAddress bssid;

  StringAccum sa;
  if (_label[0] != 0) {
    sa << _label << ": ";
  }
  if (_timestamp)
    sa << p->timestamp_anno() << " ";

  int len;
  len = sprintf(sa.reserve(9), "%4d ", p->length());
  sa.adjust_length(len);

  if (ceh->rate == 11) {
    sa << " 5.5";
  } else {
    len = sprintf(sa.reserve(2), "%2d", ceh->rate/2);
    sa.adjust_length(len);
  }
  sa << " ";

  len = sprintf(sa.reserve(9), "%2d ", ceh->rssi);
  sa.adjust_length(len);

  len = sprintf(sa.reserve(9), "%2d ", ceh->silence);
  sa.adjust_length(len);

  switch (wh->i_fc[1] & WIFI_FC1_DIR_MASK) {
  case WIFI_FC1_DIR_NODS:
    dst = EtherAddress(wh->i_addr1);
    src = EtherAddress(wh->i_addr2);
    bssid = EtherAddress(wh->i_addr3);
    break;
  case WIFI_FC1_DIR_TODS:
    bssid = EtherAddress(wh->i_addr1);
    src = EtherAddress(wh->i_addr2);
    dst = EtherAddress(wh->i_addr3);
    break;
  case WIFI_FC1_DIR_FROMDS:
    dst = EtherAddress(wh->i_addr1);
    bssid = EtherAddress(wh->i_addr2);
    src = EtherAddress(wh->i_addr3);
    break;
  case WIFI_FC1_DIR_DSTODS:
    dst = EtherAddress(wh->i_addr1);
    src = EtherAddress(wh->i_addr2);
    bssid = EtherAddress(wh->i_addr3);
    break;
  default:
    dst = EtherAddress();
    src = EtherAddress();
    bssid = EtherAddress();
  }

//  uint8_t *ptr = (uint8_t *) p->data() + sizeof(click_wifi);
  switch (type) {
  case WIFI_FC0_TYPE_MGT:
    sa << "mgmt ";
    break;
    case WIFI_FC0_TYPE_CTL:
    sa << "cntl ";
    break;
  case WIFI_FC0_TYPE_DATA:
    sa << "data ";
    break;
  default:
    sa << "unknown ";
  }

  sa << EtherAddress(wh->i_addr1);
  if (p->length() >= 16) {
    sa << " " << EtherAddress(wh->i_addr2);
  }
  else
    sa << " 00:00:00:00:00:00";

  if (p->length() > 22) {
    sa << " " << EtherAddress(wh->i_addr3);
  }
  else
    sa << " 00:00:00:00:00:00";
  sa << " ";

  if (p->length() >= sizeof(click_wifi)) {
    uint16_t seq = le16_to_cpu(*(u_int16_t *)wh->i_seq) >> WIFI_SEQ_SEQ_SHIFT;
//    uint8_t frag = le16_to_cpu(*(u_int16_t *)wh->i_seq) & WIFI_SEQ_FRAG_MASK;
    sa <<  (int) seq << " ";
  }
  else
    sa << "-1 ";

  uint16_t mactype;
  uint32_t packetid;
  uint16_t interval;
  uint8_t channel,power,bitrate;

  mactype = 0;
  packetid = 2000000000;
  interval = 0;
  channel = 0;
  bitrate = 0;
  power = 0;

  if ( ( p->length() >= (sizeof(struct click_wifi) + 6 + 2 + 15 ) ) && ( type == WIFI_FC0_TYPE_DATA ) ) {
    memcpy(&mactype,&(p->data()[sizeof(struct click_wifi) + 6]),sizeof(uint16_t));
    if ( mactype == 0x8780 ) {
      memcpy(&packetid,&(p->data()[sizeof(struct click_wifi) + 6 + 4]),sizeof(uint32_t));
      packetid = ntohl(packetid);

      memcpy(&interval,&(p->data()[sizeof(struct click_wifi) + 6 + 4 + sizeof(uint32_t)]),sizeof(uint16_t));
      interval=ntohs(interval);

      memcpy(&channel,&(p->data()[sizeof(struct click_wifi) + 6 + 4 + sizeof(uint32_t) + sizeof(uint16_t)]),sizeof(uint8_t));
      memcpy(&bitrate,&(p->data()[sizeof(struct click_wifi) + 6 + 4 + sizeof(uint32_t) + sizeof(uint16_t)+sizeof(uint8_t)]),sizeof(uint8_t));
      memcpy(&power,&(p->data()[sizeof(struct click_wifi) + 6 + 4 + sizeof(uint32_t) + sizeof(uint16_t)+sizeof(uint8_t)+sizeof(uint8_t)]),sizeof(uint8_t));

      mactype = 1;
    }
    else
      mactype = 0;
  }

  sa << mactype << " " << packetid << " " << interval << " " << (int)channel << " " << (int)bitrate << " " << (int)power;

  click_chatter("%s\n", sa.c_str());
  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2PrintPacketStat)
