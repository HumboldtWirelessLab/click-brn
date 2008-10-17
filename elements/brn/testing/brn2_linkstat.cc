#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <clicknet/udp.h>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include "../brn2.h"
#include "brn2_linkstat.hh"

CLICK_DECLS


BRN2Linkstat::BRN2Linkstat()
  : _timer(this)
{
}

BRN2Linkstat::~BRN2Linkstat()
{

}

int
BRN2Linkstat::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_parse(conf, this, errh,
      cpOptional,
      cpEtherAddress, "etheraddress", &_me,
      cpInteger, "interval", &_interval,
      cpInteger, "interval_size" , &_interval_size,
      cpInteger, "size",  &_size,
      cpEnd) < 0)
        return -1;
 
  return 0;
}

int
BRN2Linkstat::initialize(ErrorHandler *)
{
  _timer.initialize(this);
  _timer.schedule_after_msec(_interval + ( random() % _interval ) );

  return 0;
}

void
BRN2Linkstat::run_timer(Timer *t)
{
  Packet *packet_out;

  _timer.reschedule_after_msec(_interval + ( random() % _interval ) );

  packet_out = create_linkprobe(_size);
  _sequencenumber++;

  output(0).push(packet_out);
}

void
BRN2Linkstat::push( int port, Packet *packet )
{
  struct linkprobe new_linkprobe;
  int j;

  memcpy( &new_linkprobe, packet->data(), sizeof(struct linkprobe));

  if ( port == 0 )
  {
    for ( j = 0; j < neighbors_ls_info.size(); j++ )
      if ( memcmp( &(new_linkprobe.sendernode), neighbors_ls_info[j]._node_address.data(), 6 ) == 0 ) break;

    if ( j == neighbors_ls_info.size() )   // neighbor is new
    {
      neighbors_ls_info.push_back( BRN2_LinkstatInfo( EtherAddress(new_linkprobe.sendernode), new_linkprobe.id , _interval_size ) );
    }
    else
    {
      neighbors_ls_info[j].update_linkstatInfo(new_linkprobe.id);
    }

  }

  packet->kill();

}

Packet *
BRN2Linkstat::create_linkprobe(int size)
{
  struct linkprobe new_linkprobe;
  uint32_t space_left = size;

  WritablePacket *new_packet = WritablePacket::make(size);

  memset(new_packet->data(),0,size);

  if ( space_left >= sizeof(struct linkprobe) )
  {
    memcpy( &new_linkprobe.sendernode, _me.data(), 6 ) ;
    new_linkprobe.id = _sequencenumber;
    new_linkprobe.count_neighbors = neighbors_ls_info.size();

    memcpy( new_packet->data(),(void*)&new_linkprobe,sizeof(struct linkprobe) );

    space_left -= sizeof(struct linkprobe);
  }

  return(new_packet);
}

static String
read_handler(Element *, void *)
{
  return "false\n";
}

static String
read_neighbors(Element *e, void *thunk)
{
  BRN2Linkstat *lt = (BRN2Linkstat *)e;
  StringAccum sa;
   uint32_t last_sn;

  sa << "Neighbors of " << lt->_me.unparse() << ":\n";
  sa << "\tAddress\t\t\tLast Sequencenumber\t# of " << lt->_interval_size << " Probes\n";
  for (int i = 0; i < lt->neighbors_ls_info.size(); i++ )
  {
    last_sn = lt->neighbors_ls_info[i]._linkprobe_squen.size() - 1;
    sa << "\t" << lt->neighbors_ls_info[i]._node_address.unparse() << "\t" << lt->neighbors_ls_info[i]._linkprobe_squen[last_sn]  << "\t\t\t" << ( last_sn + 1 ) << "\n";
  }

  return sa.take_string();
}

void
BRN2Linkstat::add_handlers()
{
  // needed for QuitWatcher
  add_read_handler("neighbors", read_neighbors, 0);
  add_read_handler("scheduled", read_handler, 0);
}

#include <click/vector.cc>
template class Vector<EtherAddress>;
template class Vector<BRN2_LinkstatInfo>;

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2Linkstat)
