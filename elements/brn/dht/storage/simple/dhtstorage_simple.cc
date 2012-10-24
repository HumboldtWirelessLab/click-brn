#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <click/vector.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/dht/storage/dhtoperation.hh"
#include "elements/brn/dht/protocol/dhtprotocol.hh"
#include "elements/brn/dht/storage/db/db.hh"

#include "dhtprotocol_storagesimple.hh"
#include "dhtstorage_simple.hh"

CLICK_DECLS

DHTStorageSimple::DHTStorageSimple():
  _dht_key_cache(NULL),
  _check_req_queue_timer(req_queue_timer_hook,this),
  _dht_id(0),
  _max_req_time(DEFAULT_REQUEST_TIMEOUT),
  _max_req_retries(DEFAULT_MAX_RETRIES),
  _add_node_id(false)
#ifdef DHT_STORAGE_STATS
 ,_stats_requests(0),
  _stats_replies(0),
  _stats_retries(0),
  _stats_timeouts(0),
  _stats_excessive_timeouts(0),
  _stats_cache_hits(0),
  _stats_hops_sum(0)
#endif
{
  DHTStorage::init();
  _dht_routing = NULL;
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
    "DB",cpkM+cpkP, cpElement, &_db,
    "DHTROUTING", cpkN, cpElement, &_dht_routing,
    "DHTKEYCACHE", cpkN, cpElement, &_dht_key_cache,
    "ADDNODEID", cpkN, cpBool, &_add_node_id,
    "DEBUG", cpkN, cpInteger, &_debug,
    cpEnd) < 0)
      return -1;

  if (!_dht_routing || !_dht_routing->cast("DHTRouting")) {
    _dht_routing = NULL;
    if (_debug >= BrnLogger::WARN) click_chatter("No Routing");
  } else {
    if (_debug >= BrnLogger::INFO) click_chatter("Use DHT-Routing: %s",_dht_routing->dhtrouting_name());
  }

  _dht_op_handler = new DHTOperationHandler(_db,_debug);

  return 0;
}

int DHTStorageSimple::initialize(ErrorHandler *)
{
  _check_req_queue_timer.initialize(this);
  return 0;
}

