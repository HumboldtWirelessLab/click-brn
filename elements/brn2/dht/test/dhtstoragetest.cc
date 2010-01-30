#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <click/vector.hh>

#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "elements/brn2/dht/storage/dhtoperation.hh"
#include "elements/brn2/dht/storage/dhtstorage.hh"
#include "dhtstoragetest.hh"

CLICK_DECLS

DHTStorageTest::DHTStorageTest():
  _dht_storage(NULL),
  _debug(BrnLogger::DEFAULT),
  _request_timer(static_request_timer_hook,this),
  op_rep(0),
  write_req(0),
  write_rep(0),
  read_req(0),
  read_rep(0),
  not_found(0),
  no_timeout(0),
  op_time(0),
  write_time(0),
  read_time(0),
  notfound_time(0),
  timeout_time(0),
  max_timeout_time(0)
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

void
DHTStorageTest::callback_func(void *e, DHTOperation *op)
{
  DHTStorageTest *s = (DHTStorageTest *)e;
  s->callback(op);
}

void
DHTStorageTest::callback(DHTOperation *op) {
  char string[100];
  uint32_t my_key;

  BRN_DEBUG("callback %s: Status %d",class_name(),op->header.operation);

  memcpy(string, op->value, op->header.valuelen);
  string[op->header.valuelen] = '\0';
  memcpy((char*)&my_key, op->key, sizeof(uint32_t));

  if ( op->is_reply() )
  {
    op_rep++;
    op_time += op->request_duration;

    if ( op->header.status == DHT_STATUS_OK ) {
      if ( (op->header.operation & ( (uint8_t)~((uint8_t)OPERATION_REPLY))) == OPERATION_INSERT ) { //TODO: it should make sure that it is only insert !!
        write_rep++;
        write_time += op->request_duration;
      } else {
        read_rep++;
        read_time += op->request_duration;
      }
      BRN_DEBUG("Result: %s = %d",string,my_key);
    } else {
      if ( op->header.status == DHT_STATUS_TIMEOUT ) {
        no_timeout++;
        timeout_time += op->request_duration;
        if ( max_timeout_time < op->request_duration ) max_timeout_time = op->request_duration;

        BRN_DEBUG("DHT-Test: Timeout");
      } else {
        not_found++;
        notfound_time += op->request_duration;
        BRN_DEBUG("Result: %d not found",my_key);
      }
    }
  }

  delete op;
}

void
DHTStorageTest::static_request_timer_hook(Timer *t, void *f)
{
  DHTStorageTest *s = (DHTStorageTest *)f;
  s->request_timer_hook(t);
}

void
DHTStorageTest::request_timer_hook(Timer *t)
{
  DHTOperation *req;
  int result;
  char *my_value;

  req = new DHTOperation();

  if ( _mode == MODE_INSERT )
  {
    BRN_DEBUG("Insert Key: %d",_key);

    write_req++;
    my_value = new char[10];
    sprintf(my_value,">%d<",_key);

    req->insert((uint8_t*)&_key, sizeof(uint32_t), (uint8_t*)my_value, strlen(my_value));
    req->max_retries = 1;
    delete[] my_value;
    _key++;

    if ( _key == _countkey )
    {
      _key = 0;
      _mode = MODE_READ;
    }
  }
  else
  {
    read_req++;
    BRN_DEBUG("Read Key: %d",_key);
    req->read((uint8_t*)&_key, sizeof(uint32_t));
    req->max_retries = 1;
    _key = (_key + 1) % _countkey;
  }

  result = _dht_storage->dht_request(req, callback_func, (void*)this );

  if ( result == 0 )
  {
    BRN_DEBUG("Got direct-reply (local)");
    callback_func((void*)this,req);
  }
  t->schedule_after_msec( _interval );
}


//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

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

      int avg_op_time, avg_read_time, avg_write_time, avg_notf_time, avg_to_time;
      avg_op_time = avg_read_time = avg_write_time = avg_notf_time = avg_to_time = 0;

      if ( dht_str->op_rep != 0 ) avg_op_time = dht_str->op_time/dht_str->op_rep;
      if ( dht_str->read_rep != 0 ) avg_read_time = dht_str->read_time/dht_str->read_rep;
      if ( dht_str->write_rep != 0 ) avg_write_time = dht_str->write_time/dht_str->write_rep;
      if ( dht_str->not_found != 0 ) avg_notf_time = dht_str->notfound_time/dht_str->not_found;
      if ( dht_str->no_timeout != 0 ) avg_to_time = dht_str->timeout_time/dht_str->no_timeout;

      sa << "Operation-Reply: " << dht_str->op_rep << " (Avg. time: " << avg_op_time << " ms )\n";
      sa << "READ-request: " << dht_str->read_req << "\n";
      sa << "READ-reply: " << dht_str->read_rep << " (Avg. time: " << avg_read_time << " ms )\n";
      sa << "WRITE-request: " << dht_str->write_req << "\n";
      sa << "WRITE-reply: " << dht_str->write_rep << " (Avg. time: " << avg_write_time << " ms )\n";
      sa << "Not-Found: " << dht_str->not_found << " (Avg. time: " << avg_notf_time << " ms )\n";
      sa << "Timeout: " << dht_str->no_timeout << " (Avg. time: " << avg_to_time << " ms  Max. time: ";
      sa << dht_str->max_timeout_time << " ms )\n";
      return sa.take_string();
    }
    default: return String();
  }
}

static String
read_debug_param(Element *e, void *)
{
  DHTStorageTest *dt = (DHTStorageTest *)e;
  return String(dt->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  DHTStorageTest *dt = (DHTStorageTest *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug))
    return errh->error("debug parameter must be an integer value between 0 and 4");
  dt->_debug = debug;
  return 0;
}

void DHTStorageTest::add_handlers()
{
  add_read_handler("stats", read_param , (void *)H_STORAGE_STATS);
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

#include <click/vector.cc>

CLICK_ENDDECLS
EXPORT_ELEMENT(DHTStorageTest)
