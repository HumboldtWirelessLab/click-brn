#ifndef FAKEOPENSSL_HH_
#define FAKEOPENSSL_HH_

#if HAVE_OPENSSL

CLICK_DECLS

class FakeOpenSSL {
public:
  FakeOpenSSL();
  ~FakeOpenSSL();
};

CLICK_ENDDECLS
#endif
#endif
