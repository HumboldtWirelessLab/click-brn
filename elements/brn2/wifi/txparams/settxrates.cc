#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>

#include "elements/brn2/wifi/brnwifi.hh"

#include "settxrates.hh"


CLICK_DECLS


SetTXRates::SetTXRates():
    _rate0(2), _rate1(0), _rate2(0), _rate3(0),
    _tries0(1), _tries1(0), _tries2(0), _tries3(0),
    _mcs0(false), _mcs1(false), _mcs2(false), _mcs3(false),
    _bw0(IEEE80211_MCS_BW_20), _bw1(IEEE80211_MCS_BW_20), _bw2(IEEE80211_MCS_BW_20), _bw3(IEEE80211_MCS_BW_20),
    _sgi0(false), _sgi1(false), _sgi2(false), _sgi3(false),
    _gf0(false), _gf1(false), _gf2(false), _gf3(false),
    _fec0(IEEE80211_FEC_BCC), _fec1(IEEE80211_FEC_BCC), _fec2(IEEE80211_FEC_BCC), _fec3(IEEE80211_FEC_BCC),
    _sp0(false), _sp1(false), _sp2(false), _sp3(false),
    _stbc0(false), _stbc1(false), _stbc2(false), _stbc3(false),
    _wifi_extra_flags(0)
{
}

SetTXRates::~SetTXRates()
{
}

