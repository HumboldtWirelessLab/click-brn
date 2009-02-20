#include <click/config.h>
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "brn2_setchannel.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"

CLICK_DECLS

BRN2SetChannel::BRN2SetChannel() :
    _rotate( false ),
    _channel( 0 )
{
}

int
BRN2SetChannel::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "CHANNEL", cpkN, cpInteger, &_channel,
      cpEnd) < 0)
    return -1;

  return 0;
}

Packet *
BRN2SetChannel::simple_action(Packet *p_in)
{
  uint8_t operation;
  uint8_t set_channel;

  if ( p_in != NULL )
  {
    operation = BRNPacketAnno::operation_anno(p_in);
    set_channel = BRNPacketAnno::channel_anno(p_in);

    if ( ( set_channel == 0 ) && ( _channel != 0 ) )
      BRNPacketAnno::set_channel_anno(p_in, _channel, OPERATION_SET_CHANNEL_BEFORE_PACKET);
    else
      if ( set_channel != 0 )
        BRNPacketAnno::set_channel_anno(p_in, set_channel, OPERATION_SET_CHANNEL_BEFORE_PACKET);
      else
        BRNPacketAnno::set_channel_anno(p_in, _channel, OPERATION_SET_CHANNEL_BEFORE_PACKET);
  }

  return p_in;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2SetChannel)
