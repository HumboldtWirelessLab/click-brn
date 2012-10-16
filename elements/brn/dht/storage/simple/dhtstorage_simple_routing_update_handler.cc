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
#include "elements/brn2/dht/protocol/dhtprotocol.hh"
#include "elements/brn2/dht/storage/db/db.hh"

#include "dhtprotocol_storagesimple.hh"
#include "dhtstorage_simple_routing_update_handler.hh"

CLICK_DECLS

DHTStorageSimpleRoutingUpdateHandler::DHTStorageSimpleRoutingUpdateHandler():
  _dht_routing(NULL),
  _debug(BrnLogger::DEFAULT),
  _moved_id(0),
  _move_data_timer(data_move_timer_hook,this)
{
}

DHTStorageSimpleRoutingUpdateHandler::~DHTStorageSimpleRoutingUpdateHandler()
{
}

int DHTStorageSimpleRoutingUpdateHandler::configure(Vector<String> &conf, ErrorHandler *errh)
{

  if (cp_va_kparse(conf, this, errh,
    "DB",cpkM+cpkP, cpElement, &_db,
    "DHTROUTING", cpkN, cpElement, &_dht_routing,
    "DEBUG", cpkN, cpInteger, &_debug,
    cpEnd) < 0)
      return -1;

  if (!_dht_routing || !_dht_routing->cast("DHTRouting")) {
    _dht_routing = NULL;
    if (_debug >= BrnLogger::WARN) click_chatter("No Routing");
  } else {
    if (_debug >= BrnLogger::INFO) click_chatter("Use DHT-Routing: %s",_dht_routing->dhtrouting_name());
  }

  return 0;
}

static void notify_callback_func(void *e, int status)
{
  DHTStorageSimpleRoutingUpdateHandler *s = (DHTStorageSimpleRoutingUpdateHandler *)e;
  s->handle_notify_callback(status);
}

void
DHTStorageSimpleRoutingUpdateHandler::handle_notify_callback(int status)
{
  BRN_DEBUG("DHT-Routing-Callback %s: Status %d",class_name(),status);

  if ( ( status & ROUTING_STATUS_NEW_NODE ) == ROUTING_STATUS_NEW_NODE ) {
    BRN_INFO("Routing update (new node,...)");
    handle_node_update();
  } else {
    BRN_WARN("Unknown Status from routing layer");
  }
}

int DHTStorageSimpleRoutingUpdateHandler::initialize(ErrorHandler *)
{
  if ( _dht_routing != NULL ) {
    _dht_routing->add_notify_callback(notify_callback_func,(void*)this);
    _move_data_timer.initialize(this);
  } else {
    BRN_INFO("No DHT-Routing.");
  }

  return 0;
}

void DHTStorageSimpleRoutingUpdateHandler::push( int port, Packet *packet )
{
  if ( _dht_routing != NULL )   //use dht-routing, ask routing for next node
  {
    BRN_DEBUG("STORAGE PUSH");

    if ( port == 0 )
    {
      switch ( DHTProtocol::get_type(packet) ) {
        case DHT_STORAGE_SIMPLE_MOVEDDATA : {
          /**
           * Handle moved data
          **/
          BRN_DEBUG("Received moved Data");
          handle_moved_data(packet);
          return;
        }
        case DHT_STORAGE_SIMPLE_ACKDATA : {
          /**
          * Handle ack for moved data
          **/
          BRN_DEBUG("Received ack for moved Data");
          handle_ack_for_moved_data(packet);
          break;
        }
      } //switch (packet_type)
    }  //end (if port == 0)
  } else {
    BRN_WARN("Error: DHTStorageSimpleRoutingUpdateHandler: Got Packet, but have no routing. Discard Packet");
  }

  packet->kill();

}

/**************************************************************************/
/********************* D A T A - M O V E - S T U F F **********************/
/**************************************************************************/

