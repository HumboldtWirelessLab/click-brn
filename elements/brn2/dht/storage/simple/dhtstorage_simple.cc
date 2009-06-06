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
#include "elements/brn2/dht/storage/dhtoperation.hh"
#include "elements/brn2/dht/protocol/dhtprotocol.hh"
#include "elements/brn2/dht/storage/db/db.hh"

CLICK_DECLS

DHTStorageSimple::DHTStorageSimple():
  _dht_routing(NULL),
  _debug(0),
  _dht_id(0)
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

static void notify_callback_func(void *e, int status)
{
  DHTStorageSimple *s = (DHTStorageSimple *)e;

  click_chatter("callback %s: Status %d",s->class_name(),status);

  switch ( status )
  {
    case ROUTING_STATUS_UPDATE:
    {
      click_chatter("New node");
      s->handle_node_update();
      break;
    }
    default:
    {
      click_chatter("Unknown Status from routing layer");
    }
  }
}

int DHTStorageSimple::initialize(ErrorHandler *)
{
  _dht_routing->set_notify_callback(notify_callback_func,(void*)this);
  return 0;
}

uint32_t
DHTStorageSimple::dht_request(DHTOperation *op, void (*info_func)(void*,DHTOperation*), void *info_obj )
{
  DHTnode *next;
  DHTOperationForward *fwd_op;
  WritablePacket *p;
  uint32_t dht_id = 0;

  MD5::calculate_md5((char*)op->key, op->header.keylen, op->header.key_digest);
  next = _dht_routing->get_responsibly_node(op->header.key_digest);

  if ( next != NULL )
  {
    if ( _dht_routing->is_me(next) )
    {
      handle_dht_operation(op);
      op->set_reply();
    }
    else
    {
      if ( info_func == NULL ) {
        click_chatter("Request for local, but responsible is foreign node !");
        op->set_status(DHT_STATUS_KEY_NOT_FOUND);
        op->set_reply();
      } else {
        dht_id = get_next_dht_id();
        memcpy(op->header.etheraddress, _dht_routing->_me->_ether_addr.data(), 6);   //Set my etheradress as sender
        op->set_id(dht_id);

        fwd_op = new DHTOperationForward(op,info_func,info_obj);
        _fwd_queue.push_back(fwd_op);
        p = DHTProtocol::new_dht_packet(STORAGE_SIMPLE, DHT_MESSAGE, op->length());
        op->serialize_buffer(DHTProtocol::get_payload(p),op->length());
        p = DHTProtocol::push_brn_ether_header(p,&(_dht_routing->_me->_ether_addr), &(next->_ether_addr), BRN_PORT_DHTSTORAGE);
        output(0).push(p);
      }
    }
  }
  else
  {
    click_chatter("Found no node");
    op->set_status(DHT_STATUS_KEY_NOT_FOUND);
    op->set_reply();
  }

  return dht_id;
}

