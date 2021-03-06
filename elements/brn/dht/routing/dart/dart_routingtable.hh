#ifndef CLICK_DART_ROUTINGTABLE_HH
#define CLICK_DART_ROUTINGTABLE_HH
#include <click/timer.hh>

#include "elements/brn/dht/standard/dhtnode.hh"
#include "elements/brn/dht/standard/dhtnodelist.hh"
#include "elements/brn/routing/dart/dart_idstore.hh"
CLICK_DECLS

#define DART_UPDATE_ID   1
#define DART_CLEAR_STORAGE   2
class DartRoutingTable : public Element
{
    class CallbackFunction {

      public:
        void (*_info_func)(void*, int);
        void *_info_obj;

        CallbackFunction( void (*info_func)(void*,int), void *info_obj ) {
          _info_func = info_func;
          _info_obj = info_obj;
        }

       ~CallbackFunction(){}
    };

  public:
    DartRoutingTable();
    ~DartRoutingTable();

   class DRTneighbour {

        public:
          DHTnode* neighbour;
          DHTnodelist* neighbours_neighbour;
          explicit DRTneighbour(DHTnode* a): neighbour(a) {
            neighbours_neighbour = new DHTnodelist();
          }

          DRTneighbour(const DRTneighbour &drtn){
            neighbour = drtn.neighbour;
            neighbours_neighbour = new DHTnodelist();
          }

          ~DRTneighbour() {
            delete neighbours_neighbour;
          }
        };

/*ELEMENT*/
    const char *class_name() const  { return "DartRoutingTable"; }

    const char *processing() const  { return AGNOSTIC; }

    const char *port_count() const  { return "0/0"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void add_handlers();

    String routing_info(void);

    int add_node(DHTnode *node);
    int add_node(DHTnode *node, bool is_neighbour);
    int add_neighbour(DHTnode *node);
    int add_nodes(DHTnodelist *nodes);

    int update_node(DHTnode *node);

    DHTnode *get_node(EtherAddress *ea);
    DHTnode *get_neighbour(EtherAddress *ea);
    DartRoutingTable::DRTneighbour* add_neighbour_entry(DHTnode* n);
    DHTnode* add_neighbours_neighbour(DHTnode* neighbour,DHTnodelist* neighbours);

    int add_update_callback(void (*info_func)(void*,int), void *info_obj);
    void update_callback(int status);                                       //TODO: _me is updated externaly by dart_routingtable-maintenace. change it

  private:
    Vector<CallbackFunction*> _callbacklist;

  public:
  //private:
    DHTnode *_me;

    bool _validID;             //TODO: better name or other way to signal, that this node is not configured

    DHTnode *_parent;

    DHTnodelist _siblings;

    DHTnodelist _allnodes;
    DartIDStore* _ds;
    //DHTnodelist _neighbours;

    Vector<DRTneighbour*> _neighbours;
    uint8_t _ident[6];

    int _debug;
public:
    void setDartIDStorage(DartIDStore* t){_ds = t;}
};

CLICK_ENDDECLS
#endif
