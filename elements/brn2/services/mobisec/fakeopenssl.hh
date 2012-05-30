#ifndef FAKEOPENSSL_HH_
#define FAKEOPENSSL_HH_

#if HAVE_OPENSSL

#include <string>
#include <time.h>

#include <click/element.hh>
#include <click/confparse.hh>
#include <click/hashmap.hh>
#include "elements/brn2/brnelement.hh"

CLICK_DECLS

class FakeOpenSSL : public BRNElement {
public:
  FakeOpenSSL();
  ~FakeOpenSSL();

  const char *class_name() const { return "FakeOpenSSL"; }

};

CLICK_ENDDECLS
#endif
#endif
