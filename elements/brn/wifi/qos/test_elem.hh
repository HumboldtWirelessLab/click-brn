#ifndef TEST_ELEM_HH
#define TEST_ELEM_HH

#include <click/element.hh>
#include <elements/brn/brnelement.hh>

CLICK_DECLS

class TestElem : public BRNElement {

  public:

    TestElem();

    const char *class_name() const  { return "TestElem"; }
    const char *port_count() const  { return "1/1"; }
    const char *processing() const  { return "a"; }

    int configure(Vector<String> &, ErrorHandler *);
    void add_handlers();
    Packet *simple_action(Packet *p);

    void test();

};



CLICK_ENDDECLS


#endif