uint32_t
DHTStorageSimple::dht_request(DHTOperation *op, void (*info_func)(void*,DHTOperation*), void *info_obj )
{
  DHTnode *next;
  EtherAddress *next_ea;
  DHTOperationForward *fwd_op;
  WritablePacket *p;
  uint32_t dht_id, replica_count;
  int status;

  if ( ! op->digest_is_set )
    MD5::calculate_md5((char*)op->key, op->header.keylen, op->header.key_digest);  //TODO: Move this upward. (e.g. during create new dhtoperation.
    //This enables to set a own key for value -> e.g. for range queries. If digest is not set use default(md5)

  //Check whether routing support replica and whether the requested number of replica is support. Correct if something cannot be performed by routing
  if ( _dht_routing != NULL ) {
    if ( _dht_routing->max_replication() < op->header.replica )
      replica_count = _dht_routing->max_replication();
    else
      replica_count = op->header.replica;
  } else {
    replica_count = 0; //DHT used for local storage, so no replicas
  }

  fwd_op = new DHTOperationForward(op, info_func, info_obj, replica_count);

  dht_id = get_next_dht_id();
  op->set_id(dht_id);

  if ( _dht_routing != NULL ) {
    op->set_src_address_of_operation(_dht_routing->_me->_ether_addr.data());   //Set my etheradress as sender
  }

  for ( uint32_t r = 0; r <= replica_count; r++ ) {

#ifdef DHT_STORAGE_STATS
    _stats_requests++;
#endif

    /* Get next node (EtherAddress): 1. KeyCache 2. DHTRouting */
    next_ea = NULL;
    if ( _dht_key_cache != NULL ) {
      next_ea = _dht_key_cache->getEntry(op->header.key_digest, r);
#ifdef DHT_STORAGE_STATS
      if ( next_ea != NULL ) _stats_cache_hits++;
#endif
    }

    if ( (_dht_routing != NULL) && (next_ea == NULL) ) {
      next = _dht_routing->get_responsibly_node(op->header.key_digest, r);
      if ( next != NULL ) next_ea = &(next->_ether_addr);
    }

    op->header.replica = r;

    if ( (_dht_routing != NULL) && (next_ea == NULL) ) //We have routing but no next node
    {
      BRN_DEBUG("No next node!");
      fwd_op->replicaList[r].status = DHT_STATUS_KEY_NOT_FOUND;
      fwd_op->set_replica_reply(r);
    } else {
      if ( (_dht_routing == NULL) || (_dht_routing->is_me(next_ea)) ) //handle local if i'm the next or no routing available
      {
        BRN_DEBUG("Handle Operation locally");
        status = _dht_op_handler->handle_dht_operation(op);

        fwd_op->replicaList[r].status = status;
        fwd_op->replicaList[r].set_value(op->value,op->header.valuelen);
        fwd_op->set_replica_reply(r);
        //TODO: check whether all needed replica-reply are received. Wenn nicht alle replicas n�tig
        //      sind ( z.b. nur eine von 3 Antworten, dann k�nnte das soweit sein und man k�nnte sich den rest sparen.
        // vielleict sollten die replicas in einem solchen fall sortiert werden. Der locale k�nnte reichen und wenn dieser zuerst kommt,.so keine Pakete
#ifdef DHT_STORAGE_STATS
        _stats_replies++;
#endif
      }
      else
      {
        if ( info_func == NULL ) {                                                     //CALLBACK is NULL so operation has to be handle locally
          BRN_DEBUG("Request for local, but responsible is foreign node !");
#ifdef DHT_STORAGE_STATS
          _stats_replies++;
#endif
          fwd_op->replicaList[r].status = DHT_STATUS_KEY_NOT_FOUND;
          fwd_op->set_replica_reply(r);
        } else {
          p = DHTProtocolStorageSimple::new_dht_operation_packet(op, _dht_routing->_me, next_ea, _add_node_id);
          if ( noutputs() > 0 ) {
            output(0).push(p);
          }
        }
      }
    }
  }

  if ( fwd_op->have_all_replicas() ) {
    op->header.replica = fwd_op->replica_count;
    op->set_reply();

    delete fwd_op;

    return DHT_OPERATION_ID_LOCAL_REPLY;
  } else {
    _fwd_queue.push_back(fwd_op);

    int time2next = get_time_to_next();
    if ( time2next < 10 ) time2next = 10;  //TODO:
    _check_req_queue_timer.schedule_after_msec(time2next);
  }

  return dht_id;
}

/**
 * TODO: restructure the method  (too big,....)
 */

