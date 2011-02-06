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
  _request_timer(static_request_timer_hook,this),
  _starttime(0),
  _startkey(0),
  _write(false),
  _read(true),
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
  max_timeout_time(0),
  _retries(0),
  _replica(0)

{
  BRNElement::init();
}

DHTStorageTest::~DHTStorageTest()
{
}

int DHTStorageTest::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _interval = 1000;
  _countkey = 100;

  if (cp_va_kparse(conf, this, errh,
    "DHTSTORAGE", cpkP+cpkM, cpElement, &_dht_storage,
    "STARTTIME", cpkP+cpkM, cpInteger, &_starttime,
    "INTERVAL",cpkN, cpInteger, &_interval,
    "COUNTKEYS", cpkN, cpInteger, &_countkey,
    "STARTKEY", cpkN, cpInteger, &_startkey,
    "WRITE",cpkN, cpBool, &_write,
    "READ",cpkN, cpBool, &_read,
    "RETRIES", cpkN, cpInteger, &_retries,
    "REPLICA", cpkN, cpInteger, &_replica,
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
  click_srandom(_dht_storage->_dht_routing->_me->_ether_addr.hashcode());

  _request_timer.initialize(this);

  if ( _starttime > 0 ) {
    _request_timer.schedule_after_msec( _starttime + ( click_random() % _interval ) );

    _key = _startkey;
    if ( _write )
      _mode = MODE_INSERT;
    else
      _mode = MODE_READ;
  }

  last_key = 0;
  last_read = true;
  last_timeout = false;
  last_not_found = false;
  sprintf(last_value,"NONE");

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
  my_key = ntohl(my_key);

  last_key = my_key;
  sprintf(last_value,"%s",string);

  if ( op->is_reply() )
  {
    op_rep++;
    op_time += op->request_duration;

    if ( op->header.status == DHT_STATUS_OK ) {
      if ( (op->header.operation & ( (uint8_t)~((uint8_t)OPERATION_REPLY))) == OPERATION_INSERT ) { //TODO: it should make sure that it is only insert !!
        write_rep++;
        write_time += op->request_duration;
        last_read = false;
      } else {
        read_rep++;
        read_time += op->request_duration;
        last_read = true;
      }

      last_timeout = false;
      last_not_found = false;

      BRN_DEBUG("Result: %s = %d",string,my_key);
    } else {
      if ( op->header.status == DHT_STATUS_TIMEOUT ) {
        no_timeout++;
        timeout_time += op->request_duration;
        if ( max_timeout_time < op->request_duration ) max_timeout_time = op->request_duration;

        BRN_DEBUG("DHT-Test: Timeout: %s", string);
        last_timeout = true;
      } else {
        not_found++;
        notfound_time += op->request_duration;
        BRN_DEBUG("Result: %d not found",my_key);
        last_timeout = false;
      }

      sprintf(last_value,"FAILED");
      last_not_found = true;
      last_read = ! ((op->header.operation & ( (uint8_t)~((uint8_t)OPERATION_REPLY))) == OPERATION_INSERT );
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

  if ((_mode != MODE_INSERT) && ! _read ) return;

  request(_key, _mode);

  if ( _mode == MODE_INSERT )
  {
    _key++;

    if ( _key == (_startkey + _countkey) )
    {
      _key = _startkey;
      _mode = MODE_READ;
    }
  } else {
    _key = _startkey + ( (_key - _startkey + 1) % _countkey );
  }

  t->schedule_after_msec( _interval );

}

void
DHTStorageTest::request(uint32_t key, uint8_t mode)
{
  DHTOperation *req;
  int result;
  char my_value[10];
  uint32_t net_key = htonl(key);

  req = new DHTOperation();

  last_key = key;
  last_timeout = false;
  last_not_found = false;

  if ( mode == MODE_INSERT )
  {
    BRN_DEBUG("Insert Key: %d",key);

    write_req++;
    sprintf(my_value,">%d<",key);

    req->insert((uint8_t*)&net_key, sizeof(uint32_t), (uint8_t*)my_value, strlen(my_value));
    req->max_retries = _retries;
    req->set_replica(_replica);

    last_read = false;
    sprintf(last_value,">%d<",key);
  }
  else
  {
    BRN_DEBUG("Read Key: %d",key);

    read_req++;

    req->read((uint8_t*)&net_key, sizeof(uint32_t));
    req->max_retries = _retries;
    req->set_replica(_replica);

    last_read = true;
    sprintf(last_value,"NONE");
  }

  result = _dht_storage->dht_request(req, callback_func, (void*)this );

  if ( result == 0 )
  {
    BRN_DEBUG("Got direct-reply (local)");
    callback_func((void*)this,req);
  }
}



//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

enum {
  H_STORAGE_STATS,
  H_USER_TEST
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
      break;
    }
    case H_USER_TEST:
    {
      sa << "last key: " << dht_str->last_key << " value: " << String(dht_str->last_value);
      if ( dht_str->last_read ) sa << "mode: read ";
      else sa << "mode: write ";
      if ( dht_str->last_timeout ) sa << "timeout: yes ";
      else sa << "timeout: no ";
      if ( dht_str->last_not_found ) sa << "found: yes ";
      else sa << "found: no ";
      sa << "\n";
      break;
    }
    default: return String();
  }

  return sa.take_string();
}

static int
write_param(const String &in_s, Element *e, void */*thunk*/, ErrorHandler */*errh*/)
{
  DHTStorageTest *dht_str = (DHTStorageTest *)e;

  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  uint32_t key;
  uint8_t mode;

  if ( args.size() == 2 ) {
    if ( args[0] == String("write") ) mode = MODE_INSERT;
    else mode = MODE_READ;
    cp_integer(args[1], &key);
    dht_str->request(key, mode);
  }

  return 0;
}

void DHTStorageTest::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", read_param, (void *)H_STORAGE_STATS);
  add_read_handler("test", read_param, (void *)H_USER_TEST);

  add_write_handler("test", write_param, (void *)H_USER_TEST);

}

#include <click/vector.cc>

CLICK_ENDDECLS
EXPORT_ELEMENT(DHTStorageTest)
