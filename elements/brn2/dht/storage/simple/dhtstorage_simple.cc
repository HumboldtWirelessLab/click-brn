#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <click/vector.hh>

#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/dht/storage/dhtoperation.hh"
#include "elements/brn2/dht/protocol/dhtprotocol.hh"
#include "elements/brn2/dht/storage/db/db.hh"

#include "dhtprotocol_storagesimple.hh"
#include "dhtstorage_simple.hh"

CLICK_DECLS

DHTStorageSimple::DHTStorageSimple():
  _dht_routing(NULL),
  _check_req_queue_timer(req_queue_timer_hook,this),
  _debug(BrnLogger::DEFAULT),
  _dht_id(0),
  _max_req_time(DEFAULT_REQUEST_TIMEOUT),
  _max_req_retries(DEFAULT_MAX_RETRIES)
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

  if (!_dht_routing || !_dht_routing->cast("DHTRouting")) {
    _dht_routing = NULL;
    BRN_WARN("No Routing");
  } else {
    BRN_INFO("Use DHT-Routing: %s",_dht_routing->dhtrouting_name());
  }

  return 0;
}

static void notify_callback_func(void *e, int status)
{
  DHTStorageSimple *s = (DHTStorageSimple *)e;
  s->handle_notify_callback(status);
}

void
DHTStorageSimple::handle_notify_callback(int status)
{
  BRN_DEBUG("DHT-Routing-Callback %s: Status %d",class_name(),status);

  switch ( status )
  {
    case ROUTING_STATUS_UPDATE:
    {
      BRN_INFO("Routing update (new node,...)");
      handle_node_update();
      break;
    }
    default:
    {
      BRN_WARN("Unknown Status from routing layer");
    }
  }
}

int DHTStorageSimple::initialize(ErrorHandler *)
{
  _dht_routing->set_notify_callback(notify_callback_func,(void*)this);
  _check_req_queue_timer.initialize(this);
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
      if ( info_func == NULL ) {                                                     //CALLBACK is NULL so operation has to be handle locally
        BRN_DEBUG("Request for local, but responsible is foreign node !");
        op->set_status(DHT_STATUS_KEY_NOT_FOUND);
        op->set_reply();
      } else {
        dht_id = get_next_dht_id();
        memcpy(op->header.etheraddress, _dht_routing->_me->_ether_addr.data(), 6);   //Set my etheradress as sender
        op->set_id(dht_id);

        fwd_op = new DHTOperationForward(op,info_func,info_obj);
        _fwd_queue.push_back(fwd_op);
        p = DHTProtocol::new_dht_packet(STORAGE_SIMPLE, DHT_STORAGE_SIMPLE_MESSAGE, op->length());
        op->serialize_buffer(DHTProtocol::get_payload(p),op->length());
        p = DHTProtocol::push_brn_ether_header(p,&(_dht_routing->_me->_ether_addr), &(next->_ether_addr), BRN_PORT_DHTSTORAGE);
        output(0).push(p);

        int time2next = get_time_to_next();
        if ( time2next < 10 ) time2next = 10;
        _check_req_queue_timer.schedule_after_msec(time2next);
      }
    }
  }
  else
  {
    BRN_DEBUG("Found no node");
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
            Timestamp now = Timestamp::now();
            _op->max_retries = fwd->_operation->max_retries;
            _op->retries = fwd->_operation->retries;

            _op->request_time = fwd->_operation->request_time;
            _op->max_request_duration = fwd->_operation->max_request_duration;
            _op->request_duration = (now - _op->request_time).msec1();

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
//        click_chatter("reply dhtop %s",_dht_routing->_me->_ether_addr.unparse().c_str());
          int result = handle_dht_operation(_op);

          if ( result == -1 ) {
            delete _op;
          } else {
            _op->set_reply();
            p = DHTProtocol::new_dht_packet(STORAGE_SIMPLE, DHT_STORAGE_SIMPLE_MESSAGE, _op->length());
            _op->serialize_buffer(DHTProtocol::get_payload(p),_op->length());

            EtherAddress src = EtherAddress(_op->header.etheraddress);
//          click_chatter("source is: %s",src.unparse().c_str());
            p = DHTProtocol::push_brn_ether_header(p,&(_dht_routing->_me->_ether_addr), &src, BRN_PORT_DHTSTORAGE);
            delete _op;
            output(0).push(p);
          }
        }
        else
        {
//        click_chatter("Forward dhtop %s",_dht_routing->_me->_ether_addr.unparse().c_str());
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
    BRN_WARN("Error: DHTStorageSimple: Got Packet, but have no routing. Discard Packet");
  }

  packet->kill();

}

