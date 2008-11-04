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
#include "elements/brn/dht/storage/dhtoperation.hh"
#include "elements/brn/dht/storage/dhtstorage.hh"

CLICK_DECLS

DHTStorageTest::DHTStorageTest():
  _dht_storage(NULL),
  _request_timer(static_request_timer_hook,this),
  _debug(0)
{
}

DHTStorageTest::~DHTStorageTest()
{
}

int DHTStorageTest::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _interval = 1000;

  if (cp_va_kparse(conf, this, errh,
    "DHTSTORAGE", cpkN, cpElement, &_dht_storage,
    "STARTIME", cpkN, cpInteger, &_starttime,
    "COUNTKEYS", cpkN, cpInteger, &_countkey,
    "INTERVAL",cpkN, cpInteger, &_interval,
    "STARTTIME",cpkN, cpInteger, &_starttime,
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
  return 0;
}

static void callback_func(void *e, DHTOperation *op)
{
  DHTStorageTest *s = (DHTStorageTest *)e;

  click_chatter("callback %s: Status %d",s->class_name(),op->operation);
}

void
DHTStorageTest::static_request_timer_hook(Timer *t, void *f)
{
  DHTOperation *req;
  int result;
  char *my_value;
  char *my_key;

  DHTStorageTest *s = (DHTStorageTest *)f;

  my_key = new char[sizeof(uint32_t)];
  memcpy(my_key, (char*)&s->_key, sizeof(uint32_t));
  my_value = new char[10];
  sprintf(my_value,"%d",s->_key);
  click_chatter("Request: %d",s->_key++);

  req = new DHTOperation();
  req->insert(my_key, sizeof(uint32_t), my_value, strlen(my_value));

  result = s->_dht_storage->dht_request(req, callback_func, (void*)s );

  t->schedule_after_msec( s->_interval );

}

void DHTStorageTest::add_handlers()
{
}

#include <click/vector.cc>

CLICK_ENDDECLS
EXPORT_ELEMENT(DHTStorageTest)
