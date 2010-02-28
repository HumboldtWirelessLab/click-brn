#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "dhtstorage_simple_db.hh"

CLICK_DECLS

DHTStorageSimpleDB::DHTStorageSimpleDB():
  _debug(BrnLogger::DEFAULT)
{
}

DHTStorageSimpleDB::~DHTStorageSimpleDB()
{
}

int DHTStorageSimpleDB::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int DHTStorageSimpleDB::initialize(ErrorHandler *)
{
  return 0;
}


int
DHTStorageSimpleDB::handle_dht_operation(DHTOperation *op)
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
DHTStorageSimpleDB::dht_insert(DHTOperation *op)
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
DHTStorageSimpleDB::dht_write(DHTOperation *op)
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

  return 0;
}

int
DHTStorageSimpleDB::dht_read(DHTOperation *op)
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

  return 0;
}

int
DHTStorageSimpleDB::dht_remove(DHTOperation *op)
{
  BRNDB::DBrow *_row;
  EtherAddress ea = EtherAddress(op->header.etheraddress);

  _row = _db.getRow(op->header.key_digest);
  if ( _row != NULL ) {
    if ( _row->unlock(&ea) ) {            //unlock returns true if row is not locked or if ea is the source of the lock
      _db.delRow(op->header.key_digest);  //TODO: Errorhandling
      op->header.status = DHT_STATUS_OK;
    } else {
      op->header.status = DHT_STATUS_KEY_IS_LOCKED;
    }
  } else {
    op->header.status = DHT_STATUS_OK;
  }

  return 0;
}

int
DHTStorageSimpleDB::dht_lock(DHTOperation *op)
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

  return 0;
}

int
DHTStorageSimpleDB::dht_unlock(DHTOperation *op)
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

  return 0;
}

/*************************************************************************************************/
/***************************************** H A N D L E R *****************************************/
/*************************************************************************************************/

static String
read_debug_param(Element *e, void *)
{
  DHTStorageSimpleDB *db = (DHTStorageSimpleDB *)e;
  return String(db->_debug) + "\n";
}

static int
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  DHTStorageSimpleDB *db = (DHTStorageSimpleDB *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug))
    return errh->error("debug parameter must be an integer value between 0 and 4");
  db->_debug = debug;
  return 0;
}

void DHTStorageSimpleDB::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DHTStorageSimpleDB)
