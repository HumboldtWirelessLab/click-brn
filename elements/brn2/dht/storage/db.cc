#include <click/config.h>
#include "db.hh"

CLICK_DECLS

BRNDB::BRNDB()
{
}

BRNDB::~BRNDB()
{
}

int
BRNDB::insert(md5_byte_t *md5_key, uint8_t *key, uint16_t keylen, uint8_t *value, uint16_t valuelen, int lock, char *lock_node)
{
  DBrow *_new_row;

  _new_row = new DBrow();

  _new_row->md5_key = new md5_byte_t[16];
  memcpy(_new_row->md5_key,md5_key, 16);

  _new_row->key = new uint8_t[keylen];
  memcpy(_new_row->key,key,keylen);
  _new_row->keylen = keylen;

  _new_row->value = new uint8_t[valuelen];
  memcpy(_new_row->value,value,valuelen);
  _new_row->valuelen = valuelen;

  _new_row->lock = lock;
  memcpy(_new_row->lock_node, lock_node, 6);

  _table.push_back(_new_row);

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
BRNDB::size()
{
  return _table.size();
}

#include <click/vector.cc>
template class Vector<BRNDB::DBrow*>;

CLICK_ENDDECLS
ELEMENT_PROVIDES(BRNDB)
