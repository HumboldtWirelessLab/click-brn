#include <click/config.h>
#include <click/glue.hh>

#include "fakeopenssl.hh"

#if HAVE_OPENSSL

#include <click/element.hh>
#include <click/confparse.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/brn2.h"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/brnprotocol/brnprotocol.hh"

#include "kdp.hh"
#include "keymanagement.hh"

CLICK_DECLS

FakeOpenSSL::FakeOpenSSL()
{
}

FakeOpenSSL::~FakeOpenSSL() {
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(FakeOpenSSL)
ELEMENT_LIBS(-lssl -lcrypto)

#endif
