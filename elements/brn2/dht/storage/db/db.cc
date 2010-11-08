#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <click/vector.hh>

#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "db.hh"

CLICK_DECLS

BRNDB::BRNDB()
{
  BRNElement::init();
}

BRNDB::~BRNDB()
{
}

int BRNDB::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int BRNDB::initialize(ErrorHandler *)
{
  return 0;
}

int
BRNDB::insert(md5_byte_t *md5_key, uint8_t *key, uint16_t keylen, uint8_t *value, uint16_t valuelen, uint8_t replica)
{
  DBrow *_new_row;

  _new_row = new DBrow();

  memcpy(_new_row->md5_key,md5_key, 16);

  _new_row->key = new uint8_t[keylen];
  memcpy(_new_row->key,key,keylen);
  _new_row->keylen = keylen;

  _new_row->value = new uint8_t[valuelen];
  memcpy(_new_row->value,value,valuelen);
  _new_row->valuelen = valuelen;

  _new_row->replica = replica;

  _table.push_back(_new_row);

  return 0;
}

int
BRNDB::insert(BRNDB::DBrow *row)
{
  _table.push_back(row);
  return 0;
}



BRNDB::DBrow *
BRNDB::getRow(char *key, uint16_t keylen)
{
  DBrow *_ac_row;

  if ( key != NULL && keylen > 0 && _table.size() > 0 )
  {
    for ( int i = 0; i < _table.size(); i++ )
    {
      _ac_row = _table[i];

      if ( ( _ac_row->keylen == keylen ) && ( memcmp((uint8_t*)key, _ac_row->key, keylen) == 0 ) )
      {
        return _ac_row;
      }
    }
  }

  return NULL;
}

BRNDB::DBrow *
BRNDB::getRow(md5_byte_t *md5_key)
{
  DBrow *_ac_row;

  if ( md5_key != NULL && _table.size() > 0 )
  {
    for ( int i = 0; i < _table.size(); i++ )
    {
      _ac_row = _table[i];

      if ( MD5::hexcompare(_ac_row->md5_key, md5_key) == 0 )
        return _ac_row;
    }
  }

  return NULL;
}

BRNDB::DBrow *
BRNDB::getRow(int index) {
  if ( _table.size() > index ) return _table[index];

  return NULL;
}

int
BRNDB::delRow(md5_byte_t *md5_key) {
  DBrow *_ac_row;

  if ( md5_key != NULL && _table.size() > 0 )
  {
    for ( int i = 0; i < _table.size(); i++ )
    {
      _ac_row = _table[i];
      if ( ! _ac_row->isLocked() ) {
        _table.erase(_table.begin() + i);
        delete _ac_row;
        return 0;
      } else {
        return 1;
      }
    }
  }

  return 0;
}

int
BRNDB::delRow(int index) {
  delete _table[index];
  _table.erase(_table.begin() + index);
  return 0;
}

int
BRNDB::size()
{
  return _table.size();
}

/*************************************************************************************************/
/***************************************** H A N D L E R *****************************************/
/*************************************************************************************************/

void BRNDB::add_handlers()
{
  BRNElement::add_handlers();
}

#include <click/vector.cc>
template class Vector<BRNDB::DBrow*>;

CLICK_ENDDECLS
EXPORT_ELEMENT(BRNDB)
