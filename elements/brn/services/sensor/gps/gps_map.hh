#ifndef GPSMAP_HH
#define GPSMAP_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/hashmap.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/standard/vector/vector3d.hh"

#include "gps_position.hh"

CLICK_DECLS

class GPSMap : public BRNElement {

 public:
  GPSMap();
  ~GPSMap();

  const char* class_name() const { return "GPSMap"; }
  void* cast(const char*);
  int configure( Vector<String> &conf, ErrorHandler *errh );

  GPSPosition* lookup(EtherAddress eth);

  void remove(EtherAddress eth);
  void insert(EtherAddress eth, GPSPosition gps);

  typedef HashMap<EtherAddress, GPSPosition> EtherGPSMap;
  typedef EtherGPSMap::const_iterator EtherGPSMapIter;

  EtherGPSMap _map;

  typedef HashMap<EtherAddress, Vector3D> EtherSpeedMap;
  typedef EtherSpeedMap::const_iterator EtherSpeedMapIter;

  EtherSpeedMap _speed_map;

  static String read_handler(Element *e, void *thunk);
  void add_handlers();

};

CLICK_ENDDECLS
#endif /* LEASETABLE_HH */
