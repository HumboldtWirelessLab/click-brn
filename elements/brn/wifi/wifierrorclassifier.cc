#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include "wifierrorclassifier.hh"
#include "elements/brn/wifi/brnwifi.hh"

#include "elements/brn/standard/brnlogger/brnlogger.hh"

CLICK_DECLS

WifiErrorClassifier::WifiErrorClassifier()
  : _p_all(0),
    _p_ok(0),
    _p_crc(0),
    _p_phy(0),
    _p_fifo(0),
    _p_decrypt(0),
    _p_mic(0),
    _p_zerorate(0),
    _p_unknown(0)
{
  BRNElement::init();
}

WifiErrorClassifier::~WifiErrorClassifier()
{
}

Packet *
WifiErrorClassifier::simple_action(Packet *p)
{
  struct click_wifi_extra *ceha = WIFI_EXTRA_ANNO(p);

  _p_all++;

  if ( (ceha->magic == WIFI_EXTRA_MAGIC && ceha->flags & WIFI_EXTRA_RX_ERR) )
  {

    if ( ceha->flags & WIFI_EXTRA_RX_ZERORATE_ERR )
    {
      _p_zerorate++;
      if ( noutputs() > 6 )
        output(6).push(p);
      else
        p->kill();

      return 0;
    }

    if ( ceha->flags & WIFI_EXTRA_RX_CRC_ERR )
    {
      _p_crc++;
      if ( noutputs() > 1 )
        output(1).push(p);
      else
        p->kill();

      return 0;
    }

    if ( ceha->flags & WIFI_EXTRA_RX_PHY_ERR )
    {
      _p_phy++;
      if ( noutputs() > 2 )
        output(2).push(p);
      else
        p->kill();

      return 0;
    }

    if ( ceha->flags & WIFI_EXTRA_RX_FIFO_ERR )
    {
      _p_fifo++;
      if ( noutputs() > 3 )
        output(3).push(p);
      else
        p->kill();

      return 0;
    }

    if ( ceha->flags & WIFI_EXTRA_RX_DECRYPT_ERR )
    {
      _p_decrypt++;
      if ( noutputs() > 4 )
        output(4).push(p);
      else
        p->kill();

      return 0;
    }

    if ( ceha->flags & WIFI_EXTRA_RX_MIC_ERR )
    {
      _p_mic++;
      if ( noutputs() > 5 )
        output(5).push(p);
      else
        p->kill();

      return 0;
    }

    _p_unknown++;
    if ( noutputs() > 7 )
      output(7).push(p);
    else {
      BRN_DEBUG("Discard unknown error");
      p->kill();
    }

    return 0;
  }

  _p_ok++;

  return p;

}

String
WifiErrorClassifier::print_stats()
{
  StringAccum sa;

  sa << "<wifierrorclassifier packets=\"" << _p_all << "\" ok=\"" << _p_ok << "\" crc=\"" << _p_crc;
  sa << "\" phy=\"" << _p_phy << "\" fifo=\"" << _p_fifo << "\" decrypt=\"" << _p_decrypt;
  sa << "\" mic=\"" << _p_mic << "\" zerorate=\"" << _p_zerorate << "\" unknown=\"" << _p_unknown << "\" />\n";

  return sa.take_string();
}

void
WifiErrorClassifier::reset()
{
  _p_all = _p_ok = _p_crc = _p_phy = _p_fifo = _p_decrypt = _p_mic = _p_zerorate = _p_unknown = 0;
}

enum { H_OK,
       H_PHY,
       H_CRC,
       H_STATS,
       H_RESET };

static int
WifiErrorClassifier_write_param(const String &/*in_s*/, Element *e, void *vparam, ErrorHandler */*errh*/)
{
  WifiErrorClassifier *f = (WifiErrorClassifier *)e;
  switch((intptr_t)vparam) {
    case H_RESET: {    //reset
      f->reset();
    }
  }
  return 0;
}

static String
WifiErrorClassifier_read_param(Element *e, void *thunk)
{
  WifiErrorClassifier *td = (WifiErrorClassifier *)e;
  switch ((uintptr_t) thunk) {
  case H_OK: 
    return String(td->_p_ok) + "\n";
  case H_CRC:
    return String(td->_p_crc) + "\n";
  case H_PHY:
    return String(td->_p_phy) + "\n";
  case H_STATS:
    return td->print_stats();
  default:
    return String();
  }
}


void
WifiErrorClassifier::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("ok", WifiErrorClassifier_read_param, (void *) H_OK);
  add_read_handler("crc", WifiErrorClassifier_read_param, (void *) H_CRC);
  add_read_handler("phy", WifiErrorClassifier_read_param, (void *) H_PHY);

  add_read_handler("stats", WifiErrorClassifier_read_param, (void *) H_STATS);

  add_write_handler("reset", WifiErrorClassifier_write_param, (void *) H_RESET, Handler::BUTTON);

}

CLICK_ENDDECLS


EXPORT_ELEMENT(WifiErrorClassifier)