void DHTStorageSimple::push( int port, Packet *packet )
{
  DHTnode *next;
  DHTOperation *_op = NULL;
  WritablePacket *p;
  DHTOperationForward *fwd;

  if ( _dht_routing != NULL )   //use dht-routing, ask routing for next node
  {
    if ( port == 0 )
    {
      _op = new DHTOperation();
      _op->unserialize(DHTProtocol::get_payload(packet),DHTProtocol::get_payload_len(packet));
      if ( _op->is_reply() )
      {
//        click_chatter("got reply dhtop %s",_dht_routing->_me->_ether_addr.unparse().c_str());

        bool found_fwd = false;
        for( int i = 0; i < _fwd_queue.size(); i++ )
        {
          fwd = _fwd_queue[i];
          if ( MD5::hexcompare(fwd->_operation->header.key_digest, _op->header.key_digest) == 0 )  //TODO: better compare dht-id
          {
            fwd->_info_func(fwd->_info_obj,_op);
            _fwd_queue.erase(_fwd_queue.begin() + i);
            delete fwd->_operation;
            delete fwd;

            found_fwd = true;

            break;
          }
        }

        if ( ! found_fwd ) delete _op; 
      }
      else
      {
        next = _dht_routing->get_responsibly_node(_op->header.key_digest);
        if ( _dht_routing->is_me(next) )
        {
//          click_chatter("reply dhtop %s",_dht_routing->_me->_ether_addr.unparse().c_str());
          handle_dht_operation(_op);
          _op->set_reply();
          p = DHTProtocol::new_dht_packet(STORAGE_SIMPLE, DHT_MESSAGE, _op->length());
          _op->serialize_buffer(DHTProtocol::get_payload(p),_op->length());
          EtherAddress src = EtherAddress(_op->header.etheraddress);
//          click_chatter("source is: %s",src.unparse().c_str());
          p = DHTProtocol::push_brn_ether_header(p,&(_dht_routing->_me->_ether_addr), &src, BRN_PORT_DHTSTORAGE);
          delete _op;
          output(0).push(p);
        }
        else
        {
//          click_chatter("Forward dhtop %s",_dht_routing->_me->_ether_addr.unparse().c_str());
          p = packet->uniqueify();
          p = DHTProtocol::push_brn_ether_header(p,&(_dht_routing->_me->_ether_addr), &(next->_ether_addr), BRN_PORT_DHTSTORAGE);
          delete _op;
          output(0).push(p);
          return;  //Reuse packet so return here to prevend packet->kill() //TODO: reorder all
        }
      }
    }

    if ( port == 1 )
    {
      //dht takeout (packet is not for me, but i will take a look)
    }

  } else {
    click_chatter("Error: DHTStorageSimple: Got Packet, but have no routing. Discard Packet");
  }

  packet->kill();

}

void
DHTStorageSimple::handle_dht_operation(DHTOperation *op)
{
  int result;

  if ( ( op->header.operation & OPERATION_INSERT ) == OPERATION_INSERT )
  {
    if ( ( op->header.operation & OPERATION_WRITE ) == OPERATION_WRITE )
    {
      result = dht_read(op);
      if ( op->header.status == DHT_STATUS_KEY_NOT_FOUND ) {
        result = dht_insert(op);
      } else {
        result = dht_write(op);
      }
    } else {
      result = dht_insert(op);
    }
  }

  if ( ( op->header.operation & OPERATION_LOCK ) == OPERATION_LOCK )
  {
    result = dht_lock(op);
  }

  if ( ( op->header.operation & OPERATION_INSERT ) != OPERATION_INSERT )
  {
    if ( ( op->header.operation & OPERATION_WRITE ) == OPERATION_WRITE )
    {
      if ( ( op->header.operation & OPERATION_READ ) == OPERATION_READ )
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
      if ( ( op->header.operation & OPERATION_READ ) == OPERATION_READ )
      {
        if ( ( op->header.operation & OPERATION_REMOVE ) == OPERATION_REMOVE )
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
        if ( ( op->header.operation & OPERATION_REMOVE ) == OPERATION_REMOVE )
        {
          result = dht_remove(op);
          return;
        }
      }
    }
  }

  if ( ( op->header.operation & OPERATION_UNLOCK ) == OPERATION_UNLOCK)
  {
    result = dht_unlock(op);
  }

}

int
DHTStorageSimple::dht_insert(DHTOperation *op)
{
  if ( _db.getRow(op->header.key_digest) == NULL )
  {
    _db.insert(op->header.key_digest, op->key, op->header.keylen, op->value, op->header.valuelen);
    op->header.status = DHT_STATUS_OK;
  }
  else
  {
    //TODO: Handle this in a proper way (error code,...)
    click_chatter("Key already exists");
  }

  return 0;
}

int
DHTStorageSimple::dht_write(DHTOperation *op)
{
  BRNDB::DBrow *_row;

  _row = _db.getRow(op->header.key_digest);
  if ( _row != NULL )
  {
    if ( _row->value != NULL ) delete[] _row->value;
    _row->value = new uint8_t[op->header.valuelen];
    memcpy(_row->value,op->value,op->header.valuelen);
    _row->valuelen = op->header.valuelen;
    op->header.status = DHT_STATUS_OK;
  }
  else
  {
    op->header.status = DHT_STATUS_KEY_NOT_FOUND;
  }

  op->set_reply();

  return 0;
}

