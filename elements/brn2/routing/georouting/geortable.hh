#ifndef GEORTABLEELEMENT_HH
#define GEORTABLEELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>
#include "elements/brn2/services/sensor/gps/gps.hh"
#include "elements/brn2/routing/linkstat/brn2_brnlinktable.hh"

CLICK_DECLS

class GeorTable : public Element {

 public:
  //
  //methods
  //
  GeorTable();
  ~GeorTable();

  const char *class_name() const  { return "GeorTable"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "0/0"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  void updateEntry(EtherAddress *ea, struct gps_position *pos);
  String get_routing_table();

  typedef HashMap<EtherAddress, GPSPosition> GPSRoutingTable;

  GPSPosition *getPosition(EtherAddress *ea);
  GPSPosition *getClosest(struct gps_position *pos, EtherAddress *ea);
  GPSPosition *getClosestNeighbour(struct gps_position *pos, EtherAddress *ea);
  GPSPosition *getClosestNeighbour(GPSPosition *pos, EtherAddress *ea);
  GPSPosition *getLocalPosition();


 private:
   GPSRoutingTable _rt;

 public:

  Brn2LinkTable *_lt;
  GPS *_gps;

  int _debug;
};

CLICK_ENDDECLS
#endif
