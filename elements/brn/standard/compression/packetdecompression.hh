#ifndef PACKETDECOMPRESSIONELEMENT_HH
#define PACKETDECOMPRESSIONELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>
#include "lzw.hh"

#include "elements/brn2/brnelement.hh"
#include "packetcompression.hh"

/**
  TODO: - result of push and put (packet) in push-function is not considered. Change this !!!
        - invalid packet cause errors while decode. Fix it.
*/

CLICK_DECLS

class PacketDecompression : public BRNElement
{
 public:
  //
  //methods
  //
  PacketDecompression();
  ~PacketDecompression();

  const char *class_name() const  { return "PacketDecompression"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "1/2"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

 private:
  LZW lzw;

  void compression_test();

  uint8_t cmode;
  unsigned char compbuf[MAX_COMPRESSION_BUFFER];
  uint16_t ethertype;
};

CLICK_ENDDECLS
#endif
