#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>

#include "clustering.hh"

CLICK_DECLS

Clustering::Clustering()
{
}

Clustering::~Clustering()
{
}

void
Clustering::init()
{
  BRNElement::init();
}

String
Clustering::clustering_info()
{
  StringAccum sa;
  sa << "Clusterhead: " << _clusterhead.unparse();

  return sa.take_string();
}

static String
read_clustering_info(Element *e, void */*thunk*/)
{
  return ((Clustering *)e)->clustering_info();
}

static int
write_clusterhead(const String &in_s, Element *e, void */*vparam*/, ErrorHandler */*errh*/)
{
  EtherAddress ch;
  Clustering *f = (Clustering *)e;
  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  cp_ethernet_address(args[0], &ch); //TODO: exception handle
  f->_clusterhead = ch;

  return 0;
}

void
Clustering::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("clustering_info", read_clustering_info, (void *) 0);
  add_write_handler("clusterhead", write_clusterhead, (void *) 0);
}

ELEMENT_PROVIDES(Clustering)
CLICK_ENDDECLS
