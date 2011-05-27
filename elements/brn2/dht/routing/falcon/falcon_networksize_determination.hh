#ifndef FALCON_NETWORK_DETERMINATION_HH
#define FALCON_NETWORK_DETERMINATION_HH
#include <click/element.hh>

#include "elements/brn2/brnelement.hh"

#include "elements/brn2/standard/brn_md5.hh"
#include "elements/brn2/standard/packetsendbuffer.hh"
#include "falcon_routingtable.hh"

CLICK_DECLS

class FalconNetworkSizeDetermination : public BRNElement
{
  public:
    FalconNetworkSizeDetermination();
    ~FalconNetworkSizeDetermination();

/*ELEMENT*/
    const char *class_name() const  { return "FalconNetworkSizeDetermination"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

    void add_handlers();

    String networksize();
    void request_nws();

  private:
    FalconRoutingTable *_frt;
    DHTnode *get_responsibly_node_FT(md5_byte_t *key, uint8_t *position);

    void handle_nws(Packet *packet);

    int _networksize;
};

CLICK_ENDDECLS
#endif
