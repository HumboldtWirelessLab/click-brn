#include <click/config.h>
#include "dhtoperation.hh"

CLICK_DECLS

DHTOperation::DHTOperation()
{
  key = NULL;
  header.keylen = 0;
  value = NULL;
  header.valuelen = 0;
  header.status = DHT_STATUS_UNKNOWN;
  header.operation = OPERATION_UNKNOWN;
}

DHTOperation::~DHTOperation()
{
  if ( key != NULL ) delete key;
  if ( value != NULL ) delete value;
}

void
DHTOperation::insert(uint8_t *_key, uint16_t _keylen, uint8_t *_value, uint16_t _valuelen)
{
  key = _key;
  header.keylen = _keylen;
  value = _value;
  header.valuelen = _valuelen;
  header.status = DHT_STATUS_UNKNOWN;
  header.operation = (uint8_t)OPERATION_REQUEST | (uint8_t)OPERATION_INSERT;
}

void
DHTOperation::remove(uint8_t *_key, uint16_t _keylen)
{
  key = _key;
  header.keylen = _keylen;
  value = NULL;                          //TODO: clear if not NULL
  header.valuelen = 0;
  header.status = DHT_STATUS_UNKNOWN;
  header.operation = (uint8_t)OPERATION_REQUEST | (uint8_t)OPERATION_REMOVE;
}

void
DHTOperation::read(uint8_t *_key, uint16_t _keylen)
{
  key = _key;
  header.keylen = _keylen;
  value = NULL;                          //TODO: clear if not NULL
  header.valuelen = 0;
  header.status = DHT_STATUS_UNKNOWN;
  header.operation = (uint8_t)OPERATION_REQUEST | (uint8_t)OPERATION_READ;
}

void
DHTOperation::write(uint8_t *_key, uint16_t _keylen, uint8_t *_value, uint16_t _valuelen)
{
  key = _key;
  header.keylen = _keylen;
  value = _value;
  header.valuelen = _valuelen;
  header.status = DHT_STATUS_UNKNOWN;
  header.operation = (uint8_t)OPERATION_REQUEST | (uint8_t)OPERATION_WRITE;
}

void
DHTOperation::lock(uint8_t *_key, uint16_t _keylen)
{
  key = _key;
  header.keylen = _keylen;
  value = NULL;                          //TODO: clear if not NULL
  header.valuelen = 0;
  header.status = DHT_STATUS_UNKNOWN;
  header.operation = (uint8_t)OPERATION_REQUEST | (uint8_t)OPERATION_LOCK;
}

void
DHTOperation::unlock(uint8_t *_key, uint16_t _keylen)
{
  key = _key;
  header.keylen = _keylen;
  value = NULL;                          //TODO: clear if not NULL
  header.valuelen = 0;
  header.status = DHT_STATUS_UNKNOWN;
  header.operation = (uint8_t)OPERATION_REQUEST | (uint8_t)OPERATION_UNLOCK;
}

void
DHTOperation::set_value(uint8_t *new_value, uint16_t new_valuelen)
{
  if ( value != NULL )
    delete value;

  if ( new_value != NULL && new_valuelen != 0 )
  {
    value = new uint8_t[new_valuelen];
    memcpy(value, new_value, new_valuelen );
    header.valuelen = new_valuelen;
  }
  else
  {
    value = NULL;
    header.valuelen = 0;
  }
}

int
DHTOperation::serialize(uint8_t **buffer, uint16_t *len) //TODO: hton for lens
{
  uint8_t *pbuffer = *buffer;
  int plen;

  plen = SERIALIZE_STATIC_SIZE + header.valuelen + header.keylen;
  *len = plen;
  pbuffer = new uint8_t[plen];

  if ( serialize_buffer(pbuffer,plen) == -1 )
  {
//    click_chatter("Unable to seralize DHT");
    delete pbuffer;
    *len = 0;
  }

  return 0;
}

int
DHTOperation::serialize_buffer(uint8_t *buffer, uint16_t maxlen) //TODO: hton for lens
{
  int plen;

  plen = SERIALIZE_STATIC_SIZE + header.valuelen + header.keylen;
  if ( buffer == NULL || plen > maxlen ) return -1;

  memcpy(buffer,(void*)&header,SERIALIZE_STATIC_SIZE);
  memcpy(&buffer[SERIALIZE_STATIC_SIZE], key, header.keylen);
  memcpy(&buffer[SERIALIZE_STATIC_SIZE + header.keylen], value, header.valuelen);

  return plen;
}

int
DHTOperation::unserialize(uint8_t *buffer, uint16_t len)  //TODO: hton for lens
{

  if ( buffer == NULL || SERIALIZE_STATIC_SIZE > len ) return -1;

  memcpy((void*)&header, buffer, SERIALIZE_STATIC_SIZE);
  key = new uint8_t[header.keylen];
  memcpy(key, &buffer[SERIALIZE_STATIC_SIZE], header.keylen);
  value = new uint8_t[header.valuelen];
  memcpy(value, &buffer[SERIALIZE_STATIC_SIZE + header.keylen], header.valuelen);

  return 0;
}

void
DHTOperation::set_request()
{
  header.operation &= (! (uint8_t)OPERATION_REPLY);
}

void
DHTOperation::set_reply()
{
  header.operation |= (uint8_t)OPERATION_REPLY;
}

bool
DHTOperation::is_request()
{
  return ( ( header.operation & OPERATION_REPLY_REQUEST ) == OPERATION_REQUEST );
}

bool
DHTOperation::is_reply()
{
  return ( ( header.operation & OPERATION_REPLY_REQUEST ) == OPERATION_REPLY );
}

void
DHTOperation::set_id(uint32_t _id)
{
  header.id = _id;
}

uint32_t
DHTOperation::get_id()
{
  return header.id;
}

int
DHTOperation::length()
{
  return SERIALIZE_STATIC_SIZE + header.valuelen + header.keylen;
}

ELEMENT_PROVIDES(DHTOperation)
CLICK_ENDDECLS