int
DHTStorageSimple::dht_read(DHTOperation *op)
{
  BRNDB::DBrow *_row;

  _row = _db.getRow(op->header.key_digest);
  if ( _row != NULL )
  {
    op->set_value(_row->value, _row->valuelen);
    op->header.status = DHT_STATUS_OK;
  }
  else
  {
    op->header.status = DHT_STATUS_KEY_NOT_FOUND;
  }

  op->set_reply();

  return 0;
}

int
DHTStorageSimple::dht_remove(DHTOperation *op)
{
  BRNDB::DBrow *_row;
  EtherAddress ea = EtherAddress(op->header.etheraddress);

  _row = _db.getRow(op->header.key_digest);
  if ( _row != NULL ) {
    if ( _row->unlock(&ea) ) {
      _db.delRow(op->header.key_digest);  //TODO: Errorhandling
      op->header.status = DHT_STATUS_OK;
    } else {
      op->header.status = DHT_STATUS_KEY_IS_LOCKED;
    }
  } else {
    op->header.status = DHT_STATUS_OK;
  }

  op->set_reply();

  return 0;
}

int
DHTStorageSimple::dht_lock(DHTOperation *op)
{
  BRNDB::DBrow *_row;
  EtherAddress ea = EtherAddress(op->header.etheraddress);

  _row = _db.getRow(op->header.key_digest);

  if ( _row != NULL ) {
    if ( _row->lock(&ea, DEFAULT_LOCKTIME) ) {   //lock 1 hour
      op->header.status = DHT_STATUS_OK;
    } else {
      op->header.status = DHT_STATUS_KEY_IS_LOCKED;
    }
  } else {
    op->header.status = DHT_STATUS_KEY_NOT_FOUND;
  }

  op->set_reply();

  return 0;
}

int
DHTStorageSimple::dht_unlock(DHTOperation *op)
{
  BRNDB::DBrow *_row;
  EtherAddress ea = EtherAddress(op->header.etheraddress);

  _row = _db.getRow(op->header.key_digest);

  if ( _row != NULL ) {
    if ( _row->unlock(&ea) ) {   //lock 1 hour
      op->header.status = DHT_STATUS_OK;
    } else {
      op->header.status = DHT_STATUS_KEY_IS_LOCKED;
    }
  } else {
    op->header.status = DHT_STATUS_KEY_NOT_FOUND;
  }

  op->set_reply();

  return 0;
}

uint32_t
DHTStorageSimple::get_next_dht_id()
{
  if ( (_dht_id++) == 0 ) _dht_id = 1;
  return _dht_id;
}

uint32_t
DHTStorageSimple::handle_node_update()
{
  BRNDB::DBrow *_row;
  DHTnode *next;

//  if ( _db.size() == 0 ) click_chatter("No data to move");

  for ( int i = 0; i < _db.size(); i++ ) {
    _row = _db.getRow(i);

    next = _dht_routing->get_responsibly_node(_row->md5_key);
    if ( _dht_routing->is_me(next) ) {
      click_chatter("move");
    } else {
      click_chatter("Don't move");
    }
  }

  return 0;
}

/**************************************************************************/
/************************** H A N D L E R *********************************/
/**************************************************************************/

enum {
  H_DB_SIZE
};

static String
read_param(Element *e, void *thunk)
{
  DHTStorageSimple *dhtstorage_simple = (DHTStorageSimple *)e;
  StringAccum sa;

  switch ((uintptr_t) thunk)
  {
    case H_DB_SIZE :
                {
                  sa << dhtstorage_simple->_dht_routing->_me->_ether_addr.unparse() << "\n";
                  sa << dhtstorage_simple->_db.size();
                  return ( sa.take_string() );
                }
    default: return String();
  }
}

void
DHTStorageSimple::add_handlers()
{
  add_read_handler("db_size", read_param , (void *)H_DB_SIZE);
}
#include <click/vector.cc>
template class Vector<DHTStorageSimple::DHTOperationForward*>;

CLICK_ENDDECLS
EXPORT_ELEMENT(DHTStorageSimple)