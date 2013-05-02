#ifndef ROUTING_PEEK_HH
#define ROUTING_PEEK_HH
#include <click/element.hh>
#include <click/etheraddress.hh>

#include "elements/brn/brnelement.hh"

CLICK_DECLS

class RoutingPeek : public BRNElement
{
  public:

    class PeekFunction {

      public:
        bool (*_peek_func)(void *, Packet *p, EtherAddress *src, EtherAddress *dst, int brn_port);  //Packet, brn_port //TODO: add src and dst etheraddress
        void *_peek_obj;
        int _brn_port;

        PeekFunction( bool (*peek_func)(void *, Packet *p, EtherAddress *src, EtherAddress *dst, int brn_port), void *peek_obj, int brn_port ) {
          _peek_func = peek_func;
          _peek_obj = peek_obj;
          _brn_port = brn_port;
        }

        ~PeekFunction(){}
    };

    Vector<PeekFunction*> _peeklist;

    RoutingPeek();
    ~RoutingPeek();

    void init();

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

    void add_handlers();

    virtual const char *routing_name() const = 0; //const : function doesn't change the object (members).
                                                   //virtual: sp�te Bindung


    int add_routing_peek(bool (*peek_func)(void*, Packet*, EtherAddress *, EtherAddress *, int), void *peek_obj, int brn_port)
    {
      if ( ( peek_func == NULL ) || ( peek_obj == NULL ) ) return -1;
      _peeklist.push_back(new PeekFunction(peek_func, peek_obj, brn_port));
      return 0;
    }

    /**
     * false: packet should not be forwarded
     * true: nothing changed. forward the packet
     */

    bool call_routing_peek(Packet *p, EtherAddress *src, EtherAddress *dst, int brn_port) {
      PeekFunction *pf;
      for ( int i = 0; i < _peeklist.size(); i++ ) {
        pf = _peeklist[i];
        if ( pf->_brn_port == brn_port )
          if ( ! (*pf->_peek_func)(pf->_peek_obj, p, src, dst, brn_port) ) return false;
      }

      return true;
    }

    virtual uint32_t get_all_header_len(Packet *packet) = 0;

};

CLICK_ENDDECLS
#endif
