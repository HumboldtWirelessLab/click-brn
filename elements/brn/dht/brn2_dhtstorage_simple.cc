#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <click/vector.hh>

#include "brn2_dhtstorage_simple.hh"
#include "brn2_db.hh"
#include "elements/brn/dht/dhtcommunication.hh"

CLICK_DECLS

DHTStorageSimple::DHTStorageSimple():
  _dht_routing(NULL),
  _debug(0)
{
  Vector<String> _col_names;
  Vector<int> _col_types;

  _col_names.push_back("ID");
  _col_names.push_back("KEY");
  _col_names.push_back("VALUE");
  _col_names.push_back("LOCK");

  _col_types.push_back(DB_ARRAY);
  _col_types.push_back(DB_ARRAY);
  _col_types.push_back(DB_ARRAY);
  _col_types.push_back(DB_INT);

  _db = new BRNDB(_col_names,_col_types);
}

DHTStorageSimple::~DHTStorageSimple()
{
  delete _db;
}

int DHTStorageSimple::configure(Vector<String> &conf, ErrorHandler *errh)
{

  if (cp_va_parse(conf, this, errh,
    cpOptional,
    cpKeywords,
    "DHTROUTING", cpElement, "Routinglayer", &_dht_routing,
    "DEBUG", cpInteger, "debug", &_debug,
    cpEnd) < 0)
      return -1;

  if (!_dht_routing || !_dht_routing->cast("DHTRouting"))
  {
    _dht_routing = NULL;
    click_chatter("kein Routing");
  }
  else
  {
    click_chatter("Name: %s",_dht_routing->dhtrouting_name());
  }


  return 0;
}

int DHTStorageSimple::initialize(ErrorHandler *)
{
  return 0;
}

void DHTStorageSimple::push( int port, Packet *packet )
{
  if ( _dht_routing != NULL )   //use dht-routing, ask routing for next node
  {
    if ( port == 0 )
    {
      //dht replay or request
      struct dht_packet_header *dht_header = (struct dht_packet_header *)packet->data();
      if ( ( dht_header->code & DHT_DATA ) != DHT_DATA )  // not data, it is a dht_operation
      {
        EtherAddress *sender_ether = new EtherAddress(dht_header->sender_hwa);

        if ( _dht_routing->is_me(sender_ether) )
        {
          int i;

          if ( ( forward_list[i]._id == dht_header->id ) && ( forward_list[i]._sender == dht_header->prim_sender ) )
          {
            assert( ( i >=  0 ) && ( i < forward_list.size() ) );

            dht_header->receiver = forward_list[i]._sender;

            forward_list[i]._fwd_packet->kill();
            delete (forward_list[i]._ether_add);

            forward_list.erase(forward_list.begin() + i);

            //now call callback with key and value
            //break;
          }
        }
      }
    }

    if ( port == 1 )
    {
      //dht takeout (packet is not for me, but i will take a look)
    }
 
  } else {
    /*TODO: move check to initialize*/
    click_chatter("Error: DHTStorageSimple: Got Packet, but have no routing. Discard Packet");
  }
}

void DHTStorageSimple::dht_read()
{

}

void DHTStorageSimple::dht_write()
{

}

void DHTStorageSimple::dht_delete()
{

}

void DHTStorageSimple::dht_look()
{

}

void DHTStorageSimple::dht_unlook()
{

}

void static handle_routing_changes(void *dht)
{
  DHTStorageSimple *dht_handle = (DHTStorageSimple*)dht;
}

void DHTStorageSimple::add_handlers()
{
}

#include <click/vector.cc>
template class Vector<String>;
template class Vector<int>;
template class Vector<DHTStorageSimple::ForwardInfo>;
CLICK_ENDDECLS
EXPORT_ELEMENT(DHTStorageSimple)
