#ifndef CLICK_VLANNODEMAP_HH
#define CLICK_VLANNODEMAP_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
#include <click/vector.hh>

CLICK_DECLS

//RESPONSIBLE: Robert

/*TODO:
*/

class VlanNodeMap : public Element {

 public:
  class VlanNode {

   public:

    EtherAddress _etheraddress;
    uint8_t _vlan;

    VlanNode() {
    }

    VlanNode( EtherAddress etheraddress, uint8_t vlan) {
      _etheraddress = etheraddress;
      _vlan = vlan;
    }

  };

  typedef Vector<VlanNode> VlanNodeList;

  VlanNodeMap();
  ~VlanNodeMap();

  const char *class_name() const		{ return "VlanNodeMap"; }
  const char *port_count() const		{ return PORTS_0_0; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  void add_handlers();

  uint8_t getVlan(EtherAddress);
  void inser(EtherAddress,uint8_t);

  int _debug;

 private:

  VlanNodeList _vnm;

};

CLICK_ENDDECLS
#endif
