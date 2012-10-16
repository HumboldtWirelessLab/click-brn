#include <click/config.h>
#include "dhtoperation.hh"

CLICK_DECLS

DHTOperation::DHTOperation()
{
  header.id = 0;
  header.replica = 0;               //DEFAULT: ask one node, so no replica
  header.hops = 0;

  key = NULL;
  header.keylen = 0;
  value = NULL;
  header.valuelen = 0;

  memset(header.key_digest,0,sizeof(header.key_digest));

  header.status = DHT_STATUS_UNKNOWN;
  header.operation = OPERATION_UNKNOWN;

  max_retries = DHT_RETRIES_UNLIMITED;
  retries = 0;

  request_time = Timestamp::now();
  max_request_duration = DHT_DURATION_UNLIMITED;
  request_duration = 0;

  digest_is_set = false;
}

DHTOperation::~DHTOperation()
{
  if ( key != NULL )
  {
   delete[] key;
   key = NULL;
  }

  if ( value != NULL )
  {
    delete[] value;
    value = NULL;
  }
}

void
DHTOperation::insert(uint8_t *_key, uint16_t _keylen, uint8_t *_value, uint16_t _valuelen)
{
  key = new uint8_t[_keylen];
  memcpy(key, _key, _keylen);
  header.keylen = _keylen;

  value = new uint8_t[_valuelen];
  memcpy(value, _value, _valuelen);
  header.valuelen = _valuelen;

  header.status = DHT_STATUS_UNKNOWN;
  header.operation = (uint8_t)OPERATION_REQUEST | (uint8_t)OPERATION_INSERT;
}

void
DHTOperation::remove(uint8_t *_key, uint16_t _keylen)
{
  key = new uint8_t[_keylen];
  memcpy(key, _key, _keylen);
  header.keylen = _keylen;

  if ( value != NULL ) delete[] value;
  value = NULL;
  header.valuelen = 0;

  header.status = DHT_STATUS_UNKNOWN;
  header.operation = (uint8_t)OPERATION_REQUEST | (uint8_t)OPERATION_REMOVE;
}

void
DHTOperation::read(uint8_t *_key, uint16_t _keylen)
{
  key = new uint8_t[_keylen];
  memcpy(key, _key, _keylen);
  header.keylen = _keylen;

  if ( value != NULL ) delete[] value;
  value = NULL;
  header.valuelen = 0;

  header.status = DHT_STATUS_UNKNOWN;
  header.operation = (uint8_t)OPERATION_REQUEST | (uint8_t)OPERATION_READ;
}

void
DHTOperation::write(uint8_t *_key, uint16_t _keylen, uint8_t *_value, uint16_t _valuelen, bool insert)
{
  key = new uint8_t[_keylen];
  memcpy(key, _key, _keylen);
  header.keylen = _keylen;

  value = new uint8_t[_valuelen];
  memcpy(value, _value, _valuelen);
  header.valuelen = _valuelen;

  header.status = DHT_STATUS_UNKNOWN;
  if ( insert )
    header.operation = (uint8_t)OPERATION_REQUEST | (uint8_t)OPERATION_WRITE | (uint8_t)OPERATION_INSERT;
  else
    header.operation = (uint8_t)OPERATION_REQUEST | (uint8_t)OPERATION_WRITE;
}

void
DHTOperation::write(uint8_t *_key, uint16_t _keylen, uint8_t *_value, uint16_t _valuelen)
{
  write(_key, _keylen, _value, _valuelen, false);
}

void
DHTOperation::lock(uint8_t *_key, uint16_t _keylen)
{
  key = new uint8_t[_keylen];
  memcpy(key, _key, _keylen);
  header.keylen = _keylen;

  if ( value != NULL ) delete[] value;
  value = NULL;
  header.valuelen = 0;

  header.status = DHT_STATUS_UNKNOWN;
  header.operation = (uint8_t)OPERATION_REQUEST | (uint8_t)OPERATION_LOCK;
}

void
DHTOperation::unlock(uint8_t *_key, uint16_t _keylen)
{
  key = new uint8_t[_keylen];
  memcpy(key, _key, _keylen);
  header.keylen = _keylen;

  if ( value != NULL ) delete[] value;
  value = NULL;
  header.valuelen = 0;

  header.status = DHT_STATUS_UNKNOWN;
  header.operation = (uint8_t)OPERATION_REQUEST | (uint8_t)OPERATION_UNLOCK;
}

void
DHTOperation::set_lock(bool lock)
{
  if ( lock ) header.operation = ((uint8_t)header.operation & ~(uint8_t)OPERATION_UNLOCK ) | (uint8_t)OPERATION_LOCK;
  else header.operation = ((uint8_t)header.operation & ~(uint8_t)OPERATION_LOCK ) | (uint8_t)OPERATION_UNLOCK;
}

void
DHTOperation::set_key_digest(md5_byte_t *new_key_digest)
{
  digest_is_set = true;
  memcpy(header.key_digest, new_key_digest, MD5_DIGEST_LENGTH);
}

void
DHTOperation::unset_key_digest()
{
  digest_is_set = false;
}

bool
DHTOperation::is_set_key_digest()
{
  return digest_is_set;
}

void
DHTOperation::set_value(uint8_t *new_value, uint16_t new_valuelen)
{
  if ( value != NULL )
    delete[] value;

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

void
DHTOperation::set_src_address_of_operation(uint8_t *ea)
{
  src_of_operation = EtherAddress(ea);
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
//  click_chatter("Unable to seralize DHT");
    delete[] pbuffer;
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
  if ( key != NULL )
    memcpy(&buffer[SERIALIZE_STATIC_SIZE], key, header.keylen);
  if ( value != NULL )
    memcpy(&buffer[SERIALIZE_STATIC_SIZE + header.keylen], value, header.valuelen);

  return plen;
}

int
DHTOperation::unserialize(uint8_t *buffer, uint16_t len)  //TODO: hton for lens
{

  if ( buffer == NULL || SERIALIZE_STATIC_SIZE > len ) return -1;

  memcpy((void*)&header, buffer, SERIALIZE_STATIC_SIZE);
  key = new uint8_t[header.keylen ];
  memcpy(key, &buffer[SERIALIZE_STATIC_SIZE], header.keylen);
  value = new uint8_t[header.valuelen ];
  memcpy(value, &buffer[SERIALIZE_STATIC_SIZE + header.keylen], header.valuelen);

  return 0;
}

void
DHTOperation::inc_hops_in_header(uint8_t *buffer, uint16_t len)
{
  if ( buffer == NULL || SERIALIZE_STATIC_SIZE > len ) return;

  struct DHTOperationHeader *h = (struct DHTOperationHeader*)buffer;
  h->hops++;
}

void
DHTOperation::set_request()
{
  header.operation &= (~(uint8_t)OPERATION_REPLY);
}

void
DHTOperation::set_reply()
{
  header.operation |= (uint8_t)OPERATION_REPLY;
}

void
DHTOperation::set_status(uint8_t status)
{
  header.status = status;
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
DHTOperation::set_id(uint16_t _id)
{
  header.id = _id;
}

uint16_t
DHTOperation::get_id()
{
  return header.id;
}

void
DHTOperation::set_replica(uint8_t _replica)
{
  header.replica = _replica;
}

uint8_t
DHTOperation::get_replica()
{
  return header.replica;
}


int
DHTOperation::length()
{
  return SERIALIZE_STATIC_SIZE + header.valuelen + header.keylen;
}

ELEMENT_PROVIDES(DHTOperation)
CLICK_ENDDECLS

