#include <click/config.h>
#include <click/element.hh>
#include <click/etheraddress.hh>

#include "dhtnode.hh"

#include "elements/brn/standard/brn_md5.hh"

CLICK_DECLS
DHTnode::DHTnode():
  _status(STATUS_UNKNOWN),
  _last_ping(Timestamp(0)),
  _failed_ping(0),
  _neighbor(false),
  _hop_distance(0),
  _rtt(0),
  _extra(NULL)
{
  _ether_addr = EtherAddress();

  memset(_md5_digest, 0, sizeof(_md5_digest));
  _digest_length = DEFAULT_DIGEST_LENGTH;
}

DHTnode::DHTnode(EtherAddress addr):
  _status(STATUS_UNKNOWN),
  _last_ping(Timestamp(0)),
  _failed_ping(0),
  _neighbor(false),
  _hop_distance(0),
  _rtt(0),
  _extra(NULL)
{
  _ether_addr = addr;

  MD5::calculate_md5((const char*)MD5::convert_ether2hex(addr.data()).c_str(),
    strlen((const char*)MD5::convert_ether2hex(addr.data()).c_str()), _md5_digest );

  _digest_length = DEFAULT_DIGEST_LENGTH;
}

DHTnode::DHTnode(EtherAddress addr, md5_byte_t *nodeid):
  _status(STATUS_UNKNOWN),
  _last_ping(Timestamp(0)),
  _failed_ping(0),
  _neighbor(false),
  _hop_distance(0),
  _rtt(0),
  _extra(NULL)
{
  _ether_addr = addr;

  memcpy(_md5_digest, nodeid, 16);
  _digest_length = DEFAULT_DIGEST_LENGTH;
}

DHTnode::DHTnode(EtherAddress addr, md5_byte_t *nodeid, uint16_t digest_length):
  _status(STATUS_UNKNOWN),
  _last_ping(Timestamp(0)),
  _failed_ping(0),
  _neighbor(false),
  _hop_distance(0),
  _rtt(0),
  _extra(NULL)
{
  _ether_addr = addr;

  memset(_md5_digest, 0, sizeof(_md5_digest));
  _digest_length = digest_length;

  if ( (nodeid != NULL) && (_digest_length > 0) ) {
    if ( ( _digest_length & 7 ) == 0 )
      memcpy(_md5_digest, nodeid, (_digest_length >> 3));     // _digest_length mod 8
    else
      memcpy(_md5_digest, nodeid, (_digest_length >> 3) + 1); // (_digest_length mod 8) + 1
  }
}

void
DHTnode::reset()
{
  _status = STATUS_UNKNOWN;
  _extra = NULL;
  _age = Timestamp::now();
  _last_ping = Timestamp(0);
  _failed_ping = 0;
  _rtt = 0;
  _hop_distance = 0;
  _neighbor = false;
}

void
DHTnode::set_update_addr(uint8_t *ea)
{
  _ether_addr = EtherAddress(ea);
  _status = STATUS_OK;
  _extra = NULL;
  _age = Timestamp::now();

  MD5::calculate_md5((const char*)MD5::convert_ether2hex(_ether_addr.data()).c_str(),
                      strlen((const char*)MD5::convert_ether2hex(_ether_addr.data()).c_str()), _md5_digest );

  _digest_length = DEFAULT_DIGEST_LENGTH;
}

void
DHTnode::set_etheraddress(uint8_t *ea)
{
  _ether_addr = EtherAddress(ea);
}

void
DHTnode::set_nodeid(md5_byte_t *nodeid)
{
  memcpy(_md5_digest, nodeid, MAX_NODEID_LENTGH);
  _digest_length = DEFAULT_DIGEST_LENGTH;
}

void
DHTnode::set_nodeid(md5_byte_t *nodeid, uint16_t digest_length)
{
  memset(_md5_digest, 0, sizeof(_md5_digest));
  _digest_length = digest_length;

  if ( digest_length > 0 ) {
    if ( ( _digest_length & 7 ) == 0 )
      memcpy(_md5_digest, nodeid, (_digest_length >> 3));     // _digest_length mod 8
    else
      memcpy(_md5_digest, nodeid, (_digest_length >> 3) + 1); // (_digest_length mod 8) + 1
  }
}

void
DHTnode::get_nodeid(md5_byte_t *nodeid, uint8_t *digest_length)
{
  memcpy(nodeid, _md5_digest, sizeof(_md5_digest));
  *digest_length = _digest_length;
}

/*************************/
/******** A G E **********/
/*************************/

void
DHTnode::set_age_now()
{
  _age = Timestamp::now();
}

void
DHTnode::set_age_s(int s)
{
  _age = Timestamp::now() - Timestamp(s);
}

void
DHTnode::set_age(Timestamp *age)
{
  _age = *age;
}

int
DHTnode::get_age_s(Timestamp *now)
{
  if (now) return (*now - _age).sec();
  return( (Timestamp::now() - _age).sec());
}

Timestamp
DHTnode::get_age()
{
  return( (Timestamp::now() - _age));
}

/*************************/
/****** P I N G **********/
/*************************/

void
DHTnode::set_last_ping_now()
{
  _last_ping = Timestamp::now();
}

void
DHTnode::set_last_ping_s(int s)
{
  _last_ping = Timestamp::now() - Timestamp(s);
}

void
DHTnode::set_last_ping(Timestamp *last_ping)
{
  _age = *last_ping;
}

int
DHTnode::get_last_ping_s()
{
  return( (Timestamp::now() - _last_ping).sec());
}

Timestamp
DHTnode::get_last_ping()
{
  return (Timestamp::now() - _last_ping);
}

/*************************/
/**** S T A T U S ********/
/*************************/

String
DHTnode::get_status_string()
{
  switch (_status)
  {
    case STATUS_NEW: return "New";
    case STATUS_OK: return "OK";
    case STATUS_MISSED: return "Missed";
    case STATUS_AWAY: return "Away";
  }
  return "Unknown";
}

DHTnode *
DHTnode::clone(void)
{
  DHTnode *cl = new DHTnode();

  cl->_ether_addr = _ether_addr;
  cl->_status = _status;
  cl->_extra = _extra;                               //TODO: better copy for extra element of dhtnode
  cl->_age = _age;
  cl->_last_ping = _last_ping;
  cl->_failed_ping = _failed_ping;
  cl->_rtt = _rtt;
  cl->_hop_distance = _hop_distance;
  cl->_neighbor = _neighbor;

  memcpy(cl->_md5_digest, _md5_digest, sizeof(_md5_digest));
  cl->_digest_length = _digest_length;

  return cl;
}

bool
DHTnode::equals(DHTnode *n) {
  if ( n == NULL) return false;

  return ( (_digest_length == n->_digest_length) && (memcmp(_md5_digest, n->_md5_digest, 16) == 0) );
}

bool
DHTnode::equalsID(DHTnode *n) {
  if ( n == NULL) return false;

  return ( (_digest_length == n->_digest_length) && (memcmp(_md5_digest, n->_md5_digest, 16) == 0) );
}

bool
DHTnode::equalsEtherAddress(DHTnode *n) {
  if ( n == NULL) return false;

  return ( memcmp(_ether_addr.data(), n->_ether_addr.data(), 6) == 0 );
}

CLICK_ENDDECLS

ELEMENT_PROVIDES(DHTnode)