uint32_t
DHTStorageSimpleRoutingUpdateHandler::handle_node_update()
{
  BRNDB::DBrow *_row;
  DHTnode *next;
  DHTMovedDataInfo *mdi;
  WritablePacket *data_p, *p;

  if ( _dht_routing == NULL ) {
    BRN_INFO("No DHT-Routing.");
    return 0;
  }

  for ( int i = 0; i < _db->size(); i++ ) {
    _row = _db->getRow(i);

    BRN_DEBUG("Row replica: %d",_row->replica);

    next = _dht_routing->get_responsibly_node(_row->md5_key, _row->replica);
    if ( ! _dht_routing->is_me(next) ) {
      if ((_row->status == DATA_OK) && (_row->move_id == 0)) {

        if ( ++_moved_id == 0 ) _moved_id++;

         //TODO: use next to handle Timeouts while tranfering data to new nodes
         mdi = new DHTMovedDataInfo(&next->_ether_addr, _moved_id);
        _md_queue.push_back(mdi);

        _row->status = DATA_INIT_MOVE;
        _row->move_id = _moved_id;

        data_p = DHTProtocolStorageSimple::new_data_packet(&_dht_routing->_me->_ether_addr, _row->move_id, 1,
                                                           _row->serializeSize());
        uint8_t *data = DHTProtocolStorageSimple::get_data_packet_payload(data_p);
        _row->serialize(data, _row->serializeSize());

        BRN_DEBUG("Move data from %s to %s",_dht_routing->_me->_ether_addr.unparse().c_str(),next->_ether_addr.unparse().c_str());

        p = DHTProtocol::push_brn_ether_header(data_p, &(_dht_routing->_me->_ether_addr), &next->_ether_addr, BRN_PORT_DHTSTORAGE);
        output(0).push(p);
      } else {
        BRN_WARN("Row is not ok ! Already moved ???");
      }
    }
  }

  set_move_timer();

  return 0;
}

DHTStorageSimpleRoutingUpdateHandler::DHTMovedDataInfo*
DHTStorageSimpleRoutingUpdateHandler::get_move_info(EtherAddress *ea)
{
  DHTMovedDataInfo *mdi;

  for( int i = 0; i < _md_queue.size(); i++ )
  {
    mdi = _md_queue[i];
    if ( memcmp(ea->data(),mdi->_target.data(),6) == 0 ) return mdi;
  }

  return NULL;
}

void
DHTStorageSimpleRoutingUpdateHandler::handle_moved_data(Packet *p)
{
  if ( _dht_routing == NULL ) {
    BRN_INFO("No DHT-Routing.");
    return;
  }

  DHTnode *next;

  struct dht_simple_storage_data *dssd = DHTProtocolStorageSimple::get_data_packet_header(p); //points to header (moveid)
  uint8_t *data = DHTProtocolStorageSimple::get_data_packet_payload(p);                       //points to first row
  struct db_row_header *rh = (struct db_row_header *)data;

  next = _dht_routing->get_responsibly_node(rh->md5_key, rh->replica);

  if ( _dht_routing->is_me(next) )
  {
    BRN_DEBUG("Insert moved data");

    uint32_t moveid = ntohl(dssd->move_id);

    EtherAddress src = EtherAddress(DHTProtocol::get_src_data(p));

    if ( memcmp(_dht_routing->_me->_ether_addr.data(), src.data(), 6 ) == 0 ) {
      BRN_WARN("The source of the moved data (dht) is me. Some node (maybe me) has a brocken routingtable. Reset rows and try again");

      for ( int i = _db->size() - 1 ; i >= 0; i-- ) {
        BRNDB::DBrow *_row = _db->getRow(i);
        if ( _row->move_id == moveid ) {
          _row->status = DATA_OK;
          _row->move_id = 0;
        }
      }

      handle_node_update();

    } else {
      BRNDB::DBrow *new_row = new BRNDB::DBrow();
      new_row->unserialize(data, sizeof(struct db_row_header) + ntohs(rh->keylen) + ntohs(rh->valuelen));
      _db->insert(new_row);

      BRN_DEBUG("Send ack for data to source");
      WritablePacket *ack_p = DHTProtocolStorageSimple::new_ack_data_packet(moveid);
      WritablePacket *wp = DHTProtocol::push_brn_ether_header(ack_p, &(_dht_routing->_me->_ether_addr), &src , BRN_PORT_DHTSTORAGE);
      output(0).push(wp);
    }

    p->kill();

  } else {
    BRN_DEBUG("Found better node, so forward data.");
    WritablePacket *wp = p->uniqueify();
    wp = DHTProtocol::push_brn_ether_header(wp, &(_dht_routing->_me->_ether_addr), &(next->_ether_addr), BRN_PORT_DHTSTORAGE);
    output(0).push(wp);
  }
}

