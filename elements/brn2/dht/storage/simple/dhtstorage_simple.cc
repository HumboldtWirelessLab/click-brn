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
  _max_req_retries(DEFAULT_MAX_RETRIES),
  _add_node_id(false)
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
    "DHTDB",cpkM+cpkP, cpElement, &_dht_db,
    "DHTROUTING", cpkN, cpElement, &_dht_routing,
    "ADDNODEID", cpkN, cpBool, &_add_node_id,
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
  uint32_t dht_id, replica_count;
  int status;

  MD5::calculate_md5((char*)op->key, op->header.keylen, op->header.key_digest);

  //Check whether routing support replica and whether the requested number of replica is support. Correct if something cannot be performed by routing
  if ( _dht_routing->max_replication() < op->header.replica )
    replica_count = _dht_routing->max_replication();
  else
    replica_count = op->header.replica;

  fwd_op = new DHTOperationForward(op, info_func, info_obj, replica_count);

  dht_id = get_next_dht_id();
  op->set_id(dht_id);
  memcpy(op->header.etheraddress, _dht_routing->_me->_ether_addr.data(), 6);   //Set my etheradress as sender

  for ( int r = 0; r <= replica_count; r++ ) {
    next = _dht_routing->get_responsibly_replica_node(op->header.key_digest, r);
    op->header.replica = r;

    if ( next == NULL )
    {
      BRN_DEBUG("No next node!");
      fwd_op->replicaList[r].status = DHT_STATUS_KEY_NOT_FOUND;
      fwd_op->set_replica_reply(r);
    } else {
      if ( _dht_routing->is_me(next) )
      {
        status = _dht_db->handle_dht_operation(op);

        fwd_op->replicaList[r].status = status;
        fwd_op->replicaList[r].set_value(op->value,op->header.valuelen);
        fwd_op->set_replica_reply(r);
        //TODO: check whether all needed replica-reply are received. Wenn nicht alle replicas nötig
        //      sind ( z.b. nur eine von 3 Antworten, dann könnte das soweit sein und man könnte sich den rest sparen.
        // vielleict sollten die replicas in einem solchen fall sortiert werden. Der locale könnte reichen und wenn dieser zuerst kommt,.so keine Pakete
      }
      else
      {
        if ( info_func == NULL ) {                                                     //CALLBACK is NULL so operation has to be handle locally
          BRN_DEBUG("Request for local, but responsible is foreign node !");
          fwd_op->replicaList[r].status = DHT_STATUS_KEY_NOT_FOUND;
          fwd_op->set_replica_reply(r);
        } else {
          if ( _add_node_id ) {
            p = DHTProtocol::new_dht_packet(STORAGE_SIMPLE, DHT_STORAGE_SIMPLE_MESSAGE, op->length() + sizeof(struct dht_simple_storage_node_info));
            op->serialize_buffer(&((DHTProtocol::get_payload(p))[sizeof(struct dht_simple_storage_node_info)]),op->length());
            struct dht_simple_storage_node_info *ni = (struct dht_simple_storage_node_info *)DHTProtocol::get_payload(p);
            _dht_routing->_me->get_nodeid((md5_byte_t *)ni->src_id, &(ni->src_id_size));
            ni->reserved = 0;
          } else {
            p = DHTProtocol::new_dht_packet(STORAGE_SIMPLE, DHT_STORAGE_SIMPLE_MESSAGE, op->length());
            op->serialize_buffer(DHTProtocol::get_payload(p),op->length());
          }
          p = DHTProtocol::push_brn_ether_header(p,&(_dht_routing->_me->_ether_addr), &(next->_ether_addr), BRN_PORT_DHTSTORAGE);
          output(0).push(p);
        }
      }
    }
  }

  if ( fwd_op->have_all_replicas() ) {
    op->header.replica = fwd_op->replica_count;
    op->set_reply();
    return DHT_OPERATION_ID_LOCAL_REPLY;
  } else {
    _fwd_queue.push_back(fwd_op);

    int time2next = get_time_to_next();
    if ( time2next < 10 ) time2next = 10;
    _check_req_queue_timer.schedule_after_msec(time2next);
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

      if ( _add_node_id ) {
        _op->unserialize(&((DHTProtocol::get_payload(packet))[sizeof(struct dht_simple_storage_node_info)]),DHTProtocol::get_payload_len(packet) - sizeof(struct dht_simple_storage_node_info));

        EtherAddress src = EtherAddress(_op->header.etheraddress);
        struct dht_simple_storage_node_info *ni = (struct dht_simple_storage_node_info *)DHTProtocol::get_payload(packet);
        _dht_routing->update_node(&src, ni->src_id, ni->src_id_size);
      } else {
        _op->unserialize(DHTProtocol::get_payload(packet),DHTProtocol::get_payload_len(packet));
      }

      if ( _op->is_reply() )
      {
        BRN_DEBUG("Operation is reply.");

        bool found_fwd = false;
        for( int i = 0; i < _fwd_queue.size(); i++ )
        {
          fwd = _fwd_queue[i];
          if ( MD5::hexcompare(fwd->_operation->header.key_digest, _op->header.key_digest) == 0 )  //TODO: better compare dht-id
          {
            BRN_DEBUG("Found entry for Operation.");

            fwd->replicaList[_op->header.replica].status = _op->header.status;
            fwd->replicaList[_op->header.replica].set_value(_op->value, _op->header.valuelen);

            fwd->set_replica_reply(_op->header.replica);

            if ( fwd->have_all_replicas() ) {
              Timestamp now = Timestamp::now();
              _op->max_retries = fwd->_operation->max_retries;
              _op->retries = fwd->_operation->retries;

              _op->request_time = fwd->_operation->request_time;
              _op->max_request_duration = fwd->_operation->max_request_duration;
              _op->request_duration = (now - _op->request_time).msec1();

              _op->header.replica = fwd->replica_count;

              _op->set_reply();                            //TODO: currently last reply is forwarded to application

              //TODO: sum up the results of all replicas
              fwd->_info_func(fwd->_info_obj,_op);
              _fwd_queue.erase(_fwd_queue.begin() + i);
              delete fwd->_operation;                      //TODO: DOn't delete. Use this instead of last _op ***
              delete fwd;

              found_fwd = true;                            //TODO: use fwd-op instead of last _op ***
            }
            break;
          }
        }

        if ( ! found_fwd ) delete _op; 
      }
      else
      {
        next = _dht_routing->get_responsibly_replica_node(_op->header.key_digest, _op->header.replica);
        if ( _dht_routing->is_me(next) )
        {
          BRN_DEBUG("I'm responsible for Operation.");

          int result = _dht_db->handle_dht_operation(_op);

          if ( result == -1 ) {
            BRN_DEBUG("Operation error");
            delete _op;
          } else {
            _op->set_reply();

            EtherAddress src = EtherAddress(_op->header.etheraddress);                                                             //safe old soure for reply

            if ( _add_node_id ) {
              p = DHTProtocol::new_dht_packet(STORAGE_SIMPLE, DHT_STORAGE_SIMPLE_MESSAGE, _op->length() + sizeof(struct dht_simple_storage_node_info));
              memcpy(_op->header.etheraddress, _dht_routing->_me->_ether_addr.data(), 6);                                          //now i'am the soure of the packet
              _op->serialize_buffer(&((DHTProtocol::get_payload(p))[sizeof(struct dht_simple_storage_node_info)]),_op->length());
              struct dht_simple_storage_node_info *ni = (struct dht_simple_storage_node_info *)DHTProtocol::get_payload(p);
              _dht_routing->_me->get_nodeid((md5_byte_t *)ni->src_id, &(ni->src_id_size));
              ni->reserved = 0;
            } else {
              p = DHTProtocol::new_dht_packet(STORAGE_SIMPLE, DHT_STORAGE_SIMPLE_MESSAGE, _op->length());
              memcpy(_op->header.etheraddress, _dht_routing->_me->_ether_addr.data(), 6);                                          //now i'am the soure of the packet
              _op->serialize_buffer(DHTProtocol::get_payload(p),_op->length());
            }

            p = DHTProtocol::push_brn_ether_header(p,&(_dht_routing->_me->_ether_addr), &src, BRN_PORT_DHTSTORAGE);
            delete _op;
            output(0).push(p);
          }
        }
        else
        {
          BRN_DEBUG("Forward operation.");
          BRN_DEBUG("Me: %s Dst: %s",_dht_routing->_me->_ether_addr.unparse().c_str(),
                                     next->_ether_addr.unparse().c_str());

          p = packet->uniqueify();

          p = DHTProtocol::push_brn_ether_header(p,&(_dht_routing->_me->_ether_addr), &(next->_ether_addr), BRN_PORT_DHTSTORAGE);
          delete _op;
          output(0).push(p);
          return;  //Reuse packet so return here to prevend packet->kill() //TODO: reorder all
        }
      }
    }
  } else {
    BRN_WARN("Error: DHTStorageSimple: Got Packet, but have no routing. Discard Packet");
  }

  packet->kill();

}