void DHTStorageSimple::push( int port, Packet *packet )
{
  DHTnode *next;
  DHTOperation *_op = NULL;
  WritablePacket *p;
  DHTOperationForward *fwd;

  if ( _dht_routing != NULL )   //use dht-routing, ask routing for next node
  {
    BRN_DEBUG("STORAGE PUSH");

    if ( port == 0 )
    {
      switch ( DHTProtocol::get_type(packet) ) {
        case DHT_STORAGE_SIMPLE_MESSAGE : {
          /**
           * Handle Operation
           **/
          BRN_DEBUG("Operation");
          /* Take genaral information from the packet */

          _op = new DHTOperation();

          EtherAddress src;
          struct dht_simple_storage_node_info *ni = DHTProtocolStorageSimple::unpack_dht_operation_packet(packet, _op, &src, _add_node_id);
          if ( ni != NULL ) _dht_routing->update_node(&src, ni->src_id, ni->src_id_size);
          else _dht_routing->update_node(&src, NULL, 0);

          /* Handle core information from the packet */

          /**
          * Handle Reply
          **/

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

#ifdef DHT_STORAGE_STATS
                _stats_replies++;
                _stats_hops_sum += _op->header.hops;
#endif

                if ( _dht_key_cache != NULL ) _dht_key_cache->addEntry(_op->header.key_digest, _op->header.replica, DHTProtocol::get_src_data(packet));

                fwd->replicaList[_op->header.replica].status = _op->header.status;
                fwd->replicaList[_op->header.replica].set_value(_op->value, _op->header.valuelen);

                fwd->set_replica_reply(_op->header.replica);

                if ( fwd->have_all_replicas() ) {
                  Timestamp now = Timestamp::now();
                  _op->max_retries = fwd->_operation->max_retries;
                  _op->retries = fwd->_operation->retries;

                  _op->request_time = fwd->_operation->request_time;
                  _op->max_request_duration = fwd->_operation->max_request_duration;
                  _op->request_duration = (now - _op->request_time).msecval();

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
            /**
            * Handle Request
            **/

            next = _dht_routing->get_responsibly_node(_op->header.key_digest, _op->header.replica);

            if ( _dht_routing->is_me(next) )
            {
              /**
              * I'm responsible
              **/

              BRN_DEBUG("I'm responsible for Operation (%s).",_dht_routing->_me->_ether_addr.unparse().c_str());

              _op->header.hops++;
              int result = _dht_op_handler->handle_dht_operation(_op);

              if ( result == -1 ) {
                BRN_DEBUG("Operation error");
                delete _op;
              } else {
                _op->set_reply();

                //create packet for dht-reply
                EtherAddress dst_node = _op->src_of_operation;                                                          //save src of dht-request
                _op->set_src_address_of_operation(_dht_routing->_me->_ether_addr.data());                               //now i'm the src
                p = DHTProtocolStorageSimple::new_dht_operation_packet(_op, _dht_routing->_me, &dst_node, _add_node_id); //create packet

                delete _op;
                if ( noutputs() > 0 ) {
                  output(0).push(p);
                }
              }
            }
            else
            {
              /**
              * Forward the request
              **/

              BRN_DEBUG("Forward operation.");
              BRN_DEBUG("Me: %s Dst: %s",_dht_routing->_me->_ether_addr.unparse().c_str(), next->_ether_addr.unparse().c_str());

              p = packet->uniqueify();
              DHTProtocolStorageSimple::inc_hops_of_dht_operation_packet(p, _add_node_id);  //inc the count of hops direct
              p = DHTProtocol::push_brn_ether_header(p,&(_dht_routing->_me->_ether_addr), &(next->_ether_addr), BRN_PORT_DHTSTORAGE);
              delete _op;
              if ( noutputs() > 0 ) {
                output(0).push(p);
              }
              return;  //Reuse packet so return here to prevend packet->kill() //TODO: reorder all
            }
          }
          break;
        } // end of handle operation
      } //switch (packet_type)
    }  //end (if port == 0)
  } else {
    BRN_WARN("Error: DHTStorageSimple: Got Packet, but have no routing. Discard Packet");
  }

  packet->kill();

}