int
SetTXRates::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;

  if (cp_va_kparse(conf, this, errh,
      "RATE0", cpkN, cpByte, &_rate0,
      "RATE1", cpkN, cpByte, &_rate1,
      "RATE2", cpkN, cpByte, &_rate2,
      "RATE3", cpkN, cpByte, &_rate3,

      "TRIES0", cpkN, cpByte, &_tries0,
      "TRIES1", cpkN, cpByte, &_tries1,
      "TRIES2", cpkN, cpByte, &_tries2,
      "TRIES3", cpkN, cpByte, &_tries3,

      "MCS0", cpkN, cpBool, &_mcs0,
      "MCS1", cpkN, cpBool, &_mcs1,
      "MCS2", cpkN, cpBool, &_mcs2,
      "MCS3", cpkN, cpBool, &_mcs3,

      "BW0", cpkN, cpInteger, &_bw0,
      "BW1", cpkN, cpInteger, &_bw1,
      "BW2", cpkN, cpInteger, &_bw2,
      "BW3", cpkN, cpInteger, &_bw3,

      "SGI0", cpkN, cpBool, &_sgi0,
      "SGI1", cpkN, cpBool, &_sgi1,
      "SGI2", cpkN, cpBool, &_sgi2,
      "SGI3", cpkN, cpBool, &_sgi3,

      "GF0", cpkN, cpBool, &_gf0,
      "GF1", cpkN, cpBool, &_gf1,
      "GF2", cpkN, cpBool, &_gf2,
      "GF3", cpkN, cpBool, &_gf3,

      "FEC0", cpkN, cpInteger, &_fec0,
      "FEC1", cpkN, cpInteger, &_fec1,
      "FEC2", cpkN, cpInteger, &_fec2,
      "FEC3", cpkN, cpInteger, &_fec3,

      "SP0", cpkN, cpBool, &_sp0,
      "SP1", cpkN, cpBool, &_sp1,
      "SP2", cpkN, cpBool, &_sp2,
      "SP3", cpkN, cpBool, &_sp3,

      "STBC0", cpkN, cpBool, &_stbc0,
      "STBC1", cpkN, cpBool, &_stbc1,
      "STBC2", cpkN, cpBool, &_stbc2,
      "STBC3", cpkN, cpBool, &_stbc3,

      "DEBUG", 0, cpBool, &_debug,
      cpEnd) < 0)
    return -1;

  if (!_mcs0 && (_rate0 == 0) ) _rate0 = 2; //TODO: check, whether 0 isn't valid. maybe 0 can be used to detect base rate
  if ( _tries0 == 0 ) _tries0 = 1;

  BrnWifi::clear_click_wifi_extra_extention(&_mcs_flags);

  if ( _mcs0 ) {
    _wifi_extra_flags |= WIFI_EXTRA_MCS_RATE0;

    if ( _sgi0 ) BrnWifi::fromMCS( _rate0, _bw0, IEEE80211_GUARD_INTERVAL_SHORT, &_rate0);
    else         BrnWifi::fromMCS( _rate0, _bw0, IEEE80211_GUARD_INTERVAL_LONG, &_rate0);

    BrnWifi::setFEC(&_mcs_flags, 0, _fec0);
    if ( _gf0 ) BrnWifi::setHTMode(&_mcs_flags, 0, IEEE80211_HT_MODE_GREENFIELD);
    if ( _sp0 ) BrnWifi::setPreambleLength(&_mcs_flags, 0, IEEE80211_PREAMBLE_LENGTH_SHORT);
    if ( _stbc0 ) BrnWifi::setSTBC(&_mcs_flags, 0, IEEE80211_STBC_ENABLE);
  }

  if ( _mcs1 ) {
    _wifi_extra_flags |= WIFI_EXTRA_MCS_RATE1;

    if ( _sgi1 ) BrnWifi::fromMCS( _rate1, _bw1, IEEE80211_GUARD_INTERVAL_SHORT, &_rate1);
    else         BrnWifi::fromMCS( _rate1, _bw1, IEEE80211_GUARD_INTERVAL_LONG, &_rate1);

    BrnWifi::setFEC(&_mcs_flags, 1, _fec1);
    if ( _gf1 ) BrnWifi::setHTMode(&_mcs_flags, 1, IEEE80211_HT_MODE_GREENFIELD);
    if ( _sp1 ) BrnWifi::setPreambleLength(&_mcs_flags, 1, IEEE80211_PREAMBLE_LENGTH_SHORT);
    if ( _stbc1 ) BrnWifi::setSTBC(&_mcs_flags, 1, IEEE80211_STBC_ENABLE);
  }

  if ( _mcs2 ) {
    _wifi_extra_flags |= WIFI_EXTRA_MCS_RATE2;

    if ( _sgi2 ) BrnWifi::fromMCS( _rate2, _bw2, IEEE80211_GUARD_INTERVAL_SHORT, &_rate2);
    else         BrnWifi::fromMCS( _rate2, _bw2, IEEE80211_GUARD_INTERVAL_LONG, &_rate2);

    BrnWifi::setFEC(&_mcs_flags, 2, _fec2);
    if ( _gf2 ) BrnWifi::setHTMode(&_mcs_flags, 2, IEEE80211_HT_MODE_GREENFIELD);
    if ( _sp2 ) BrnWifi::setPreambleLength(&_mcs_flags, 2, IEEE80211_PREAMBLE_LENGTH_SHORT);
    if ( _stbc2 ) BrnWifi::setSTBC(&_mcs_flags, 2, IEEE80211_STBC_ENABLE);
  }

  if ( _mcs3 ) {
    _wifi_extra_flags |= WIFI_EXTRA_MCS_RATE3;

    if ( _sgi3 ) BrnWifi::fromMCS( _rate3, _bw3, IEEE80211_GUARD_INTERVAL_SHORT, &_rate3);
    else         BrnWifi::fromMCS( _rate3, _bw3, IEEE80211_GUARD_INTERVAL_LONG, &_rate3);

    BrnWifi::setFEC(&_mcs_flags, 3, _fec3);
    if ( _gf3 ) BrnWifi::setHTMode(&_mcs_flags, 3, IEEE80211_HT_MODE_GREENFIELD);
    if ( _sp3 ) BrnWifi::setPreambleLength(&_mcs_flags, 3, IEEE80211_PREAMBLE_LENGTH_SHORT);
    if ( _stbc3 ) BrnWifi::setSTBC(&_mcs_flags, 3, IEEE80211_STBC_ENABLE);
  }

  return 0;
}

Packet *
SetTXRates::simple_action(Packet *p)
{
  click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
  ceh->magic = WIFI_EXTRA_MAGIC;

  struct brn_click_wifi_extra_extention *wee = BrnWifi::get_brn_click_wifi_extra_extention(p);

  ceh->rate = _rate0;
  ceh->rate1 = _rate1;
  ceh->rate2 = _rate2;
  ceh->rate3 = _rate3;

  ceh->max_tries = _tries0;
  ceh->max_tries1 = _tries1;
  ceh->max_tries2 = _tries2;
  ceh->max_tries3 = _tries3;

  ceh->flags |= _wifi_extra_flags;
  memcpy(wee, &_mcs_flags, sizeof(struct brn_click_wifi_extra_extention));

  return p;
}


enum {H_DEBUG};

static String
SetTXRates_read_param(Element *e, void *thunk)
{
  SetTXRates *td = (SetTXRates *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
        return String(td->_debug) + "\n";
      default:
        return String();
    }
}

static int
SetTXRates_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  SetTXRates *f = (SetTXRates *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  }
  return 0;
}

void
SetTXRates::add_handlers()
{
  add_read_handler("debug", SetTXRates_read_param, (void *) H_DEBUG);
  add_write_handler("debug", SetTXRates_write_param, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetTXRates)
