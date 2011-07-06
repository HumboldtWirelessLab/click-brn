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
    _device(NULL),
    _channel(0)
{
  BRNElement::init();
}

int
BRN2SetChannel::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEVICE", cpkP, cpElement, &_device,
      "CHANNEL", cpkN, cpInteger, &_channel,
      "DEBUG", 0, cpInteger, &_debug,
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
#if CLICK_USERLEVEL
  StringAccum cmda;
  if (access("/sbin/iwconfig", X_OK) == 0)
    cmda << "/sbin/iwconfig";
  else if (access("/usr/sbin/iwconfig", X_OK) == 0)
    cmda << "/usr/sbin/iwconfig";
  else
    return 0;

  if ( devname == "" ) {
    if ( _device == NULL ) {
      return 0;
    } else {
      cmda << " " << _device->getDeviceName();
    }
  } else {
    cmda << " " << devname;
  }

  cmda << " channel " << channel;
  String cmd = cmda.take_string();

  BRN_DEBUG("SetChannel command: %s",cmd.c_str());

  String out = shell_command_output_string(cmd, "", errh);
  if (out)
    BRN_ERROR("%s: %s", cmd.c_str(), out.c_str());
#endif

  if (_device) _device->setChannel(channel);

  return 0;
}

/* set channel for packet anno */
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

/* set channel system wide */

static int 
setchannel_write_param(const String &in_s, Element *e, void */*vparam*/, ErrorHandler *errh)
{
  BRN2SetChannel *sc = (BRN2SetChannel *)e;
  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  String dev;
  int channel_index, channel;

  if ( args.size() < 2 ) {
    dev = String("");
    channel_index = 0;
  } else {
    dev = args[0];
    channel_index = 1;
  }

  if (!cp_integer(args[channel_index], &channel))
    return errh->error("channel parameter must be integer");
  sc->set_channel(channel);

  sc->set_channel_iwconfig(dev, channel, errh);

  return 0;
}

/* Get Info */

String
BRN2SetChannel::get_info()
{
  StringAccum sa;
  sa << "<setchannel name=\"" << BRN_NODE_NAME << "\" id=\"" << BRN_NODE_NAME << "\" channel=\"" << get_channel()  << "\" >\n\t<device name=\"";
  if ( _device != NULL ) {
    sa << _device->getDeviceName() << "\" device_addr=\"" << _device->getEtherAddress()->unparse() << "\" />\n";
  } else {
    sa << "n/a\" device_addr=\"n/a\" />\n";
  }

  sa << "</setchannel>\n";
  return sa.take_string();
}

static String 
BRN2SetChannel_channel_read_param(Element *e, void */*thunk*/)
{
  BRN2SetChannel *sc = (BRN2SetChannel *)e;

  return sc->get_info();
}

void
BRN2SetChannel::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("channel", BRN2SetChannel_channel_read_param, (void *)0);
  add_write_handler("channel", channel_write_param, (void *)0);

  add_read_handler("systemchannel", BRN2SetChannel_channel_read_param, (void *)0);
  add_write_handler("systemchannel", setchannel_write_param, (void *)0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2SetChannel)
