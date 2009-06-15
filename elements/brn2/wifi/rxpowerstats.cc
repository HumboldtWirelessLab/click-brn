#include <click/config.h>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <clicknet/wifi.h>
#include <click/etheraddress.hh>

#include "rxpowerstats.hh"
#include "elements/brn2/wifi/brnwifi.h"

CLICK_DECLS

int
RXPowerStats::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;
  _debug = false;

  ret = cp_va_kparse(conf, this, errh,
          "DEBUG", cpkP, cpBool, &_debug,
          cpEnd);

  return ret;
}

RXPowerStats::RxRateInfoList *
RXPowerStats::getRxRateInfoList(EtherAddress src)
{
  for ( int i = 0; i < _rxinfolist.size(); i++ ) {
    if ( memcmp(src.data(), _rxinfolist[i]._src.data(),6 ) == 0 )
      return &(_rxinfolist[i]._rxrateinfo);
  }

  _rxinfolist.push_back(RxSourceInfo(src));

  for ( int i = (_rxinfolist.size() - 1); i >= 0; i-- ) {
    if ( memcmp(src.data(), _rxinfolist[i]._src.data(),6 ) == 0 )
      return &(_rxinfolist[i]._rxrateinfo);
  }
  return NULL;
}

RXPowerStats::RxRateInfo *
RXPowerStats::getRxRateInfo(RXPowerStats::RxRateInfoList *rxinfo, uint8_t rate)
{
  for ( int i = 0; i < rxinfo->size(); i++ ) {
    if ( (*rxinfo)[i]._rate == rate )
      return &((*rxinfo)[i]);
  }

  rxinfo->push_back(RxRateInfo(rate));

  for ( int i = (rxinfo->size() - 1); i >= 0; i-- ) {
    if ( (*rxinfo)[i]._rate == rate )
      return &((*rxinfo)[i]);
  }
  return NULL;
}

void
RXPowerStats::push(int port, Packet *p)
{
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

  if ( !(ceh->flags & WIFI_EXTRA_TX) ) {
    struct click_wifi *w = (struct click_wifi *) p->data();
    struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

    EtherAddress src = EtherAddress(w->i_addr2);
    RxRateInfoList *rxril = getRxRateInfoList(src);
    RxRateInfo *rxri = getRxRateInfo(rxril, ceh->rate);

    uint8_t state = STATE_UNKNOWN;
    if ( ceh->magic == WIFI_EXTRA_MAGIC && ( (ceh->flags & WIFI_EXTRA_RX_ERR) == 0 ))
      state = STATE_OK;
    else if ( ceh->magic == WIFI_EXTRA_MAGIC && (ceh->flags & WIFI_EXTRA_RX_ERR) &&
             (ceh->flags & WIFI_EXTRA_RX_CRC_ERR) )
      state = STATE_CRC;
    else if ( ceh->magic == WIFI_EXTRA_MAGIC && (ceh->flags & WIFI_EXTRA_RX_ERR) &&
              (ceh->flags & WIFI_EXTRA_RX_PHY_ERR) )
      state = STATE_PHY;

    if ( ceh->rssi > 60 ) 
      rxri->_rxinfo.push_back(RxInfo(p->length(), 0, ((signed char)ceh->silence), state));
    else
      rxri->_rxinfo.push_back(RxInfo(p->length(), ceh->rssi, ((signed char)ceh->silence), state));
  }

  output(port).push(p);
}

String
RXPowerStats::stats()
{
  StringAccum sa;
  RxRateInfoList *rxril;
  RxInfoList *rxil;
  RxRateInfo *rxri;
  RxInfo *rxi;

  for ( int s = 0; s < _rxinfolist.size(); s++ ) {
    rxril = &(_rxinfolist[s]._rxrateinfo);

    sa << "Source: " << _rxinfolist[s]._src << "\n";
    for ( int r = 0; r < rxril->size(); r++ ) {
      rxri = &((*rxril)[r]);
      sa << "Rate: " << (int)rxri->_rate;

      rxil = &(rxri->_rxinfo);

      int sumpow = 0;

      for ( int x = 0; x < rxil->size(); x++ ) {
        rxi = &((*rxil)[x]);
        sumpow += rxi->_power;
      }

      sumpow /= rxil->size();
      sa << " Avg. Power: " << sumpow << "\n";
    }
  }
  return sa.take_string();
}

void 
RXPowerStats::reset()
{
  _rxinfolist.clear();
}

enum {H_DEBUG, H_STATS, H_RESET};

static String 
RXPowerStats_read_param(Element *e, void *thunk)
{
  RXPowerStats *td = (RXPowerStats *)e;
  switch ((uintptr_t) thunk) {
    case H_DEBUG:
      return String(td->_debug) + "\n";
    case H_STATS:
      return td->stats();
    default:
      return String();
  }
}

static int 
RXPowerStats_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  RXPowerStats *f = (RXPowerStats *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_DEBUG: {    //debug
      bool debug;
      if (!cp_bool(s, &debug))
        return errh->error("debug parameter must be boolean");
      f->_debug = debug;
      break;
    }
    case H_RESET: {    //reset
      f->reset();
    }
  }
  return 0;
}

void
RXPowerStats::add_handlers()
{
  add_read_handler("debug", RXPowerStats_read_param, (void *) H_DEBUG);
  add_read_handler("stats", RXPowerStats_read_param, (void *) H_STATS);

  add_write_handler("debug", RXPowerStats_write_param, (void *) H_DEBUG);
  add_write_handler("reset", RXPowerStats_write_param, (void *) H_RESET, Handler::BUTTON);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RXPowerStats)
