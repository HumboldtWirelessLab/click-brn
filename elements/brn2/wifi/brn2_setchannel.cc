#include <click/config.h>
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/userutils.hh>
#include <unistd.h>

#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "brn2_setchannel.hh"

CLICK_DECLS

BRN2SetChannel::BRN2SetChannel() :
    _channel(0)
{
  BRNElement::init();
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


/**************************************************************************************************************/
/********************************************** H A N D L E R *************************************************/
/**************************************************************************************************************/

int
BRN2SetChannel::set_channel_iwconfig(const String &devname, int channel, ErrorHandler *errh)
{
  StringAccum cmda;
  if (access("/sbin/iwconfig", X_OK) == 0)
    cmda << "/sbin/iwconfig";
  else if (access("/usr/sbin/iwconfig", X_OK) == 0)
    cmda << "/usr/sbin/iwconfig";
  else
    return 0;

  cmda << " " << devname << " channel " << channel;
  String cmd = cmda.take_string();

  String out = shell_command_output_string(cmd, "", errh);
  if (out)
    BRN_ERROR("%s: %s", cmd.c_str(), out.c_str());
  return 0;
}


static String 
channel_read_param(Element *e, void */*thunk*/)
{
  StringAccum sa;
  BRN2SetChannel *sc = (BRN2SetChannel *)e;
  return String(sc->get_channel()) + "\n";
}

static int 
channel_write_param(const String &in_s, Element *e, void */*vparam*/, ErrorHandler *errh)
{
  BRN2SetChannel *sc = (BRN2SetChannel *)e;
  String s = cp_uncomment(in_s);
  int channel;
  if (!cp_integer(s, &channel))
    return errh->error("channel parameter must be integer");
  sc->set_channel(channel);

  return 0;
}

static int 
setchannel_write_param(const String &in_s, Element *e, void */*vparam*/, ErrorHandler *errh)
{
  BRN2SetChannel *sc = (BRN2SetChannel *)e;
  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  String dev = args[0];
  int channel;
  if (!cp_integer(args[1], &channel))
    return errh->error("channel parameter must be integer");
  sc->set_channel(channel);

  sc->set_channel_iwconfig(dev, channel, errh);

  return 0;
}

void
BRN2SetChannel::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("channel", channel_read_param, (void *)0);
  add_write_handler("channel", channel_write_param, (void *)0);
  add_write_handler("set_channel", setchannel_write_param, (void *)0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2SetChannel)
