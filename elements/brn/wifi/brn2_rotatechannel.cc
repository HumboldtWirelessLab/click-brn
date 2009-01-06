#include <click/config.h>
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "brn2_rotatechannel.hh"
#include "elements/brn/standard/brnpacketanno.hh"

CLICK_DECLS

BRN2RotateChannel::BRN2RotateChannel() :
    _rotate( false ),
    _channel( 1 )
{
}

int
BRN2RotateChannel::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh, 
        "ROTATE", cpkP+cpkM, cpBool, /*"rotate channels",*/ &_rotate,
      cpEnd) < 0)
    return -1;

  return 0;
}

Packet *
BRN2RotateChannel::simple_action(Packet *p_in)
{
  if ( p_in != NULL )
  {
    if( _rotate == true )
      _channel = _channel % 14 + 1;

    BRNPacketAnno::set_channel_anno(p_in, _channel, OPERATION_SET_CHANNEL_BEFORE_PACKET);
  }

  return p_in;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2RotateChannel)
