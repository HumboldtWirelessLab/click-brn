#include <click/config.h>
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "brn2_setchannel.hh"
#include "elements/brn/standard/brnpacketanno.hh"

CLICK_DECLS

BRN2SetChannel::BRN2SetChannel() :
    _rotate( false ),
    _channel( 1 )
{
}

int
BRN2SetChannel::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh, 
      cpEnd) < 0)
    return -1;

  return 0;
}

Packet *
BRN2SetChannel::simple_action(Packet *p_in)
{
  uint8_t operation;

  if ( p_in != NULL )
  {
    operation = BRNPacketAnno::operation_anno(p_in);
    BRNPacketAnno::set_channel_anno(p_in, _channel, OPERATION_SET_CHANNEL_BEFORE_PACKET);
  }

  return p_in;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2SetChannel)
