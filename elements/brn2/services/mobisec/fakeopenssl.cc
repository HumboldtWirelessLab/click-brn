#include <click/config.h>
#include <click/glue.hh>

#include "fakeopenssl.hh"

#if defined(HAVE_OPENSSL)

CLICK_DECLS

FakeOpenSSL::FakeOpenSSL()
{
}

FakeOpenSSL::~FakeOpenSSL() {
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns)
ELEMENT_PROVIDES(FakeOpenSSL)
ELEMENT_LIBS(-lssl -lcrypto)

#endif
