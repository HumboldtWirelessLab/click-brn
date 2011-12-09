#ifndef PACKETCOMPRESSIONELEMENT_HH
#define PACKETCOMPRESSIONELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>
#include "lzw.hh"

#include "elements/brn2/brnelement.hh"

/**
  TODO: - result of push and put (packet) in push-function is not considered. Change this !!!
        - invalid packet cause errors while decode. Fix it.
*/

CLICK_DECLS

#define DECOMPRESSION_ERROR -1
#define COMPRESSION_ERROR -1

#define COMP_OUTPORT         0
#define DECOMP_OUTPORT       0
#define COMP_ERROR_OUTPORT   1
#define DECOMP_ERROR_OUTPORT 1

#define COMP_INPORT 0
#define DECOMP_INPORT 0

#define MAX_COMPRESSION_BUFFER 3000
#define COMPRESSION_ETHERTYPE 0xc0eb

CLICK_SIZE_PACKED_STRUCTURE(
  struct compression_header {,
  uint8_t marker;
#define COMPRESSION_MARKER 0x9F

  uint8_t compression_mode:3;
#define COMPRESSION_MODE_FULL      0
#define COMPRESSION_MODE_ETHERNET  1
#define COMPRESSION_MODE_BRN       2

  uint8_t compression_type:5;
#define COMPRESSION_NONE  0
#define COMPRESSION_LZW   1
#define COMPRESSION_STRIP 2

  uint16_t uncompressed_len;
});

CLICK_SIZE_PACKED_STRUCTURE(
  struct compression_option {,
  uint16_t offset;
});

class PacketCompression : public BRNElement
{

 public:
  //
  //methods
  //
  PacketCompression();
  ~PacketCompression();

  const char *class_name() const  { return "PacketCompression"; }
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
  uint32_t _compression;
  uint32_t _strip_len;
  unsigned char compbuf[MAX_COMPRESSION_BUFFER];
  uint16_t ethertype;
};

CLICK_ENDDECLS
#endif