void
DHTStorageSimpleRoutingUpdateHandler::handle_ack_for_moved_data(Packet *p)
{
  BRNDB::DBrow *_row;
  uint32_t moveid = DHTProtocolStorageSimple::get_ack_movid(p); //points to header (moveid)

  BRN_DEBUG("Search in table for move_id: %d.",moveid);

  for ( int i = _db->size() - 1 ; i >= 0; i-- ) {
    _row = _db->getRow(i);

    if ( _row->move_id == moveid ) {
      _db->delRow(i);
    }
  }

  DHTMovedDataInfo *mdi;

  for( int i = _md_queue.size() - 1; i >= 0; i-- )
  {
    mdi = _md_queue[i];
    if ( moveid == mdi->_movedID ) {
      delete _md_queue[i];
      _md_queue.erase(_md_queue.begin() + i);
    }
  }
}

/*********** D A T A - M O V E   T I M E R **************/

void
DHTStorageSimpleRoutingUpdateHandler::data_move_timer_hook(Timer *, void *f)
{
  DHTStorageSimpleRoutingUpdateHandler *dhtss = (DHTStorageSimpleRoutingUpdateHandler *)f;
  dhtss->check_moved_data();
}

void
DHTStorageSimpleRoutingUpdateHandler::check_moved_data()
{
  if ( _dht_routing == NULL ) {
    BRN_INFO("No DHT-Routing.");
    return;
  }

  DHTMovedDataInfo *mdi;
  Timestamp now = Timestamp::now();
  BRNDB::DBrow *_row;
  int timediff;

  for( int i = (_md_queue.size() - 1); i >= 0; i-- )
  {
    mdi = _md_queue[i];

    timediff = (now - mdi->_move_time).msecval();

    if ( timediff >= 2000 ) {
      BRN_DEBUG("TIMEOUT for moveid %d",mdi->_movedID);
      if ( mdi->_tries > 3 ) {
        BRN_DEBUG("Finale timeout for moveid %d",mdi->_movedID);
        //TODO: remove entry and start again to move data
        mdi->_tries = 0;
        mdi->_move_time = now;
      } else {
        mdi->_tries++;
        mdi->_move_time = now;

        for ( int i = _db->size() - 1 ; i >= 0; i-- ) {
          _row = _db->getRow(i);

          if ( _row->move_id == mdi->_movedID ) break;
        }

        //TODO search for better and move to this (maybe there was an update since last move
        WritablePacket *data_p = DHTProtocolStorageSimple::new_data_packet(&_dht_routing->_me->_ether_addr, _row->move_id, 1,
            _row->serializeSize());
        uint8_t *data = DHTProtocolStorageSimple::get_data_packet_payload(data_p);
        _row->serialize(data, _row->serializeSize());

        BRN_DEBUG("Try again: Move data from %s to %s",_dht_routing->_me->_ether_addr.unparse().c_str(),mdi->_target.unparse().c_str());

        WritablePacket *p = DHTProtocol::push_brn_ether_header(data_p, &(_dht_routing->_me->_ether_addr), &mdi->_target, BRN_PORT_DHTSTORAGE);
        output(0).push(p);
      }
    }
  }

  set_move_timer();
}

void
DHTStorageSimpleRoutingUpdateHandler::set_move_timer()
{
  DHTMovedDataInfo *mdi;
  int min_time, ac_time;

  Timestamp now = Timestamp::now();

  if ( _md_queue.size() > 0 ) {
    mdi = _md_queue[0];
    min_time = 2000 - (now - mdi->_move_time).msecval();
  } else
    min_time = -1;

  for( int i = 1; i < _md_queue.size(); i++ )
  {
    mdi = _md_queue[i];
    ac_time = 2000 - (now - mdi->_move_time).msecval();
    if ( ac_time < min_time ) min_time = ac_time;
  }

  if ( min_time < 0 ) return;

  _move_data_timer.schedule_after_msec( min_time );

}

/**************************************************************************/
/************************** H A N D L E R *********************************/
/**************************************************************************/

void
DHTStorageSimpleRoutingUpdateHandler::add_handlers()
{

}
#include <click/vector.cc>
template class Vector<DHTStorageSimpleRoutingUpdateHandler::DHTMovedDataInfo*>;

CLICK_ENDDECLS
EXPORT_ELEMENT(DHTStorageSimpleRoutingUpdateHandler)
