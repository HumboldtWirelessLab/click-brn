#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>
#include "settxpowerrate.hh"

CLICK_DECLS


SetTXPowerRate::SetTXPowerRate()
{
}

SetTXPowerRate::~SetTXPowerRate()
{
}

int
SetTXPowerRate::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  if (cp_va_kparse(conf, this, errh,
      "RT", cpkM, cpElement, &_rtable,
      "MAXPOWER", cpkM, cpElement, &_maxpower,
      "CHANNELSTATS", cpkP, cpElement, &_cst,
      "DEBUG", 0, cpBool, &_debug,
      cpEnd) < 0)
    return -1;
  return 0;
}

void
SetTXPowerRate::push( int port, Packet *p )
{
  ChannelStats::PacketInfo *nodestat;
  struct click_wifi *wh = (struct click_wifi *) p->data();
  click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);


 /* switch ( port) {
    case 0:                                       //Got packet from upper layer
        {
          nodestat = _stats.getDst(wh->i_addr1);

          if ( nodestat == NULL ) nodestat = _stats.newDst(wh->i_addr1);

          break;
        }
   case 1:                                   //Got packet from device//Got packet from upper layer
      {
        if ( ceh->flags & WIFI_EXTRA_TX ) // TXFEEDBACK
        {
          nodestat = _stats.getDst(wh->i_addr1);   //dst of packet is other node (txfeedback)
        }
        else
        {
          nodestat = _stats.getDst(wh->i_addr2);   //src of packet is other node
        }
        break;
      }
  }*/
}

enum {H_DEBUG};

static String 
SetTXPowerRate_read_param(Element *e, void *thunk)
{
  SetTXPowerRate *td = (SetTXPowerRate *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
        return String(td->_debug) + "\n";
      default:
        return String();
    }
}
static int 
SetTXPowerRate_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  SetTXPowerRate *f = (SetTXPowerRate *)e;
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
SetTXPowerRate::add_handlers()
{
  add_read_handler("debug", SetTXPowerRate_read_param, (void *) H_DEBUG);
  add_write_handler("debug", SetTXPowerRate_write_param, (void *) H_DEBUG);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(SetTXPowerRate)
