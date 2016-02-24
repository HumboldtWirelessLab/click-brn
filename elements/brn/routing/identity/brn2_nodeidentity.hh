/* Copyright (C) 2005 BerlinRoofNet Lab
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA. 
 *
 * For additional licensing options, consult http://www.BerlinRoofNet.de 
 * or contact brn@informatik.hu-berlin.de. 
 */

#ifndef BRN2NODEIDENTITYELEMENT_HH
#define BRN2NODEIDENTITYELEMENT_HH

#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/element.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/standard/brn_md5.hh"

#include "brn2_device.hh"

CLICK_DECLS

/*
 * =c
 * BRN2NodeIdentity()
 * =s a list of ethernet addresses
 * stores the ethernet address of associated node (clients, brn nodes) ...
 * =d
 */
class BRN2NodeIdentity : public BRNElement {

 public:
  //
  //methods
  //
  BRN2NodeIdentity();
  ~BRN2NodeIdentity();

  const char *class_name() const	{ return "BRN2NodeIdentity"; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }
  int initialize(ErrorHandler *);
  void add_handlers();

  //returns true if the given ethernet address belongs to this node (e.g. wlan-dev)
  bool isIdentical(const EtherAddress *);
  bool isIdentical(const uint8_t *data);

  int getDeviceNumber(EtherAddress *);
  int countDevices() { return _node_devices_size; }
  BRN2Device *getDeviceByNumber(uint8_t);
  BRN2Device *getDeviceByIndex(uint8_t);

  const EtherAddress *getMainAddress() CLICK_DEPRECATED;
  const EtherAddress *getMasterAddress();
  const EtherAddress *getServiceAddress();

  String _nodename;           //name of node. if not set, then etheraddr of _master_device is the name

  void setNodeName(String name) { _nodename = name; }
  String getNodeName() { return _nodename; }

  //void setMasterDeviceName(String name);
  //void setMasterDeviceID(int id);

  md5_byte_t *getNodeID() { return _node_id; };
  uint32_t getNodeID32() { return _node_id_32; };

  //Vector<BRN2Device*> _node_devices;   //TODO: should be private
  BRN2Device **_node_devices;
  uint32_t _node_devices_size;

 private:
  //
  //member
  //
  md5_byte_t _node_id[16];  //md5 sum of master addr (nodeid)
  uint32_t _node_id_32;  //md5 sum of master addr (nodeid)

  BRN2Device* _master_device; //name of master device. This can also be a virtual device. If not set,
                              //then the first device is master
  int _master_device_id;      //id of master device

  BRN2Device* _service_device;
  int _service_device_id;

 public:
  md5_byte_t _click_binary_id[16];  //md5 sum of click_binary
  md5_byte_t _click_script_id[16];  //md5 sum of click_script

  String _instance_id;
  String _instance_owner;
  String _instance_group;

};

CLICK_ENDDECLS
#endif
