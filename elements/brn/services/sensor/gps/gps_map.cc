#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/etheraddress.hh>
#include <click/straccum.hh>
#include "gps_map.hh"

CLICK_DECLS

GPSMap::GPSMap():
  _timeout(GPSMAP_DEFAULT_TIMEOUT),
  _timeout_timer(this)
{
  BRNElement::init();
}

GPSMap::~GPSMap()
{
}

int
GPSMap::configure( Vector<String> &conf, ErrorHandler *errh )
{
  if (cp_va_kparse(conf, this, errh,
      "TIMEOUT", cpkP, cpInteger, &_timeout,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0 ) {
      return -1;
  }
  return 0;
}

void *
GPSMap::cast(const char *n)
{
  if (strcmp(n, "GPSMap") == 0)
    return (GPSMap *)this;
  return 0;
}

int
GPSMap::initialize(ErrorHandler *)
{
  _timeout_timer.initialize(this);
  _timeout_timer.schedule_after_msec(_timeout);

  return 0;
}

void
GPSMap::run_timer(Timer*)
{
  _timeout_timer.schedule_after_msec(_timeout);

  Timestamp now = Timestamp::now();

  for (TimestampMapIter iter = _time_map.begin(); iter.live(); iter++) {
    BRN_ERROR("remove due to timeout: %s",iter.key().unparse().c_str());
    if ( (now-iter.value()).msecval() > _timeout ) remove(iter.key());
  }
}

GPSPosition *
GPSMap::lookup(EtherAddress eth)
{
  return _map.findp(eth);
}

void
GPSMap::remove(EtherAddress eth)
{
  _map.erase(eth);
  _time_map.erase(eth);
}

void
GPSMap::insert(EtherAddress eth, GPSPosition gps) {
  _map.insert(eth, gps);
  _time_map.insert(eth,Timestamp::now());
}

enum {H_MAP, H_INSERT};

String
GPSMap::read_handler(Element *e, void *thunk)
{
  GPSMap *gpsmap = (GPSMap *)e;
  switch ((uintptr_t) thunk) {
    case H_MAP: {
      StringAccum sa;
      sa << "<gps_map id=\"" << gpsmap->get_node_name() << "\" count=\"" << gpsmap->_map.size() << "\" time=\"" << Timestamp::now().unparse() << "\" >\n";
      for (EtherGPSMapIter iter = gpsmap->_map.begin(); iter.live(); iter++) {
        GPSPosition gps = iter.value();
        EtherAddress ea = iter.key();
        Timestamp ts = gpsmap->_time_map.find(ea);
#ifdef CLICK_NS
        sa << "\t<node mac=\"" << ea.unparse() << "\" lat=\"" << gps._x;
        sa << "\" long=\"" << gps._y << "\" alt=\"" << gps._z;
#else
        sa << "\t<node mac=\"" << ea.unparse() << "\" lat=\"" << gps._latitude.unparse();
        sa << "\" long=\"" << gps._longitude.unparse() << "\" alt=\"" << gps._altitude.unparse();
#endif
        sa << "\" speed=\"" << gps._speed.unparse() << "\" time=\"" << ts.unparse() << "\" />\n";
      }
      sa << "</gps_map>\n";
      return sa.take_string();
  }
  default:
    return String();
  }
}
static int
insert_node(const String &in_s, Element *e, void */*thunk*/, ErrorHandler */*errh*/)
{
  GPSMap *gpsmap = (GPSMap *)e;

  GPSPosition pos;
  EtherAddress ea;

  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  cp_ethernet_address(args[0], &ea);

  if ( args.size() > 3 ) {
    pos.setGPS(args[1], args[2], args[3]);
  } else {
    pos.setGPS(args[1],args[2], "0.0");
  }

  gpsmap->insert(ea,pos);

  return 0;
}

void
GPSMap::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("map", read_handler, (void *) H_MAP);
  add_write_handler("insert", insert_node, H_INSERT);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(GPSMap)

