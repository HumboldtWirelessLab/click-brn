#include <click/config.h>
#include "dhtoperation.hh"

CLICK_DECLS

DHTOperation::DHTOperation()
{
  key = NULL;
  keylen = 0;
  value = NULL;
  valuelen = 0;
  status = DHT_STATUS_UNKNOWN;
  operation = OPERATION_UNKNOWN;
}

DHTOperation::~DHTOperation()
{
  if ( key != NULL ) delete key;
  if ( value != NULL ) delete value;
}

void
DHTOperation::insert(char *_key, uint16_t _keylen, char *_value, uint16_t _valuelen)
{
  key = _key;
  keylen = _keylen;
  value = _value;
  valuelen = _valuelen;
  status = DHT_STATUS_UNKNOWN;
  operation = (uint8_t)OPERATION_REQUEST | (uint8_t)OPERATION_INSERT;
}

void
DHTOperation::remove(char *_key, uint16_t _keylen)
{
  key = _key;
  keylen = _keylen;
  value = NULL;                          //TODO: clear if not NULL
  valuelen = 0;
  status = DHT_STATUS_UNKNOWN;
  operation = (uint8_t)OPERATION_REQUEST | (uint8_t)OPERATION_REMOVE;
}

void
DHTOperation::read(char *_key, uint16_t _keylen)
{
  key = _key;
  keylen = _keylen;
  value = NULL;                          //TODO: clear if not NULL
  valuelen = 0;
  status = DHT_STATUS_UNKNOWN;
  operation = (uint8_t)OPERATION_REQUEST | (uint8_t)OPERATION_READ;
}

void
DHTOperation::write(char *_key, uint16_t _keylen, char *_value, uint16_t _valuelen)
{
  key = _key;
  keylen = _keylen;
  value = _value;
  valuelen = _valuelen;
  status = DHT_STATUS_UNKNOWN;
  operation = (uint8_t)OPERATION_REQUEST | (uint8_t)OPERATION_WRITE;
}

void
DHTOperation::lock(char *_key, uint16_t _keylen)
{
  key = _key;
  keylen = _keylen;
  value = NULL;                          //TODO: clear if not NULL
  valuelen = 0;
  status = DHT_STATUS_UNKNOWN;
  operation = (uint8_t)OPERATION_REQUEST | (uint8_t)OPERATION_LOCK;
}

void
DHTOperation::unlock(char *_key, uint16_t _keylen)
{
  key = _key;
  keylen = _keylen;
  value = NULL;                          //TODO: clear if not NULL
  valuelen = 0;
  status = DHT_STATUS_UNKNOWN;
  operation = (uint8_t)OPERATION_REQUEST | (uint8_t)OPERATION_UNLOCK;
}

int
DHTOperation::serialize(char **buffer, uint16_t *len)
{
  char *pbuffer = *buffer;
  int plen;

  plen = SERIALIZE_STATIC_SIZE + valuelen + keylen;
  *len = plen;
  pbuffer = new char[plen];

  if ( serialize_buffer(pbuffer,plen) == -1 )
  {
    click_chatter("Unable to seralize DHT");
    delete pbuffer;
    *len = 0;
  }

  return 0;
}

int
DHTOperation::serialize_buffer(char *buffer, uint16_t maxlen)
{
  int plen;
  uint8_t *p_u8;
  uint16_t *p_u16;
  uint32_t *p_u32;
  char *p_c;

  plen = SERIALIZE_STATIC_SIZE + valuelen + keylen;
  if ( buffer == NULL || plen > maxlen ) return -1;

  p_u8 = (uint8_t*)buffer;
  p_u8[0] = operation;
  p_u8[1] = status;
  p_u32 = (uint32_t*)&p_u8[2];
  p_u32[0] = id;
  p_u16 = (uint16_t*)&p_u32[1];
  p_u16[0] = keylen;
  p_u16[1] = valuelen;
  p_c = (char*)&p_u16[2];
  memcpy(p_c, key, keylen);
  memcpy(&p_c[keylen], value, valuelen);

  return plen;
}

int
DHTOperation::unserialize(char *buffer, uint16_t len)
{
  uint8_t *p_u8;
  uint16_t *p_u16;
  uint32_t *p_u32;
  char *p_c;

  if ( buffer == NULL || SERIALIZE_STATIC_SIZE > len ) return -1;

  p_u8 = (uint8_t*)buffer;
  operation = p_u8[0];
  status = p_u8[1];

  p_u32 = (uint32_t*)&p_u8[2];
  id = p_u32[0];

  p_u16 = (uint16_t*)&p_u32[1];
  keylen = p_u16[0];
  valuelen = p_u16[1];

  p_c = (char*)&p_u16[2];
  key = new char[keylen];
  memcpy(key, p_c, keylen);
  value = new char[valuelen];
  memcpy(value, &p_c[keylen], valuelen);

  return 0;
}

void
DHTOperation::set_request()
{
  operation &= (! (uint8_t)OPERATION_REPLY);
}

void
DHTOperation::set_reply()
{
  operation |= (uint8_t)OPERATION_REPLY;
}

bool
DHTOperation::is_request()
{
  return ( ( operation & OPERATION_REPLY_REQUEST ) == OPERATION_REQUEST );
}

bool
DHTOperation::is_reply()
{
  return ( ( operation & OPERATION_REPLY_REQUEST ) == OPERATION_REPLY );
}

void
DHTOperation::set_id(uint32_t _id)
{
  id = _id;
}

uint32_t
DHTOperation::get_id()
{
  return id;
}

ELEMENT_PROVIDES(DHTOperation)
CLICK_ENDDECLS

