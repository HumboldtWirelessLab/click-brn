#ifndef PACKETCOMPRESSIONELEMENT_HH
#define PACKETCOMPRESSIONELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>
#include "lzw.hh"


CLICK_DECLS

class PacketCompression : public Element {

 public:
  //
  //methods
  //
  PacketCompression();
  ~PacketCompression();

  const char *class_name() const  { return "PacketCompression"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "2/2"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

  int _debug;

 private:
  LZW _lzw;

  void compression_test();

};

CLICK_ENDDECLS
#endif
