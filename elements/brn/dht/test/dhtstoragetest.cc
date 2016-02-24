#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <click/vector.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "elements/brn/dht/storage/dhtoperation.hh"
#include "elements/brn/dht/storage/dhtstorage.hh"
#include "dhtstoragetest.hh"

CLICK_DECLS

DHTStorageTest::DHTStorageTest():
  _dht_storage(NULL),
  _key(0),_interval(0),_mode(0),
  _request_timer(static_request_timer_hook,this),
  _starttime(0),
  _startkey(0),
  _countkey(0),
  _write(false),
  _read(true),
  op_rep(0),
  write_req(0),
  write_rep(0),
  read_req(0),
  read_rep(0),
  append_req(0),
  append_rep(0),
  not_found(0),
  no_timeout(0),
  op_time(0),
  write_time(0),
  read_time(0),
  append_time(0),
  notfound_time(0),
  timeout_time(0),
  max_timeout_time(0),
  _retries(0),
  _replica(0),
  last_read(false),
  last_timeout(false),
  last_not_found(false)
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
  click_brn_srandom();

  _request_timer.initialize(this);

  if ( _starttime > 0 ) {
    _request_timer.schedule_after_msec( _starttime + ( click_random() % _interval ) );

    _key = _startkey;
    if ( _write )
      _mode = MODE_INSERT;
    else
      _mode = MODE_READ;
  }

  last_key = "n/a";
  last_read = true;
  last_timeout = false;
  last_not_found = false;
  last_value = "n/a";

  return 0;
}

void
DHTStorageTest::callback_func(void *e, DHTOperation *op)
{
  DHTStorageTest *s = reinterpret_cast<DHTStorageTest *>(e);
  s->callback(op);
}

void
DHTStorageTest::callback(DHTOperation *op) {
  String value;
  String key;

  BRN_DEBUG("callback %s: Status %d",class_name(),op->header.operation);

  value = String(op->value, op->header.valuelen);
  key = String(op->key, op->header.keylen);

  last_key = key;
  last_value = value;

  if ( op->is_reply() )
  {
    op_rep++;
    op_time += op->request_duration;

    if ( op->header.status == DHT_STATUS_OK ) {
      if ( (op->header.operation & ( (uint8_t)~((uint8_t)OPERATION_REPLY))) == OPERATION_INSERT ) { //TODO: it should make sure that it is only insert !!
        write_rep++;
        write_time += op->request_duration;
        last_read = false;
      } else if ( (op->header.operation & ( (uint8_t)~((uint8_t)OPERATION_REPLY))) == OPERATION_READ ) {
        read_rep++;
        read_time += op->request_duration;
        last_read = true;
      } else {
        append_rep++;
        append_time += op->request_duration;
        last_read = false;
      }

      last_timeout = false;
      last_not_found = false;

      BRN_DEBUG("Result: %s = %s",value.c_str(), key.c_str());
    } else {
      if ( op->header.status == DHT_STATUS_TIMEOUT ) {
        no_timeout++;
        timeout_time += op->request_duration;
        if ( max_timeout_time < op->request_duration ) max_timeout_time = op->request_duration;

        BRN_DEBUG("DHT-Test: Timeout: %s", value.c_str());
        last_timeout = true;
      } else {
        not_found++;
        notfound_time += op->request_duration;
        BRN_DEBUG("Result: %s not found",key.c_str());
        last_timeout = false;
      }

      last_value = "n/a";
      last_not_found = true;
      last_read = ! ((op->header.operation & ( (uint8_t)~((uint8_t)OPERATION_REPLY))) == OPERATION_INSERT );
    }
  }

  delete op;
}

void
DHTStorageTest::static_request_timer_hook(Timer *t, void *f)
{
  DHTStorageTest *s = reinterpret_cast<DHTStorageTest *>(f);
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
  String s_key = String(key);
  String s_value = ">" + String(key) + "<";

  request(s_key, s_value, mode);
}