uint16_t
DHTStorageSimple::get_next_dht_id()
{
  if ( ++_dht_id == 0 ) _dht_id++;
  return _dht_id;
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
  DHTOperationForward *fwd;
  int min_time, ac_time;

  Timestamp now = Timestamp::now();

  if ( _fwd_queue.size() > 0 ) {
    fwd = _fwd_queue[0];
    min_time = _max_req_time - (now - fwd->_last_request_time).msecval();
  } else
    return -1;

  for( int i = 1; i < _fwd_queue.size(); i++ )
  {
    fwd = _fwd_queue[i];
    ac_time = _max_req_time - (now - fwd->_last_request_time).msecval();
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

    timediff = (now - fwd->_last_request_time).msecval();

//  click_chatter("Test: Timediff: %d MAX: %d",timediff,_max_req_time);
    if ( timediff >= (int)_max_req_time ) {

#ifdef DHT_STORAGE_STATS
      _stats_timeouts++;
#endif

      _op = fwd->_operation;

      if ( isFinalTimeout(fwd) ) {
#ifdef DHT_STORAGE_STATS
        _stats_excessive_timeouts++;
#endif
        _op->set_status(DHT_STATUS_TIMEOUT); //DHT_STATUS_MAXRETRY
        _op->set_reply();
        _op->request_duration = (now - _op->request_time).msecval();

        fwd->_info_func(fwd->_info_obj,_op);
        _fwd_queue.erase(_fwd_queue.begin() + i);
        delete fwd;
      } else {
        // Retry
        fwd->_last_request_time = Timestamp::now();
        _op->retries++;

#ifdef DHT_STORAGE_STATS
        _stats_retries++;
#endif
        if ( _dht_routing != NULL ) {
          for ( uint16_t r = 0; r <= fwd->replica_count; r++ ) {
            if ( ! fwd->have_replica(r) ) {
              next = _dht_routing->get_responsibly_node(_op->header.key_digest, r);  //TODO:handle if next is null or me

              _op->header.replica = r;

              p = DHTProtocolStorageSimple::new_dht_operation_packet(_op, _dht_routing->_me, &(next->_ether_addr), _add_node_id);
              if ( noutputs() > 0 ) {
                output(0).push(p);
              }
            }
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
  H_STATS
};

String
DHTStorageSimple::read_stats()
{
  StringAccum sa;

  sa << "<dhtstorage id=\"" << BRN_NODE_NAME << "\" time=\"" << Timestamp::now().unparse() << "\" >\n";

#ifdef DHT_STORAGE_STATS
  sa << "\t<stats request=\"" << _stats_requests << "\" replies=\"" << _stats_replies << "\" retries=\"" << _stats_retries;
  sa << "\" timeouts=\"" << _stats_timeouts << "\" excessive_timeouts=\"" << _stats_excessive_timeouts;
  sa << "\" cachehits=\"" << _stats_cache_hits;
  sa << "\" avg_hops=\"" << (uint32_t)((_stats_replies)?(_stats_hops_sum/_stats_replies):0) << "\" />\n";
#endif


  int moved = 0;
  for ( int i = _dht_op_handler->_db->size() - 1 ; i >= 0; i-- )
    if ( _dht_op_handler->_db->getRow(i)->move_id != 0 ) moved++;

  sa << "\t<db rows=\"" << _dht_op_handler->_db->size() << "\" moved=\"" << moved << "\" >\n";

  char digest[16*2 + 1];

  for ( int i = _dht_op_handler->_db->size() - 1 ; i >= 0; i-- ) {
    BRNDB::DBrow *_row = _dht_op_handler->_db->getRow(i);
    MD5::printDigest(_row->md5_key, digest);

    sa << "\t\t<row md5=\"" << digest << "\" keylen=\"" << _row->keylen << "\" valuelen=\"" << _row->valuelen << "\" locked=\"";
    sa << String(_row->locked) << "\" movedid=\"" << _row->move_id << "\" replica=\"" << (uint32_t)_row->replica << "\" />\n";
  }

  sa << "\t</db>\n</dhtstorage>\n";

  return sa.take_string();
}


static String
read_param(Element *e, void *thunk)
{
  DHTStorageSimple *dhtstorage_simple = (DHTStorageSimple *)e;
  StringAccum sa;

  switch ((uintptr_t) thunk)
  {
    case H_STATS : return dhtstorage_simple->read_stats();
  }

  return String();
}

void
DHTStorageSimple::add_handlers()
{
  DHTStorage::add_handlers();

  add_read_handler("stats", read_param , (void *)H_STATS);
}

#include <click/vector.cc>
template class Vector<DHTStorageSimple::DHTOperationForward*>;

CLICK_ENDDECLS
EXPORT_ELEMENT(DHTStorageSimple)
