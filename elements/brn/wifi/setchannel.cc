/* Copyright (C) 2005 BerlinRoofNet Lab
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA. 
 *
 * For additional licensing options, consult http://www.BerlinRoofNet.de 
 * or contact brn@informatik.hu-berlin.de. 
 */

#include <click/config.h>
#include <click/glue.hh>
#include "setchannel.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>


# include <click/cxxprotect.h>
CLICK_CXX_PROTECT

#include "linux/wireless.h"
#include <net/iw_handler.h>

CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>

////////////////////////////////////////////////////////////////////////

static double
iw_freq2float(iw_freq *	in)
{

  //TODO: Does not work with kernel click and MIPS: undefined symbols
  return 0;

  /* Version without libm : slower */
/*
  int		i;
  double	res = (double) in->m;
  for(i = 0; i < in->e; i++)
    res *= 10;
  return(res);
*/
}

////////////////////////////////////////////////////////////////////////

static int 
freq2channel(int freq)
{
  int channel = -1;
  
  switch(freq)
  {
    case 241200000:
      channel = 1;
      break;
    case 241700000:
      channel = 2;
      break;
    case 242200000:
      channel = 3;
      break;
    case 242700000:
      channel = 4;
      break;
    case 243200000:
      channel = 5;
      break;
    case 243700000:
      channel = 6;
      break;
    case 244200000:
      channel = 7;
      break;
    case 244700000:
      channel = 8;
      break;
    case 245200000:
      channel = 9;
      break;
    case 245700000:
      channel = 10;
      break;
    case 246200000:
      channel = 11;
      break;
    case 246700000:
      channel = 12;
      break;
    case 247200000:
      channel = 13;
      break;
    case 248400000:
      channel = 14;
      break;
    default:
      channel = -1;
      break;
  }
  
  return( channel );
}

static int 
channel2freq(int channel)
{
  int frq[] = { -1, 241200000, 241700000, 242200000, 242700000, 
    243200000, 243700000, 244200000, 244700000, 245200000, 245700000,
    246200000, 246700000, 247200000, 248400000 };
  
  if( channel > 14 )
    return( -1 );
  
  return( frq[channel] );
}

////////////////////////////////////////////////////////////////////////

SetChannel::SetChannel() :
  _rotate( false ),
  _channel( 1 )
{
}

////////////////////////////////////////////////////////////////////////

int
SetChannel::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh, 
     /* cpOptional,
        cpString, "device name", &_dev_name, 
        cpBool, "rotate channels", &_rotate,
      cpKeywords,*/
      "DEV", cpkP+cpkM, cpString, /*"device name",*/ &_dev_name,
      "ROTATE", cpkP+cpkM, cpBool, /*"rotate channels",*/ &_rotate,
      cpEnd) < 0)
    return -1;

  if( 0 == _dev_name.length() )
    return errh->error("Devince not specified");

  return 0;
}

////////////////////////////////////////////////////////////////////////
Packet*
SetChannel::simple_action(Packet *p)
{
  if( true == _rotate )
  {
    _channel = _channel % 14 + 1;
    set_channel(_channel);
  }
  
  return( p );
}

////////////////////////////////////////////////////////////////////////

int 
SetChannel::get_channel()
{
  int freq = get_freq();
  
  return( freq2channel(freq) );
}

////////////////////////////////////////////////////////////////////////

void 
SetChannel::set_channel(int channel)
{
  int freq = channel2freq(channel);
  
  set_freq(freq);
}

////////////////////////////////////////////////////////////////////////

int
SetChannel::get_freq() {
  struct net_device* dev = NULL;
  int ret = -1;
  double dFreq = .0;
  struct iwreq iwr;

  dev = dev_get_by_name( _dev_name.c_str() );
  if( dev == NULL )
  {
    return( String("Device not found.") );
  }
  if( NULL == dev->wireless_handlers )
  {
    dev_put(dev);
    return( String("no wireless extensions found.") );
  }
  
  iw_handler get_channel = dev->wireless_handlers->standard[5];
  if( NULL == get_channel )
  {
    dev_put(dev);
    return( String("No get_frequency handler found.") );
  }
  
  ret = get_channel( dev, NULL, &(iwr.u), NULL);
  if ( 0 != ret )
  {
    dev_put(dev);
    StringAccum sa;
    sa << "Could not get channel (ret=" << ret << ")";
    return( sa.take_string() );
  }

  dev_put(dev);

  // TODO take care of exponent iwr.u.freq.e
  return( iwr.u.freq.m );
}

////////////////////////////////////////////////////////////////////////

void
SetChannel::set_freq(int freq) {
  struct net_device* dev = NULL;
  int ret = -1;
  double dFreq = .0;
  struct iwreq iwr;

  dev = dev_get_by_name( _dev_name.c_str() );
  if( dev == NULL )
  {
    click_chatter( "Device not found." );
  }
  if( NULL == dev->wireless_handlers )
  {
    dev_put(dev);
    click_chatter( "no wireless extensions found." );
  }
  
  iw_handler set_channel = dev->wireless_handlers->standard[4];
  if( NULL == set_channel )
  {
    dev_put(dev);
    click_chatter( "No set_frequency handler found." );
  }

  iwr.u.freq.m = freq;
  iwr.u.freq.e = 1;
  ret = set_channel( dev, NULL, &(iwr.u), NULL);
  if ( 0 != ret )
  {
    dev_put(dev);
    StringAccum sa;
    sa << "Could not set channel (ret=" << ret << ")";
    click_chatter( sa.take_string().c_str() );
  }

  dev_put(dev);
}

////////////////////////////////////////////////////////////////////////

static String
SetChannel_read_freq(Element *e, void *thunk)
{
  SetChannel *fd = (SetChannel *) e;
  
  if( 1 == (int) thunk )
    return( String(fd->get_channel()) );

  return( String(fd->get_freq()) );
}

////////////////////////////////////////////////////////////////////////


static int 
SetChannel_write_freq(const String &in_s, Element *e, void *vparam,
                      ErrorHandler *errh)
{
  SetChannel *lt = (SetChannel *)e;
  String s = cp_uncomment(in_s);
  
  int param;
  if (!cp_integer(s, &param)) 
    return errh->error("freq/channel parameter must be int");
    
  if( 1 == (int) vparam )
    lt->set_channel(param);
  else
    lt->set_freq(param);

  return 0;
}

////////////////////////////////////////////////////////////////////////

void
SetChannel::add_handlers()
{
  add_read_handler("channel", SetChannel_read_freq, (void *) 1);
  add_write_handler("channel", SetChannel_write_freq, (void *) 1);

  add_read_handler("frequency", SetChannel_read_freq, 0);
  add_write_handler("frequency", SetChannel_write_freq, 0);
}

////////////////////////////////////////////////////////////////////////

ELEMENT_REQUIRES(linuxmodule)
EXPORT_ELEMENT(SetChannel)

////////////////////////////////////////////////////////////////////////
