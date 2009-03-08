#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <click/vector.hh>

#include "dhtstoragetest.hh"
#include "elements/brn2/dht/storage/dhtoperation.hh"
#include "elements/brn2/dht/storage/dhtstorage.hh"

CLICK_DECLS

DHTStorageTest::DHTStorageTest():
  _dht_storage(NULL),
  _request_timer(static_request_timer_hook,this),
  _debug(0),
  write_req(0),
  write_rep(0),
  read_req(0),
  read_rep(0)
{
}

DHTStorageTest::~DHTStorageTest()
{
}

int DHTStorageTest::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _interval = 1000;
  _write = false;
  _countkey = 100;

  if (cp_va_kparse(conf, this, errh,
    "DHTSTORAGE", cpkN, cpElement, &_dht_storage,
    "STARTTIME",cpkN, cpInteger, &_starttime,
    "INTERVAL",cpkN, cpInteger, &_interval,
    "COUNTKEYS", cpkN, cpInteger, &_countkey,
    "WRITE",cpkN, cpBool, &_write,
    "DEBUG", cpkN, cpInteger, &_debug,
    cpEnd) < 0)
      return -1;

  if (!_dht_storage || !_dht_storage->cast("DHTStorage"))
  {
    _dht_storage = NULL;
    click_chatter("No Storage");
  }

  return 0;
}

int DHTStorageTest::initialize(ErrorHandler *)
{
  _request_timer.initialize(this);
  _request_timer.schedule_after_msec( _starttime + ( click_random() % _interval ) );

  _key = 0;
  if ( _write )
    _mode = MODE_INSERT;
  else
    _mode = MODE_READ;

  return 0;
}

static void callback_func(void *e, DHTOperation *op)
{
  DHTStorageTest *s = (DHTStorageTest *)e;
  char string[100];
  uint32_t my_key;

//  click_chatter("callback %s: Status %d",s->class_name(),op->header.operation);
  memcpy(string, op->value, op->header.valuelen);
  string[op->header.valuelen] = '\0';
  memcpy((char*)&my_key, op->key, sizeof(uint32_t));

  if ( op->is_reply() )
  {
    if ( op->header.status == DHT_STATUS_OK ) {
      if ( (op->header.operation & ( /*(uint8_t)!((uint8_t)OPERATION_INSERT)*/127 )) == OPERATION_INSERT )
        s->write_rep++;
      else
        s->read_rep++;
      click_chatter("Result: %s = %d",string,my_key);
    } else
      click_chatter("Result: %d not found",my_key);
  }

  delete op;
}

void
DHTStorageTest::static_request_timer_hook(Timer *t, void *f)
{
  DHTOperation *req;
  int result;
  char *my_value;

  DHTStorageTest *s = (DHTStorageTest *)f;

  req = new DHTOperation();

  if ( s->_mode == MODE_INSERT )
  {
    s->write_req++;
    my_value = new char[10];
    sprintf(my_value,">%d<",s->_key);

    click_chatter("Insert Key: %d",s->_key);
    req->insert((uint8_t*)&s->_key, sizeof(uint32_t), (uint8_t*)my_value, strlen(my_value));
    delete[] my_value;
    s->_key++;
    if ( s->_key == s->_countkey )
    {
      s->_key = 0;
      s->_mode = MODE_READ;
    }
  }
  else
  {
    s->read_req++;
    click_chatter("Read Key: %d",s->_key);
    req->read((uint8_t*)&s->_key, sizeof(uint32_t));
    s->_key++;
    if ( s->_key == s->_countkey )
      s->_key = 0;
  }

  result = s->_dht_storage->dht_request(req, callback_func, (void*)s );

  if ( result == 0 )
  {
    click_chatter("Got direct-reply (local)");
    callback_func(s,req);
  }
  t->schedule_after_msec( s->_interval );
}

enum {
  H_STORAGE_STATS
};

static String
    read_param(Element *e, void *thunk)
{
  StringAccum sa;
  DHTStorageTest *dht_str = (DHTStorageTest *)e;

  switch ((uintptr_t) thunk)
  {
    case H_STORAGE_STATS :
    {
      sa << "READ-request: " << dht_str->read_req << "\n";
      sa << "READ-reply: " << dht_str->read_rep << "\n";
      sa << "WRITE-request: " << dht_str->write_req << "\n";
      sa << "WRITE-reply: " << dht_str->write_rep << "\n";
      return sa.take_string();
    }
    default: return String();
  }
}

void DHTStorageTest::add_handlers()
{
  add_read_handler("stats", read_param , (void *)H_STORAGE_STATS);
}

#include <click/vector.cc>

CLICK_ENDDECLS
EXPORT_ELEMENT(DHTStorageTest)