int
DHTStorageSimple::handle_dht_operation(DHTOperation *op)
{
  int result;

  //TODO: use switch-case and test for all possible combinations -> more readable ?? possible with lock ??
  if ( ( op->header.operation & OPERATION_INSERT ) == OPERATION_INSERT )
  {
    if ( ( op->header.operation & OPERATION_WRITE ) == OPERATION_WRITE )
    {
      result = dht_read(op);
      if ( op->header.status == DHT_STATUS_KEY_NOT_FOUND ) {
//        click_chatter("insert on overwrite");
        result = dht_insert(op);
      } else {
//        click_chatter("overwrite existing");
        result = dht_write(op);
      }
    } else {
//      click_chatter("insert");
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
//        click_chatter("Read/write");
        result = dht_read(op);
        result = dht_write(op);
      }
      else
      {
//        click_chatter("write");
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
          return result;
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
          return result;
        }
      }
    }
  }

  if ( ( op->header.operation & OPERATION_UNLOCK ) == OPERATION_UNLOCK)
  {
    result = dht_unlock(op);
  }

  return result;
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
    BRN_WARN("Key already exists");
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

  for ( int i = 0; i < _db.size(); i++ ) {
    _row = _db.getRow(i);

    next = _dht_routing->get_responsibly_node(_row->md5_key);
    if ( _dht_routing->is_me(next) ) {
      BRN_DEBUG("Move data to new node.");
    } else {
      BRN_DEBUG("Don't move data to new node.");
    }
  }

  return 0;
}

/**************************************************************************/
/********************** T I M E R - S T U F F *****************************/
/**************************************************************************/
/**
  * To get the time for next, the timediff between time since last request and the max requesttime
 */

int
DHTStorageSimple::get_time_to_next()
{
  DHTOperation *_op = NULL;
  DHTOperationForward *fwd;
  int min_time, ac_time;

  Timestamp now = Timestamp::now();

  if ( _fwd_queue.size() > 0 ) {
    fwd = _fwd_queue[0];
    min_time = _max_req_time - (now - fwd->_last_request_time).msec1();
  } else
    return -1;

  for( int i = 1; i < _fwd_queue.size(); i++ )
  {
    fwd = _fwd_queue[i];
    ac_time = _max_req_time - (now - fwd->_last_request_time).msec1();
    if ( ac_time < min_time ) min_time = ac_time;
  }

  if ( min_time < 0 ) return 0;

  return min_time;
}

bool
DHTStorageSimple::isFinalTimeout(DHTOperationForward *fwdop)
{
//  click_chatter("RETRIES: %d   %d",fwdop->_operation->retries, fwdop->_operation->max_retries);
  return ( fwdop->_operation->retries == fwdop->_operation->max_retries );
}

void
DHTStorageSimple::check_queue()
{
  DHTnode *next;
  DHTOperation *_op = NULL;
  DHTOperationForward *fwd;
  WritablePacket *p;
  int timediff;

  Timestamp now = Timestamp::now();

//  click_chatter("Queue: %d",_fwd_queue.size());
  for( int i = (_fwd_queue.size() - 1); i >= 0; i-- )
  {
    fwd = _fwd_queue[i];

    timediff = (now - fwd->_last_request_time).msec1();

//    click_chatter("Test: Timediff: %d MAX: %d",timediff,_max_req_time);
    if ( timediff >= _max_req_time ) {

      _op = fwd->_operation;

      if ( isFinalTimeout(fwd) ) {
       //Timeout
//        click_chatter("Timeout");
        _op->set_status(DHT_STATUS_TIMEOUT); //DHT_STATUS_MAXRETRY
        _op->set_reply();
        _op->request_duration = (now - _op->request_time).msec1();

        fwd->_info_func(fwd->_info_obj,_op);
        _fwd_queue.erase(_fwd_queue.begin() + i);
        delete fwd;
      } else {
        // Retry
        fwd->_last_request_time = Timestamp::now();
        _op->retries++;

        next = _dht_routing->get_responsibly_node(_op->header.key_digest);           //TODO: handle if next is null
        p = DHTProtocol::new_dht_packet(STORAGE_SIMPLE, DHT_STORAGE_SIMPLE_MESSAGE, _op->length());
        _op->serialize_buffer(DHTProtocol::get_payload(p),_op->length());
        p = DHTProtocol::push_brn_ether_header(p,&(_dht_routing->_me->_ether_addr), &(next->_ether_addr), BRN_PORT_DHTSTORAGE);
        output(0).push(p);
      }
    }
  }

  if ( _fwd_queue.size() > 0 )
    _check_req_queue_timer.schedule_after_msec( get_time_to_next() );
}

void
DHTStorageSimple::req_queue_timer_hook(Timer *, void *f)
{
//  click_chatter("Timer");
  DHTStorageSimple *dhtss = (DHTStorageSimple *)f;
  dhtss->check_queue();
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
