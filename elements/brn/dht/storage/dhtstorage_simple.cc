#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <click/vector.hh>

#include "dhtstorage_simple.hh"
#include "elements/brn/dht/storage/dhtoperation.hh"
#include "db.hh"

CLICK_DECLS

DHTStorageSimple::DHTStorageSimple():
  _dht_routing(NULL),
  _debug(0)
{
}

DHTStorageSimple::~DHTStorageSimple()
{
}

void *
DHTStorageSimple::cast(const char *name)
{
  if (strcmp(name, "DHTStorageSimple") == 0)
    return (DHTStorageSimple *) this;
  else if (strcmp(name, "DHTStorage") == 0)
    return (DHTStorage *) this;
  else
    return NULL;
}

int DHTStorageSimple::configure(Vector<String> &conf, ErrorHandler *errh)
{

  if (cp_va_kparse(conf, this, errh,
    "DHTROUTING", cpkN, cpElement, &_dht_routing,
    "DEBUG", cpkN, cpInteger, &_debug,
    cpEnd) < 0)
      return -1;

  if (!_dht_routing || !_dht_routing->cast("DHTRouting"))
  {
    _dht_routing = NULL;
    click_chatter("kein Routing");
  }
/*  else
  {
    click_chatter("Name: %s",_dht_routing->dhtrouting_name());
  }
*/

  return 0;
}

static void callback_func(void *e, int status)
{
  DHTStorageSimple *s = (DHTStorageSimple *)e;

//  click_chatter("callback %s: Status %d",s->class_name(),status);
}

int DHTStorageSimple::initialize(ErrorHandler *)
{
  _dht_routing->set_notify_callback(callback_func,(void*)this);
  return 0;
}

int
DHTStorageSimple::dht_request(DHTOperation *op, void (*info_func)(void*,DHTOperation*), void *info_obj )
{
  DHTnode *next;
  md5_byte_t _md5_digest[16];
  MD5::calculate_md5(op->key, op->keylen, _md5_digest);
  next = _dht_routing->get_responsibly_node(_md5_digest);

  if ( next != NULL )
    click_chatter("Node: %s",next->_ether_addr.unparse().c_str());

  if ( _dht_routing->is_me(next) )
  {
    click_chatter("handle local");
    handle_dht_operation(op);
    op->set_reply();
    info_func(info_obj,op);
  }
  else
  {
    click_chatter("Send");
  }
  return 0;
}

void DHTStorageSimple::push( int port, Packet *packet )
{
  if ( _dht_routing != NULL )   //use dht-routing, ask routing for next node
  {
    if ( port == 0 )
    {
    }

    if ( port == 1 )
    {
      //dht takeout (packet is not for me, but i will take a look)
    }
 
  } else {
    click_chatter("Error: DHTStorageSimple: Got Packet, but have no routing. Discard Packet");
    packet->kill();
  }
}

void
DHTStorageSimple::handle_dht_operation(DHTOperation *op)
{
  int result;

  if ( op->operation & OPERATION_INSERT == OPERATION_INSERT )
  {
    result = dht_insert(op);
  }

  if ( op->operation & OPERATION_LOCK == OPERATION_LOCK )
  {
    result = dht_lock(op);
  }

  if ( op->operation & OPERATION_INSERT != OPERATION_INSERT )
  {
    if ( op->operation & OPERATION_WRITE == OPERATION_WRITE )
    {
      if ( op->operation & OPERATION_READ == OPERATION_READ )
      {
        result = dht_read(op);
        result = dht_write(op);
      }
      else
      {
        result = dht_write(op);
      }
    }
    else
    {
      if ( op->operation & OPERATION_READ == OPERATION_READ )
      {
        if ( op->operation & OPERATION_REMOVE == OPERATION_REMOVE )
        {
          result = dht_read(op);
          result = dht_remove(op);
          return;
        }
        else
        {
          result = dht_read(op);
        }
      }
      else
      {
        if ( op->operation & OPERATION_REMOVE == OPERATION_REMOVE )
        {
          result = dht_remove(op);
          return;
        }
      }
    }
  }

  if ( op->operation & OPERATION_UNLOCK == OPERATION_UNLOCK)
  {
    result = dht_unlock(op);
  }

}

int
DHTStorageSimple::dht_insert(DHTOperation *op)
{
  click_chatter("Insert");
}

int
DHTStorageSimple::dht_write(DHTOperation *op)
{

}

int
DHTStorageSimple::dht_read(DHTOperation *op)
{

}

int
DHTStorageSimple::dht_remove(DHTOperation *op)
{

}

int
DHTStorageSimple::dht_lock(DHTOperation *op)
{

}

int
DHTStorageSimple::dht_unlock(DHTOperation *op)
{

}

void
DHTStorageSimple::add_handlers()
{
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DHTStorageSimple)
