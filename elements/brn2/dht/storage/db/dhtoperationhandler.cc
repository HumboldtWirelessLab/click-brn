#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/error.hh>
#include <click/straccum.hh>


#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "dhtoperationhandler.hh"

CLICK_DECLS

DHTOperationHandler::DHTOperationHandler(BRNDB *db):
  _debug(BrnLogger::DEFAULT)
{
  _db = db;
}

DHTOperationHandler::DHTOperationHandler(BRNDB *db, int debug)
{
  _db = db;
  _debug = debug;
}

DHTOperationHandler::~DHTOperationHandler()
{
}

int
DHTOperationHandler::handle_dht_operation(DHTOperation *op)
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
DHTOperationHandler::dht_insert(DHTOperation *op)
{
  if ( _db->getRow(op->header.key_digest) == NULL )
  {
    _db->insert(op->header.key_digest, op->key, op->header.keylen, op->value, op->header.valuelen);
    op->header.status = DHT_STATUS_OK;
  }
  else
  {
    if ( _debug == BrnLogger::WARN ) click_chatter("Key already exists");
    op->header.status = DHT_STATUS_KEY_ALREADY_EXISTS;
  }

  return 0;
}

int
DHTOperationHandler::dht_write(DHTOperation *op)
{
  BRNDB::DBrow *_row;

  _row = _db->getRow(op->header.key_digest);
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
DHTOperationHandler::dht_read(DHTOperation *op)
{
  BRNDB::DBrow *_row;

  _row = _db->getRow(op->header.key_digest);
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
DHTOperationHandler::dht_remove(DHTOperation *op)
{
  BRNDB::DBrow *_row;
  EtherAddress ea = EtherAddress(op->header.etheraddress);

  _row = _db->getRow(op->header.key_digest);
  if ( _row != NULL ) {
    if ( _row->unlock(&ea) ) {            //unlock returns true if row is not locked or if ea is the source of the lock
      _db->delRow(op->header.key_digest);  //TODO: Errorhandling
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
DHTOperationHandler::dht_lock(DHTOperation *op)
{
  BRNDB::DBrow *_row;
  EtherAddress ea = EtherAddress(op->header.etheraddress);

  _row = _db->getRow(op->header.key_digest);

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
DHTOperationHandler::dht_unlock(DHTOperation *op)
{
  BRNDB::DBrow *_row;
  EtherAddress ea = EtherAddress(op->header.etheraddress);

  _row = _db->getRow(op->header.key_digest);

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

CLICK_ENDDECLS
ELEMENT_PROVIDES(DHTOperationHandler)
ELEMENT_REQUIRES(BRNDB)
