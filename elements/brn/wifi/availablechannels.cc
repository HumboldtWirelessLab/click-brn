/*
 * AvailableChannels
*/

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "availablechannels.hh"
CLICK_DECLS

AvailableChannels::AvailableChannels()
{
}

AvailableChannels::~AvailableChannels()
{
}

void *
AvailableChannels::cast(const char *n)
{
  if (strcmp(n, "AvailableChannels") == 0)
    return (AvailableChannels *) this;
  else
    return 0;
}

int
AvailableChannels::configure(Vector<String> &conf, ErrorHandler */*errh*/)
{
  int res = 0;
  int r;
  Vector<String> args;

  _debug = false;

  cp_spacevec(conf[0], args);

  for (int x = 0; x < args.size(); x++) {
    cp_integer(args[x], &r);
    _channels.push_back(r);
  }

  return res;
}

int
AvailableChannels::get(int i)
{
  if ( i < _channels.size() ) return _channels[i];
  return -1;
}

void
AvailableChannels::take_state(Element *e, ErrorHandler *)
{
  AvailableChannels *q = (AvailableChannels *)e->cast("AvailableChannels");
  if (!q) return;

}

int
AvailableChannels::insert(int channel)
{
  for (int x = 0; x < _channels.size(); x++) {
    if ( _channels[x] == channel ) return 0;
  }

  _channels.push_back(channel);

  return 0;
}



enum {H_DEBUG, H_INSERT, H_REMOVE, H_CHANNELS};


static String
AvailableChannels_read_param(Element *e, void *thunk)
{
  AvailableChannels *td = (AvailableChannels *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  case H_CHANNELS: {
    StringAccum sa;
    sa << "CHANNELS ";

    for (int x = 0; x < td->_channels.size(); x++) {
      sa << " " << td->_channels[x];
    }

    sa << "\n";
    return sa.take_string();
  }
  default:
    return String();
  }
}
static int
AvailableChannels_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  AvailableChannels *f = (AvailableChannels *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {
    bool debug;
    if (!cp_bool(s, &debug))
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  case H_INSERT:
    int i;
    if (!cp_integer(s, &i))
      return errh->error("remove parameter must be integer");
    return f->insert(i);
  case H_REMOVE: {
    int i;
    if (!cp_integer(s, &i))
      return errh->error("remove parameter must be integer");
    f->_channels.erase(&i);
    break;
  }

  }
  return 0;
}

void
AvailableChannels::add_handlers()
{
  add_read_handler("debug", AvailableChannels_read_param, (void *) H_DEBUG);
  add_read_handler("channels", AvailableChannels_read_param, (void *) H_CHANNELS);


  add_write_handler("debug", AvailableChannels_write_param, (void *) H_DEBUG);
  add_write_handler("insert", AvailableChannels_write_param, (void *) H_INSERT);
  add_write_handler("remove", AvailableChannels_write_param, (void *) H_REMOVE);


}

CLICK_ENDDECLS
EXPORT_ELEMENT(AvailableChannels)

