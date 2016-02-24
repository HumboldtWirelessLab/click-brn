#include <click/config.h>
#include "wmmwifiencap.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <click/packet_anno.hh>
#include <elements/wifi/wirelessinfo.hh>

#include "elements/brn/wifi/brnwifi.hh"

CLICK_DECLS

WMMWifiEncap::WMMWifiEncap()
  : qos(0),queue(0),_debug(false),_mode(0),_winfo(NULL)
{
}

WMMWifiEncap::~WMMWifiEncap()
{
}

int
WMMWifiEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int _queue = 0;
  int _qos = 0;

  _debug = false;
  _mode = WIFI_FC1_DIR_NODS;
  if (cp_va_kparse(conf, this, errh,
		   "MODE", cpkP+cpkM, cpUnsigned, &_mode,
		   "BSSID", cpkP, cpEthernetAddress, &_bssid,
       "QOS", cpkP, cpInteger, &_qos,
       "QUEUE", cpkP, cpInteger, &_queue,
       "WIRELESS_INFO", 0, cpElement, &_winfo,
		   "DEBUG", 0, cpBool, &_debug,
		   cpEnd) < 0)
    return -1;

  queue = _queue;
  qos = _qos;

  return 0;
}

Packet *
WMMWifiEncap::simple_action(Packet *p)
{


  EtherAddress src;
  EtherAddress dst;
  EtherAddress bssid = _winfo ? _winfo->_bssid : _bssid;

  uint16_t ethtype;
  WritablePacket *p_out = 0;

  struct wifi_qos_field *qos_field;

  if (p->length() < sizeof(struct click_ether)) {
    click_chatter("%{element}: packet too small: %d vs %d\n",
		  this,
		  p->length(),
		  sizeof(struct click_ether));

    p->kill();
    return 0;

  }

  const click_ether *eh = reinterpret_cast<const click_ether *>( p->data());
  src = EtherAddress(eh->ether_shost);
  dst = EtherAddress(eh->ether_dhost);
  memcpy(&ethtype, p->data() + 12, 2);

  p_out = p->uniqueify();
  if (!p_out) {
    return 0;
  }


  p_out->pull(sizeof(struct click_ether));
  p_out = p_out->push(sizeof(struct click_llc));

  if (!p_out) {
    return 0;
  }

  memcpy(p_out->data(), WIFI_LLC_HEADER, WIFI_LLC_HEADER_LEN);
  memcpy(p_out->data() + 6, &ethtype, 2);

  //add 2 byte for QOS
  if (!(p_out = p_out->push(sizeof(struct click_wifi) + 2 * sizeof(uint8_t)))) return 0;

  struct click_wifi *w = (struct click_wifi *) p_out->data();

  memset(p_out->data(), 0, sizeof(click_wifi));
  w->i_fc[0] = (uint8_t) (WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_DATA | WIFI_FC0_SUBTYPE_QOS);
  w->i_fc[1] = 0;
  w->i_fc[1] |= (uint8_t) (WIFI_FC1_DIR_MASK & _mode);

  qos_field = (struct wifi_qos_field*)&(w[1]);

  qos_field->qos = qos;        //16 + 8 + 4;
  qos_field->queue = queue;    //queue-size

  switch (_mode) {
  case WIFI_FC1_DIR_NODS:
    memcpy(w->i_addr1, dst.data(), 6);
    memcpy(w->i_addr2, src.data(), 6);
    memcpy(w->i_addr3, bssid.data(), 6);
    break;
  case WIFI_FC1_DIR_TODS:
    memcpy(w->i_addr1, bssid.data(), 6);
    memcpy(w->i_addr2, src.data(), 6);
    memcpy(w->i_addr3, dst.data(), 6);
    break;
  case WIFI_FC1_DIR_FROMDS:
    memcpy(w->i_addr1, dst.data(), 6);
    memcpy(w->i_addr2, bssid.data(), 6);
    memcpy(w->i_addr3, src.data(), 6);
    break;
  case WIFI_FC1_DIR_DSTODS:
    /* XXX this is wrong */
    memcpy(w->i_addr1, dst.data(), 6);
    memcpy(w->i_addr2, src.data(), 6);
    memcpy(w->i_addr3, bssid.data(), 6);
    break;
  default:
    click_chatter("%{element}: invalid mode %d\n",
		  this,
		  _mode);
    p_out->kill();
    return 0;
  }
  return p_out;
}


enum {H_DEBUG, H_MODE, H_BSSID};

static String
WMMWifiEncap_read_param(Element *e, void *thunk)
{
  WMMWifiEncap *td = reinterpret_cast<WMMWifiEncap *>(e);
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
      case H_MODE:
	return String(td->_mode) + "\n";
      case H_BSSID:
	return td->_bssid.unparse() + "\n";
    default:
      return String();
    }
}
static int
WMMWifiEncap_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  WMMWifiEncap *f = reinterpret_cast<WMMWifiEncap *>(e);
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!cp_bool(s, &debug))
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }

  case H_MODE: {    //mode
    int m;
    if (!cp_integer(s, &m))
      return errh->error("mode parameter must be int");
    f->_mode = m;
    break;
  }
  case H_BSSID: {    //debug
    EtherAddress e;
    if (!cp_ethernet_address(s, &e))
      return errh->error("bssid parameter must be ethernet address");
    f->_bssid = e;
    break;
  }
  }
  return 0;
}

void
WMMWifiEncap::add_handlers()
{
  add_read_handler("debug", WMMWifiEncap_read_param, (void *) H_DEBUG);
  add_read_handler("mode", WMMWifiEncap_read_param, (void *) H_MODE);
  add_read_handler("bssid", WMMWifiEncap_read_param, (void *) H_BSSID);

  add_write_handler("debug", WMMWifiEncap_write_param, (void *) H_DEBUG);
  add_write_handler("mode", WMMWifiEncap_write_param, (void *) H_MODE);
  add_write_handler("bssid", WMMWifiEncap_write_param, (void *) H_BSSID);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(WMMWifiEncap)