void
DHTStorageTest::request(String key, String value, uint8_t mode)
{
  DHTOperation *req;
  int result;

  req = new DHTOperation();

  last_key = key;
  last_value = value;
  last_timeout = false;
  last_not_found = true;

  if ( mode == MODE_INSERT )
  {
    BRN_DEBUG("Insert Key: %s",key.c_str());

    write_req++;
    req->insert((uint8_t*)key.data(), key.length(), (uint8_t*)value.data(), value.length());
    req->max_retries = _retries;
    req->set_replica(_replica);

    last_read = false;
  }
  else if ( mode == MODE_READ )
  {
    BRN_DEBUG("Read Key: %s",key.c_str());

    read_req++;

    req->read((uint8_t*)key.data(), key.length());
    req->max_retries = _retries;
    req->set_replica(_replica);

    last_read = true;
    last_value = "n/a";
  }
  else //APPEND
  {
    BRN_DEBUG("Append Key: %s",key.c_str());

    append_req++;

    req->append((uint8_t*)key.data(), key.length(), (uint8_t*)value.data(), value.length());
    req->max_retries = _retries;
    req->set_replica(_replica);

    last_read = true;
    last_value = "n/a";
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
  H_USER_TEST,
  H_RETRIES
};

String
DHTStorageTest::print_stats()
{
  StringAccum sa;

  int avg_op_time, avg_read_time, avg_write_time, avg_append_time, avg_notf_time, avg_to_time;
  avg_op_time = avg_read_time = avg_write_time = avg_append_time = avg_notf_time = avg_to_time = 0;

  if ( op_rep != 0 ) avg_op_time = op_time/op_rep;
  if ( read_rep != 0 ) avg_read_time = read_time/read_rep;
  if ( write_rep != 0 ) avg_write_time = write_time/write_rep;
  if ( append_rep != 0 ) avg_append_time = append_time/append_rep;
  if ( not_found != 0 ) avg_notf_time = notfound_time/not_found;
  if ( no_timeout != 0 ) avg_to_time = timeout_time/no_timeout;

  sa << "<dhtstoragetest node=\"" << BRN_NODE_NAME << "\" replies=\"" << op_rep << "\" avg_time=\"";
  sa << avg_op_time << "\" unit=\"ms\" >\n";
  sa << "\t<read requests=\"" << read_req << "\" replies=\"" << read_rep;
  sa << "\" avg_time=\"" << avg_read_time << "\" />\n";
  sa << "\t<write requests=\"" << write_req << "\" replies=\"" << write_rep;
  sa << "\" avg_time=\"" << avg_write_time << "\" />\n";
  sa << "\t<append requests=\"" << append_req << "\" replies=\"" << append_rep;
  sa << "\" avg_time=\"" << avg_append_time << "\" />\n";
  sa << "\t<not_found count=\"" << not_found << "\" avg_time=\"" << avg_notf_time << "\" />\n";
  sa << "\t<timeout count=\"" << no_timeout << "\" avg_time=\"" << avg_to_time << "\"  max_time=\"";
  sa << max_timeout_time << "\" />\n</dhtstoragetest>\n";

  return sa.take_string();
}

String
DHTStorageTest::print_test_results()
{
  StringAccum sa;

  sa << "<dhtstoragetestoperation node=\"" << BRN_NODE_NAME << "\" key=\"" << last_key << "\" value=\"" << last_value;
  if ( last_read ) sa << "\" mode=\"read\" ";
  else sa << "\" mode=\"write\" ";
  if ( last_timeout ) sa << "timeout=\"yes\" ";
  else sa << "timeout=\"no\" ";
  if ( last_not_found ) sa << "found=\"no\" ";
  else sa << "found=\"yes\" ";
  sa << "/>\n";

  return sa.take_string();
}

static String
read_param(Element *e, void *thunk)
{
  DHTStorageTest *dht_str = reinterpret_cast<DHTStorageTest *>(e);

  switch ((uintptr_t) thunk)
  {
    case H_STORAGE_STATS : return dht_str->print_stats();
    case H_USER_TEST: return dht_str->print_test_results();
    case H_RETRIES: return String(dht_str->_retries);
  }

  return String();
}

static int
write_param(const String &in_s, Element *e, void *thunk, ErrorHandler *errh)
{
  DHTStorageTest *dht_str = reinterpret_cast<DHTStorageTest *>(e);

  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  switch ((uintptr_t) thunk)
  {
    case H_USER_TEST: {
      String key;
      String value;

      if ( args.size() > 1 ) {
        uint8_t mode;
        if ( args[0] == String("write") ) mode = MODE_INSERT;
        else if (args[0] == String("read")) mode = MODE_READ;
        else mode = MODE_APPEND;
        key = args[1];
        if ( args.size() > 2 ) value = args[2];
        else value = ">" + args[1] + "<"; //=key
        dht_str->request(key, value, mode);
      }
      break;
    }
    case H_RETRIES: {
      int ret;
      if (!cp_integer(args[0], &ret)) return errh->error("retries parameter must be integer");
      dht_str->_retries = ret;
      break;
    }
  }

  return 0;
}

void DHTStorageTest::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", read_param, (void *)H_STORAGE_STATS);
  add_read_handler("test", read_param, (void *)H_USER_TEST);
  add_read_handler("retries", read_param, (void *)H_RETRIES);

  add_write_handler("retries", write_param, (void *)H_RETRIES);
  add_write_handler("test", write_param, (void *)H_USER_TEST);

}

#include <click/vector.cc>

CLICK_ENDDECLS
EXPORT_ELEMENT(DHTStorageTest)