uint16_t
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

  for ( int i = 0; i < _dht_db->_db.size(); i++ ) {
    _row = _dht_db->_db.getRow(i);

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

/**
 * Handles Timeout. It checks, which replica didn't answer. Only this, has to request again.
 */

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

//  click_chatter("Test: Timediff: %d MAX: %d",timediff,_max_req_time);
    if ( timediff >= _max_req_time ) {

      _op = fwd->_operation;

      if ( isFinalTimeout(fwd) ) {
       //Timeout
//      click_chatter("Timeout");
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

        for ( int r = 0; r <= fwd->replica_count; r++ ) {
          if ( ! fwd->have_replica(r) ) {
            next = _dht_routing->get_responsibly_replica_node(_op->header.key_digest, r);//TODO:handle if next is null

            _op->header.replica = r;

            if ( _add_node_id ) {
              p = DHTProtocol::new_dht_packet(STORAGE_SIMPLE, DHT_STORAGE_SIMPLE_MESSAGE, _op->length() + sizeof(struct dht_simple_storage_node_info));
              _op->serialize_buffer(&((DHTProtocol::get_payload(p))[sizeof(struct dht_simple_storage_node_info)]),_op->length());
              struct dht_simple_storage_node_info *ni = (struct dht_simple_storage_node_info *)DHTProtocol::get_payload(p);
              _dht_routing->_me->get_nodeid((md5_byte_t *)ni->src_id, &(ni->src_id_size));
              ni->reserved = 0;
            } else {
              p = DHTProtocol::new_dht_packet(STORAGE_SIMPLE, DHT_STORAGE_SIMPLE_MESSAGE, _op->length());
              _op->serialize_buffer(DHTProtocol::get_payload(p),_op->length());
            }

            p = DHTProtocol::push_brn_ether_header(p,&(_dht_routing->_me->_ether_addr), &(next->_ether_addr), BRN_PORT_DHTSTORAGE);
            output(0).push(p);
          }
        }
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
                  sa << "DB-Node: " << dhtstorage_simple->_dht_routing->_me->_ether_addr.unparse() << "\n";
                  sa << "DB-Size (No. rows): " << dhtstorage_simple->_dht_db->_db.size();
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
