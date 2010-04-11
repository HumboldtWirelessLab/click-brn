#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>

#include "dhtrouting.hh"

CLICK_DECLS

DHTRouting::DHTRouting()
{
}

DHTRouting::~DHTRouting()
{
}

int
DHTRouting::change_node_id(md5_byte_t */*key*/, int /*keylen*/)
{
  return CHANGE_NODE_ID_STATUS_NOT_POSSIBLE;
}


enum {H_NODE_ID};

static String
read_node_id(Element *e, void *thunk)
{
  DHTRouting *dhtr = (DHTRouting *)e;
  switch ((uintptr_t) thunk) {
    case H_NODE_ID:
      char digest[16*2 + 1];
      MD5::printDigest(dhtr->_me->_md5_digest, digest);
      return String(digest) + " " + String(dhtr->_me->_digest_length) + "\n";
    default:
      return String();
  }
}

static int
write_node_id(const String &in_s, Element *e, void *vparam, ErrorHandler */*errh*/)
{
  DHTRouting *f = (DHTRouting *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_NODE_ID: {
      md5_byte_t md5_digest[MAX_NODEID_LENTGH];
      int keylen;
      Vector<String> args;
      cp_spacevec(s, args);

      MD5::digestFromString(md5_digest, args[0].c_str());
      cp_integer(args[1], &keylen);

      int result = f->change_node_id(md5_digest, keylen);
      click_chatter("Result: %d",result);

      break;
    }
  }
  return 0;
}

void
DHTRouting::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("node_id", read_node_id, (void *) H_NODE_ID);
  add_write_handler("node_id", write_node_id, (void *) H_NODE_ID);
}

ELEMENT_PROVIDES(DHTRouting)
CLICK_ENDDECLS
