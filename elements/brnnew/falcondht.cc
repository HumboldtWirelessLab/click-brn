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

 /*
 * falcondht.{cc,hh}
 */

#include <click/config.h>
#include "falcondht.hh"
#include "dht/dhtcommunication.hh"
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "nblist.hh"
#include "brnlinkstat.hh"

CLICK_DECLS

extern "C" {
  static int bucket_sorter(const void *va, const void *vb) {
      FalconDHT::DHTnode *a = (FalconDHT::DHTnode *)va, *b = (FalconDHT::DHTnode *)vb;

      return FalconDHT::DHTnode::hexcompare(a->md5_digest, b->md5_digest);
  }
}


FalconDHT::FalconDHT():
  _debug(BrnLogger::DEFAULT),
  _linkstat(NULL),
  _lookup_timer(static_lookup_timer_hook,this),
  _sendbuffer_timer(static_queue_timer_hook,this)
{
}

FalconDHT::~FalconDHT()
{
  if (linkprob_info)
    delete[] linkprob_info;
  linkprob_info = NULL;

  for(int i = 0; i < dht_list.size(); i++ )
  {
    if (dht_list[i].key_data)
      delete[] dht_list[i].key_data;
    dht_list[i].key_data = NULL;

    if (dht_list[i].value_data)
      delete[] dht_list[i].value_data;
    dht_list[i].value_data = NULL;
  }
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//+++++++++++++++++++++++++++++++++++ C O N F I G U R E ++++++++++++++++++++++++++++++++++++++++//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//


int
FalconDHT::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int max_jitter;
  _fake_arp_for_simulator = false;

 if (cp_va_parse(conf, this, errh,
    cpOptional,
    cpEtherAddress, "ether address", &_me,
    cpElement, "BRNLinkStat", &_linkstat,
    cpIPPrefix, "address prefix", &_net_address, &_subnet_mask, /* e.g. "10.9.0.0/16" */
    cpInteger, "Startup Time", &_startup_time,
    cpInteger, "minimal Jitter", &_min_jitter,
    cpInteger, "maximal Jitter", &max_jitter,
    cpInteger, "min Jitter between 2 Packets", &_min_dist,
  cpKeywords,
//  "FINGER", cpUnsigned, "fingers", &max_fingers,
    "DEBUG", cpInteger, "debug", &_debug,
    "FAKE_ARP", cpBool, "fake arp for simulator", &_fake_arp_for_simulator,
    cpEnd) < 0)
      return -1;
	
  if (!_linkstat || !_linkstat->cast("BRNLinkStat")) 
  {
    _linkstat = NULL;
//    return errh->error("BRNLinkStat element is not provided or not a BRNLinkStat");
  }

  _jitter = max_jitter - _min_jitter;

//  if ( !max_fingers ) max_fingers = 16;

  max_fingers = 16;
  max_range = 1 << max_fingers;
  linkprob_info = (uint8_t*) new uint8_t [7 * max_fingers];

  return 0;
}

int
FalconDHT::initialize(ErrorHandler *)
{
  init_routing(&_me);
  dht_packet_id = 0;

  unsigned int sched_time = (unsigned int ) ( random() % ( BRN_FDHT_LOOKUP_TIMER_INTERVAL * NODE_DETECTION_INTERVAL ) );
  _lookup_timer.initialize(this);
  _lookup_timer.schedule_after_msec( _startup_time + sched_time );

  node_detection_time = 1;

  unsigned int j = (unsigned int ) ( _min_jitter + ( random() % ( _jitter ) ) );

  _sendbuffer_timer.initialize(this);
  _sendbuffer_timer.schedule_after_msec( 1 + j );


// D E B U G
  push_backs = 0;
  msg_forward = 0;

  linkprob_count = 0;
  linkprop_table_change = false;

  extra_finger_table.clear();

  return 0;
}

void
FalconDHT::static_queue_timer_hook(Timer *, void *v)
{
  FalconDHT *f = (FalconDHT *)v;
  f->queue_timer_hook();
}

/* functions to manage the sendbuffer */
void
FalconDHT::static_lookup_timer_hook(Timer *, void *v)
{
  FalconDHT *f = (FalconDHT *)v;
  f->lookup_timer_hook();
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//++++++++++++++++++++++++++++++++ S I M P L E   A C T I O N +++++++++++++++++++++++++++++++++++//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//


void
FalconDHT::push( int port, Packet *packet )  
{
  int result;
	
  BRN_DEBUG("DHT: PUSH");

  if ( port == 0 )
  {

    BRN_DEBUG("DHT: Hab was von DHT");
    BRN_DEBUG("DHT (Node:%s) : Habe ein Packet von DHT-Node",_me.s().c_str() );

    dht_intern_packet(packet);
  }

  if ( port == 1 )
  {

    BRN_DEBUG("DHT: Hab was von DHCP/ARP");
    BRN_DEBUG("DHT (Node:%s) : Habe ein Packet von ARP/DHCP",_me.s().c_str() );

    struct dht_packet_header *dht_p_header = (struct dht_packet_header *)packet->data();
    memcpy( dht_p_header->sender_hwa, _me.data(), 6 );                                    //ich bin sender

    result = dht_operation(packet);
    // TODO inspect result
  }

  if ( ( port == 2 ) && ( packet->length() > ( 54 + ( 8 * BRN_DSR_MAX_HOP_COUNT ) ) ) )
  {

    BRN_DEBUG("DHT: Hab was von DSR");
    BRN_DEBUG("DHT (Node:%s) : Habe ein Packet von DSR",_me.s().c_str() );

    //jump over DSR and BRN

    int ether_offset = 54 + ( 8 * BRN_DSR_MAX_HOP_COUNT ); // 16 = BRN_DSR_MAX_HOP_COUNT  insgesamt 182

    uint8_t *p_data = (uint8_t *)packet->data(); 

    p_data = &p_data[ether_offset];

    if ( ( memcmp( _me.data(), &p_data[0] , 6 ) == 0 ) || ( memcmp( _me.data(), &p_data[6] , 6 ) == 0 ) )
    {
      BRN_DEBUG("DHT_DSR: Takeout: von mir oder fuer mich, also gleich wieder raus damit");
      output( 2 ).push( packet );
      return;
    }

    uint16_t proto_type = htons(0x8086);

    if ( memcmp( &p_data[12] , &proto_type , 2 ) != 0 )
    {
      BRN_WARN("DHT_DSR: Takeout: Kein BRN ! irgendwas laeuft schief");
      output( 2 ).push( packet );
      return;
    }

    if ( ( p_data[sizeof(click_ether) + 0] != 7 ) ||  ( p_data[sizeof(click_ether) + 1] != 7 ) )
    {
      BRN_WARN("DHT_DSR: Takeout: Nix fuer mich !");
      output( 2 ).push( packet );
      return;
    }

    BRN_DEBUG("DHT_DSR: es ist ein DHT-Packet in DSR");

    struct dht_packet_header *dht_header = (struct dht_packet_header *)&p_data[sizeof(click_ether) + 6];

    if ( ( ( dht_header->code & ROUTING ) == ROUTING ) || ( ( dht_header->code & DHT_DATA ) == DHT_DATA ) )
    {
      BRN_DEBUG("DHT_DSR: Packet ist Routing oder Data-Paket! KEINE DHT-Operation !");
      output( 2 ).push( packet );
      return;
    }

    BRN_DEBUG("DHT_DSR: Packet ist DHT-Operation!");

    if ( memcmp( &dht_header->sender_hwa[0] , &p_data[0] , 6 ) == 0 )
    {
      BRN_DEBUG("DHT_DSR: Packet ist Antwort auf DHT-Operation!");
      output( 2 ).push( packet );
      return;
    }

    DHTentry *_dht_entry = new DHTentry();

    uint8_t *option = &p_data[sizeof(click_ether) + 6 + sizeof(struct dht_packet_header)];                           //copy key

    _dht_entry->key_type = option[1];
    _dht_entry->key_len = option[2];

    if ( _dht_entry->key_len > 0 )
    {
      _dht_entry->key_data = (uint8_t *) new uint8_t [_dht_entry->key_len];
      memcpy( _dht_entry->key_data , &option[3], _dht_entry->key_len );
    }
    else
    {
      delete _dht_entry;
      output( 2 ).push( packet );
    }

    EtherAddress *cor_node = find_corresponding_node(_dht_entry);

    EtherAddress cur_dest = EtherAddress(&p_data[0]);

    EtherAddress cur_src = EtherAddress(&p_data[6]);
   // uint8_t *cur_src_data = (uint8_t *)packet->data(); 
   // EtherAddress cur_src = EtherAddress(&cur_src_data[6 + 3 + 6]);

    if ( myself < (unsigned int)dht_members.size() )
    {
       BRN_DEBUG("DHT_DSR: my range is %s",dht_members[myself].start_ip_range.s().c_str());
    }
    else
    {
       BRN_DEBUG("DHT_DSR: myself out of range");
    }
    
    
    if ( my_finger_is_better(&cur_src , &cur_dest , cor_node , _dht_entry) )
    {
      delete[] _dht_entry->key_data;
      _dht_entry->key_data = NULL;
      delete _dht_entry;
      _dht_entry = NULL;

      packet->pull( ether_offset +  sizeof(click_ether) + 6 );
      if ( *cor_node == _me )
      {
        BRN_DEBUG("DHT_DSR: Hab was besseres ! MICH");
        dht_operation(packet);
      }
      else
      {
        BRN_DEBUG("DHT_DSR: Hab was besseres! ein anderen Knoten");
        send_packet_to_node_direct( packet, cor_node->data() );
      }
    }
    else
    {
      BRN_DEBUG("DHT_DSR: Hab nichts besseres");
      delete[] _dht_entry->key_data;
      _dht_entry->key_data = NULL;
      delete _dht_entry;
      _dht_entry = NULL;

      output( 2 ).push( packet );
    }

  }

}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//+++++++++++++++++++++++++++++++ D H T - O P E R A T I O N S ++++++++++++++++++++++++++++++++++//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//


int
FalconDHT::dht_operation(Packet *p_in)
{
  WritablePacket *dht_p_out = NULL;

  struct dht_packet_header dht_p;

  DHTentry *_dht_entry = new DHTentry();

  uint8_t *option = NULL;
  uint8_t *dht_p_out_data = NULL;

  int result;

// R E A D   K E Y

  BRN_DEBUG("DHT: Recv-Paket");

  if ( p_in == NULL ) BRN_DEBUG("DHT: NO PAKET");

  BRN_DEBUG("DHT: Recv-Paket(len=%d): Unpack Packet ! Copy static part",p_in->length());

  DHTPacketUtil::dht_unpack_payload(p_in);

  uint8_t *dht_p_in_data = (uint8_t *)p_in->data();                                   //copy static part
  memcpy((uint8_t *)&dht_p, dht_p_in_data, sizeof(struct dht_packet_header));

//  struct dht_packet_payload *get_payload_by_number(Packet *dht_packet, uint8_t num)

  option = &dht_p_in_data[sizeof(struct dht_packet_header)];                           //copy key

  _dht_entry->key_type = option[1];
  _dht_entry->key_len = option[2];
  if ( _dht_entry->key_len > 0 )
  {
    _dht_entry->key_data = (uint8_t *) new uint8_t [_dht_entry->key_len];
    memcpy( _dht_entry->key_data , &option[3], _dht_entry->key_len );
  }


// F O R W A R D I N G ? ? ?

  EtherAddress *cor_node;

  cor_node = find_corresponding_node(_dht_entry);

  if ( *cor_node != _me )
  {
    BRN_DEBUG("DHT (Node:%s) : pack and forward to: %s", _me.s().c_str(), cor_node->s().c_str());

    DHTPacketUtil::dht_pack_payload(p_in);

    if ( dht_p.sender == DHT )
    {
      BRN_DEBUG("DHT: Forward Paket with send_to");
      send_packet_to_node_direct( p_in,cor_node->data() );
    }
    else
    {
      BRN_DEBUG("DHT: Forward Paket with forward");
      forward_packet_to_node( p_in, cor_node );
    }

    msg_forward++;

    if ( _dht_entry->key_len > 0 ) delete[] _dht_entry->key_data;
    delete _dht_entry;
    return(1);
  }

   if ( i_am_the_real_one( _dht_entry) )
   {
     BRN_DEBUG("DHT: Ich bin wirklich der richtige");
   }
   else
   {
     BRN_DEBUG("DHT: ich bin nicht der richtige fuer den Key");
     BRN_DEBUG("DHT: Info:%s",routing_info().c_str());
     BRN_DEBUG("DHT: Key ist %s",IPAddress(_dht_entry->key_data).s().c_str());
   }

// L O C A L - O P E R A T I O N !!

  BRN_DEBUG("DHT: Local operation");

  if ( dht_p.payload_len > ( DHT_PAYLOAD_OVERHEAD + _dht_entry->key_len ) )
  {
    BRN_DEBUG("DHT: Copy Value");

    _dht_entry->value_len = dht_p.payload_len - ( DHT_PAYLOAD_OVERHEAD + _dht_entry->key_len );
    _dht_entry->value_data = (uint8_t *)new uint8_t [_dht_entry->value_len];
    memcpy( _dht_entry->value_data, &option[ DHT_PAYLOAD_OVERHEAD + _dht_entry->key_len ], _dht_entry->value_len );
  }

  BRN_DEBUG("DHT: New Packet");

  dht_p.receiver = dht_p.sender;
  dht_p.sender = DHT;
  dht_p.flags = 0;


  if ( ( dht_p.code & LOCK ) == LOCK )
  {
    dht_lock( _dht_entry, dht_p.sender_hwa );
  }
	
  if ( ( dht_p.code & READ ) == READ )
  {
    BRN_DEBUG("DHT: READ");

    result = dht_read(_dht_entry);

    if ( result != 1 )
    {
      dht_p_out = p_in->put( _dht_entry->value_len );  // new Packet with space for the value

      BRN_DEBUG("DHT:  dht_p_out: %d",dht_p_out );

      dht_p_out_data = dht_p_out->data();
      option = &dht_p_out_data[ sizeof(struct dht_packet_header) + _dht_entry->key_len + DHT_PAYLOAD_OVERHEAD ];  //copy value

      dht_p.payload_len = dht_p.payload_len + _dht_entry->value_len;

      memcpy(&option[0], _dht_entry->value_data, _dht_entry->value_len );
    }
    else
    {
      BRN_DEBUG("DHT: Key not found");

      // Answer to unknown IP addresses, if running in simulator fake mode
      if (_fake_arp_for_simulator 
        && (( dht_p.receiver == ARP ) || ( dht_p.prim_sender == ARP )) )
      {
        BRN_INFO("DHT: Sender ist ARP ! use FAKEIP");

        if ( _dht_entry->value_len != 0 ) delete[] _dht_entry->value_data;
  
        _dht_entry->value_len = 9;
        _dht_entry->value_data = (uint8_t*) new uint8_t [_dht_entry->value_len];

        _dht_entry->value_data[0] = 1;
        _dht_entry->value_data[1] = TYPE_MAC;
        _dht_entry->value_data[2] = 6;
        memcpy( &_dht_entry->value_data[3] , "\1\2\3\4\5\6" , 6 );

        dht_p_out = p_in->put( _dht_entry->value_len );  // new Packet with space for the value

        dht_p_out_data = dht_p_out->data();
        option = &dht_p_out_data[ sizeof(struct dht_packet_header) + _dht_entry->key_len + DHT_PAYLOAD_OVERHEAD ];  //copy value

        dht_p.payload_len = dht_p.payload_len + _dht_entry->value_len;

        memcpy(&option[0], _dht_entry->value_data, _dht_entry->value_len ); 

      }
      else
      {
         BRN_DEBUG("DHT: NO IP FOUND");

         dht_p.flags = 1;
         dht_p_out = p_in->put(0);

         dht_p_out_data = dht_p_out->data();
      }
    }			

    memcpy(dht_p_out_data,(uint8_t *)&dht_p, sizeof(struct dht_packet_header) );
  }

  if ( ( dht_p.code & INSERT ) == INSERT )
  {
    BRN_DEBUG("DHT: INSERT");
    result = dht_read(_dht_entry);

    if ( result == 1 )
    {
      BRN_DEBUG("DHT: New Key");
      result = dht_write(_dht_entry);
    }
    else
    {
      BRN_DEBUG("DHT: Key exists ! No Insert");
      result = 1;
    }

    dht_p.flags = result;
	if (dht_p_out == NULL)
    	dht_p_out = p_in->put(0);
    dht_p_out_data = dht_p_out->data();		
    memcpy(dht_p_out_data,(uint8_t *)&dht_p, sizeof(struct dht_packet_header) );
  }
	
  if ( ( dht_p.code & WRITE ) == WRITE )
  {
    BRN_DEBUG("DHT: WRITE");

    result = dht_read(_dht_entry);

    if ( result == 1 )
    {
      BRN_DEBUG("DHT: New Key");
      result = dht_write(_dht_entry);
    }
    else
    {
      BRN_DEBUG("DHT: Key exists");

    /*  use only if dht_read overwrite existing values while reading a key to test if it exists
    
      delete[] _dht_entry->value_data;
      _dht_entry->value_len = dht_p.payload_len - ( DHT_PAYLOAD_OVERHEAD + _dht_entry->key_len );
      _dht_entry->value_data = (uint8_t *)new uint8_t [_dht_entry->value_len];
      memcpy( _dht_entry->value_data, &option[ DHT_PAYLOAD_OVERHEAD + _dht_entry->key_len ], _dht_entry->value_len );
    */
      
      result = dht_overwrite(_dht_entry);
    }

    dht_p.flags = result;
	if(dht_p_out == NULL)
    	dht_p_out = p_in->put(0);
    dht_p_out_data = dht_p_out->data();		

    memcpy(dht_p_out_data,(uint8_t *)&dht_p, sizeof(struct dht_packet_header) );
  }

  if ( ( dht_p.code & REMOVE ) == REMOVE )
  {
    BRN_DEBUG("DHT: Remove");

    result = dht_remove(_dht_entry);
	if (dht_p_out == NULL)
    	dht_p_out = p_in->put(0);
    dht_p_out_data = dht_p_out->data();		

    memcpy(dht_p_out_data,(uint8_t *)&dht_p, sizeof(struct dht_packet_header) );
  }

  if ( ( dht_p.code & UNLOCK ) == UNLOCK )
  {
    dht_unlock( _dht_entry, dht_p.sender_hwa );
  }

  BRN_DEBUG("DHT: Packet is ready");

  if ( _dht_entry->key_len > 0 ) delete[] _dht_entry->key_data;
  if ( _dht_entry->value_len > 0 ) delete[] _dht_entry->value_data;

  delete _dht_entry;

//Antwort für entfernten DHT-Knoten gefunden, sende ihm antwort

  BRN_DEBUG("DHT: dht_pack");
  DHTPacketUtil::dht_pack_payload(dht_p_out);

  if ( dht_p.receiver == DHT )
  {
    BRN_DEBUG("DHT: DHT-Operation: Zurueck zum Ursprung");
    send_packet_to_node_direct(dht_p_out, dht_p.sender_hwa );
    return(1);
  }
  else
  {
    BRN_DEBUG("DHT: Zum Client");
    BRN_DEBUG("DHT (Node:%s) : Sende ein Packet an ARP/DHCP (LOCAL)",_me.s().c_str() );

    output( 1 ).push( dht_p_out );
    return(0);
  }
}



int FalconDHT::dht_read(DHTentry *_dht_entry)
{
  BRN_DEBUG("DHT: Function: dht_read: start");
  if (NULL != _dht_entry && _dht_entry->key_type == TYPE_IP) {
    BRN_DEBUG("dht_read: looking for %s", IPAddress(_dht_entry->key_data).s().c_str());
  }

  for(int i = 0; i < dht_list.size(); i++ ) {
    if ( dht_list[i].key_len == _dht_entry->key_len )
    {
      if ( memcmp( dht_list[i].key_data, _dht_entry->key_data, _dht_entry->key_len ) == 0 )
      {
        if ( _dht_entry->value_len != 0 ) 
	{
	  return(0);
	  //delete _dht_entry->value_data;
        }
        _dht_entry->value_len = dht_list[i].value_len;

        if ( _dht_entry->value_len != 0 )
        {
          _dht_entry->value_data = (uint8_t*) new uint8_t [_dht_entry->value_len];
          memcpy( _dht_entry->value_data , dht_list[i].value_data, dht_list[i].value_len );
        }
        return(0);
      }
    }
  }
 
  BRN_DEBUG("DHT: Function: dht_read end");

  return(1);
}

int FalconDHT::dht_overwrite(DHTentry *_dht_entry)
{
  int index = 0;
  uint16_t val_len = 0;
  uint32_t new_key_len, new_key_type, new_key_valueindex;
 
  new_key_len = new_key_type = new_key_valueindex = 0;
  
  uint16_t new_val_len;
  uint8_t *new_val; 

  uint16_t old_index_start;
  uint16_t old_index_end;
		
//  click_chatter("DHT: overwrite");
  for(int i = 0; i < dht_list.size(); i++ )
    if ( dht_list[i].key_len == _dht_entry->key_len )
    {
      if ( memcmp( dht_list[i].key_data, _dht_entry->key_data, _dht_entry->key_len ) == 0 )
      {
        if ( dht_list[i].value_len != 0 )
	{
          if ( dht_list[i].value_data[1] == TYPE_SUB_LIST )
          {
//	    click_chatter("DHT: overwrite with subkey");
	    if ( _dht_entry->value_len >= DHT_PAYLOAD_OVERHEAD )   
	    {
	      if ( _dht_entry->value_data[1] != TYPE_SUB_LIST )
	        return(1);
	      else
	      {
	        for( index = DHT_PAYLOAD_OVERHEAD; index < ( _dht_entry->value_len - DHT_PAYLOAD_OVERHEAD );)
                {
                  if ( _dht_entry->value_data[index] == 0 ) //found subkey
		  {
		    new_key_len = _dht_entry->value_data[index + 2];
		    new_key_type = _dht_entry->value_data[index + 1];
		    new_key_valueindex = index;
		    break;
		  }
	        }	  
	      }
	    }
	    else
	      return(1);
 	  
//	    click_chatter("DHT: overwrite with sub-key 2");
//	    click_chatter("DHT: New: len %d type %d index %d",new_key_len,new_key_type,new_key_valueindex);
	  
	  
            for( index = DHT_PAYLOAD_OVERHEAD; index < ( dht_list[i].value_len - DHT_PAYLOAD_OVERHEAD );)
            {
              val_len = dht_list[i].value_data[index + 2];

              if ( dht_list[i].value_data[index] == 0 ) //found subkey
              {
                //subkey found, compare key : if equal : overwrite and exit : else jump over
		if ( ( dht_list[i].value_data[index + 2] == new_key_len ) && (dht_list[i].value_data[index + 1] == new_key_type ) )
		{
		
//		  click_chatter("Found one subkey");  
		
		  if ( memcmp( &(dht_list[i].value_data[index + 3]), &(_dht_entry->value_data[new_key_valueindex + 3]), new_key_len ) == 0 )
		  {
//		    click_chatter("it's the same");
		  
//		    click_chatter("%c == %c",dht_list[i].value_data[index + 3],_dht_entry->value_data[new_key_valueindex + 3]);
		  
		    old_index_start = index;
		    old_index_end = index + new_key_len + DHT_PAYLOAD_OVERHEAD;
		    
		    //search for next subkey or end
		    while ( ( old_index_end < ( dht_list[i].value_len - DHT_PAYLOAD_OVERHEAD ) ) && ( dht_list[i].value_data[old_index_end] != 0 ) ) 
		    {
		     
//		     click_chatter("%d jump",old_index_end);
		      old_index_end += dht_list[i].value_data[old_index_end + 2] + DHT_PAYLOAD_OVERHEAD;
//		     click_chatter("%d jumpdest",old_index_end);
		      
		    }
		    
		    if ( old_index_end >= ( dht_list[i].value_len - DHT_PAYLOAD_OVERHEAD ) ) // subkey was the last
		    {
//		       click_chatter("subkey was last");
		    
                       new_val_len = old_index_start + _dht_entry->value_len - DHT_PAYLOAD_OVERHEAD; 
                       new_val = (uint8_t*) new uint8_t [new_val_len];
		       
		       dht_list[i].value_data[2] += new_val_len - dht_list[i].value_len; 
		       
                       memcpy( new_val, dht_list[i].value_data , old_index_start );
                       memcpy( &(new_val[old_index_start]), &(_dht_entry->value_data[DHT_PAYLOAD_OVERHEAD]) , (_dht_entry->value_len - DHT_PAYLOAD_OVERHEAD) );
		       
                       delete[] dht_list[i].value_data;
	      
                       dht_list[i].value_len = new_val_len;
	               dht_list[i].value_data = new_val;
		      
		    }
		    else  //subkey in the middle
		    {
//		       click_chatter("subkey middler");
		     
                       new_val_len =  old_index_start + ( dht_list[i].value_len - old_index_end ) + _dht_entry->value_len - DHT_PAYLOAD_OVERHEAD; 
                       new_val = (uint8_t*) new uint8_t [new_val_len];
		    
		       dht_list[i].value_data[2] += new_val_len - dht_list[i].value_len; 

                       memcpy( new_val, dht_list[i].value_data , old_index_start );
		       memcpy( &(new_val[old_index_start]), &(_dht_entry->value_data[DHT_PAYLOAD_OVERHEAD]), ( _dht_entry->value_len - DHT_PAYLOAD_OVERHEAD ) );
		       memcpy( &(new_val[old_index_start + (_dht_entry->value_len - DHT_PAYLOAD_OVERHEAD) ]), &(dht_list[i].value_data[old_index_end]), ( dht_list[i].value_len - old_index_end ) );
		       
	               delete[] dht_list[i].value_data;
	      
                       dht_list[i].value_len = new_val_len;
	               dht_list[i].value_data = new_val;
		    }
		    
		    return(0);
		  }
		  else
		  {
		    index += DHT_PAYLOAD_OVERHEAD + val_len;
//		    click_chatter("dht: but its another one");
		  }
		}
		else
		  index += DHT_PAYLOAD_OVERHEAD + val_len;
              }
              else  //no subkey,so jump over
              {
                index += DHT_PAYLOAD_OVERHEAD + val_len;
              }
            }
	    
//	    click_chatter("dht:Subkey not found! insert");
	    
	    if ( index >= ( dht_list[i].value_len - DHT_PAYLOAD_OVERHEAD ) )
	    {
              //subkey not found : insert
	      
//	      click_chatter("insert");
	      
              new_val_len = dht_list[i].value_len + _dht_entry->value_len - DHT_PAYLOAD_OVERHEAD; 
              new_val = (uint8_t*) new uint8_t [new_val_len];
      
              dht_list[i].value_data[2] += new_val_len - dht_list[i].value_len; 
              
	      memcpy( new_val, dht_list[i].value_data , dht_list[i].value_len );
	      memcpy( &(new_val[dht_list[i].value_len]), &(_dht_entry->value_data[DHT_PAYLOAD_OVERHEAD]), (_dht_entry->value_len - DHT_PAYLOAD_OVERHEAD) );
            
	      delete[] dht_list[i].value_data;
	      
	      dht_list[i].value_len = new_val_len;
	      dht_list[i].value_data = new_val;
	      
	      return(0);
	    }
          }
          else
          {
            delete[] dht_list[i].value_data;
          }
           
          dht_list[i].value_len = _dht_entry->value_len ;
        
          if ( _dht_entry->value_len != 0 )
          {
            dht_list[i].value_data = (uint8_t*) new uint8_t [_dht_entry->value_len];
            memcpy( dht_list[i].value_data, _dht_entry->value_data , dht_list[i].value_len );
          }
      
          return(0);
        }
      }
    }

  return(1);
}

int FalconDHT::dht_write(DHTentry *_dht_entry)
{
  DHTentry _new_entry;
  _new_entry._relocated_id = 0;
  _new_entry._is_relocated = false;

  _new_entry.key_type = _dht_entry->key_type;
  _new_entry.key_len = _dht_entry->key_len;
  _new_entry.key_data = (uint8_t*) new uint8_t [_dht_entry->key_len];
  memcpy( _new_entry.key_data ,_dht_entry->key_data , _new_entry.key_len );
  
  _new_entry.value_len = _dht_entry->value_len;
  _new_entry.value_data = (uint8_t*) new uint8_t [_dht_entry->value_len];
  memcpy( _new_entry.value_data ,_dht_entry->value_data , _new_entry.value_len );
  
  push_backs++;

  dht_list.push_back(_new_entry);

  return(0);
}


int FalconDHT::dht_remove(DHTentry *_dht_entry)
{
  int index = 0;
  uint16_t val_len = 0;
  uint32_t new_key_len, new_key_type, new_key_valueindex;

  new_key_len = new_key_type = new_key_valueindex = 0;
 
  uint16_t new_val_len;
  uint8_t *new_val; 

  uint16_t old_index_start;
  uint16_t old_index_end;


  for(int i = ( dht_list.size() - 1 ); i >= 0; i-- )
    if ( dht_list[i].key_len == _dht_entry->key_len )
    {
      if ( memcmp( dht_list[i].key_data, _dht_entry->key_data, _dht_entry->key_len ) == 0 )
      {
        if ( dht_list[i]._is_locked == false )
        {
          if ( dht_list[i].value_len != 0 )
          {
            if ( dht_list[i].value_data[1] == TYPE_SUB_LIST )
            {
//	      click_chatter("DHT: overwrite with subkey");
	      if ( _dht_entry->value_len >= DHT_PAYLOAD_OVERHEAD )   
	      {
	        if ( _dht_entry->value_data[1] != TYPE_SUB_LIST )
	          return(1);
	        else
	        {
	          for( index = DHT_PAYLOAD_OVERHEAD; index < ( _dht_entry->value_len - DHT_PAYLOAD_OVERHEAD );)
                  {
                    if ( _dht_entry->value_data[index] == 0 ) //found subkey
	            {
		      new_key_len = _dht_entry->value_data[index + 2];
		      new_key_type = _dht_entry->value_data[index + 1];
		      new_key_valueindex = index;
		      break;
		    }
	          }	  
	        }
	      }
	      else
	        return(1);
		
//	      click_chatter("DHT: overwrite with sub-key 2");
//	      click_chatter("DHT: New: len %d type %d index %d",new_key_len,new_key_type,new_key_valueindex);
	  
	  
              for( index = DHT_PAYLOAD_OVERHEAD; index < ( dht_list[i].value_len - DHT_PAYLOAD_OVERHEAD );)
              {
                val_len = dht_list[i].value_data[index + 2];

                if ( dht_list[i].value_data[index] == 0 ) //found subkey
                {
                  //subkey found, compare key : if equal : overwrite and exit : else jump over
    		  if ( ( dht_list[i].value_data[index + 2] == new_key_len ) && (dht_list[i].value_data[index + 1] == new_key_type ) )
		  {
		
//		    click_chatter("Found one subkey");  
		
		    if ( memcmp( &(dht_list[i].value_data[index + 3]), &(_dht_entry->value_data[new_key_valueindex + 3]), new_key_len ) == 0 )
		    {
//		      click_chatter("it's the same");
		  
//		      click_chatter("%c == %c",dht_list[i].value_data[index + 3],_dht_entry->value_data[new_key_valueindex + 3]);
		  
		      old_index_start = index;
		      old_index_end = index + new_key_len + DHT_PAYLOAD_OVERHEAD;
		    
		      //search for next subkey or end
		      while ( ( old_index_end < ( dht_list[i].value_len - DHT_PAYLOAD_OVERHEAD ) ) && ( dht_list[i].value_data[old_index_end] != 0 ) ) 
		      {
		     
//		        click_chatter("%d jump",old_index_end);
		        old_index_end += dht_list[i].value_data[old_index_end + 2] + DHT_PAYLOAD_OVERHEAD;
//		        click_chatter("%d jumpdest",old_index_end);
		      
		      }
		    
		      if ( old_index_end >= ( dht_list[i].value_len - DHT_PAYLOAD_OVERHEAD ) ) // subkey was the last
		      {
//		        click_chatter("subkey was last");
		    
                        new_val_len = old_index_start; 
                        new_val = (uint8_t*) new uint8_t [new_val_len];
		       
                        dht_list[i].value_data[2] += new_val_len - dht_list[i].value_len; 
                    
		        memcpy( new_val, dht_list[i].value_data , old_index_start );
		       
                        delete[] dht_list[i].value_data;
	      
                        dht_list[i].value_len = new_val_len;
	                dht_list[i].value_data = new_val;
		      
		      }
		      else  //subkey in the middle
		      {
//		        click_chatter("subkey middler");
		     
                        new_val_len =  old_index_start + ( dht_list[i].value_len - old_index_end ); 
                        new_val = (uint8_t*) new uint8_t [new_val_len];
		    
                        dht_list[i].value_data[2] += new_val_len - dht_list[i].value_len; 
                 
		        memcpy( new_val, dht_list[i].value_data , old_index_start );
		        memcpy( &(new_val[old_index_start]), &(dht_list[i].value_data[old_index_end]), ( dht_list[i].value_len - old_index_end ) );
		       
	                delete[] dht_list[i].value_data;
	      
                        dht_list[i].value_len = new_val_len;
	                dht_list[i].value_data = new_val;
		      }
		    
		      return(0);
		    }  //found the subkey
		    else
		    {
		      index += DHT_PAYLOAD_OVERHEAD + val_len;
//		      click_chatter("dht: but its another one");
		    }
		  }//found a subkey
		  else
		    index += DHT_PAYLOAD_OVERHEAD + val_len;
                }
                else  //no subkey,so jump over
                {
                  index += DHT_PAYLOAD_OVERHEAD + val_len;
                }
              }// end for
            } //entry is a sublist
	    else
            {
              if ( dht_list[i].key_len != 0 ) delete[] dht_list[i].key_data;
              if ( dht_list[i].value_len != 0 ) delete[] dht_list[i].value_data;
              dht_list.erase(dht_list.begin() + i);
              return(0);
            }
	  } //value > DHT_PAYLOADOVERHAED
          else
          {
            if ( dht_list[i].key_len != 0 ) delete[] dht_list[i].key_data;
            if ( dht_list[i].value_len != 0 ) delete[] dht_list[i].value_data;
            dht_list.erase(dht_list.begin() + i);
            return(0);
          }
        }
      }
    }
    
  return(0);
}

int FalconDHT::dht_lock(DHTentry *_dht_entry, uint8_t *hwa)
{
  for(int i = 0; i < dht_list.size(); i++ )
    if ( dht_list[i].key_len == _dht_entry->key_len )
    {
      if ( memcmp( dht_list[i].key_data, _dht_entry->key_data, _dht_entry->key_len ) == 0 )
      {
        if ( dht_list[i]._is_locked == false )
        {
          dht_list[i]._is_locked = true;
          memcpy( dht_list[i]._lock_hwa, hwa, 6 );
          return(0);
        }
        else
          return(1);
      }
    }
 
  return(1);
}

int FalconDHT::dht_unlock(DHTentry *_dht_entry, uint8_t *hwa)
{
  for(int i = 0; i < dht_list.size(); i++ )
    if ( dht_list[i].key_len == _dht_entry->key_len )
    {
      if ( memcmp( dht_list[i].key_data, _dht_entry->key_data, _dht_entry->key_len ) == 0 )
      {
        if ( ( dht_list[i]._is_locked == true ) && ( memcmp(dht_list[i]._lock_hwa, hwa, 6 ) == 0 ) )
        {
          dht_list[i]._is_locked = false;
          return(0);
        }
        else
          return(1);
      }
    }
 
  return(1);
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//+++++++++++++++++++++++++++++++++ C O M M U N I C A T I O N ++++++++++++++++++++++++++++++++++//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//

void
FalconDHT::forward_packet_to_node(Packet *fwd_packet, EtherAddress *rcv_node)
{
  struct dht_packet_header *dht_p_header = (struct dht_packet_header *)fwd_packet->data();

  BRN_DEBUG("DHT: Forward Paket (forward_packet_to_node)");

  forward_list.push_back( ForwardInfo(dht_p_header->sender, dht_p_header->id, rcv_node, fwd_packet) );
  dht_p_header->prim_sender = dht_p_header->sender;

  if ( dht_p_header->sender != DHT )
  {
    BRN_DEBUG("DHT: Forward ARP/DHCP-Paket");
    memcpy( dht_p_header->sender_hwa, _me.data(), 6 );                                    //ich bin sender
    dht_p_header->sender = DHT;

  }
  else
  {
    BRN_DEBUG("DHT: Forward DHT-Paket");
    BRN_DEBUG("DHT (Node:%s) : Sende ein Packet an DHT-Node %s",_me.s().c_str(), rcv_node->s().c_str() );

    memcpy( dht_p_header->sender_hwa, _me.data(), 6 );                                    //ich bin sender
  }    
 
  send_packet_to_node(fwd_packet->clone(),rcv_node->data() );
}

void
FalconDHT::send_packet_to_node(Packet *send_packet,uint8_t *rcv_hwa )
{
  uint16_t body_lenght = send_packet->length();
  body_lenght = htons(body_lenght);

  WritablePacket *out_packet = send_packet->push( sizeof(click_ether) + 6 ); //space for click_ether und BRNProtokoll(6) 
  uint8_t *packet_data = (uint8_t *)out_packet->data();

  packet_data[sizeof(click_ether) + 0] = BRN_PORT_DHT;
  packet_data[sizeof(click_ether) + 1] = BRN_PORT_DHT;
  memcpy(&packet_data[sizeof(click_ether) + 2], &body_lenght,2);
  packet_data[sizeof(click_ether) + 4] = 9;
  packet_data[sizeof(click_ether) + 5] = 0;
  
  click_ether *ether = (click_ether *)out_packet->data();

  memcpy(ether->ether_dhost, rcv_hwa, 6 );
  memcpy(ether->ether_shost, _me.data(), 6 );
  ether->ether_type = 0x8680; /*0*/

  out_packet->set_ether_header(ether);
  out_packet->set_user_anno_c( BRN_DSR_PAYLOADTYPE_KEY, BRN_DSR_PAYLOADTYPE_DHT );


  EtherAddress eth_add(rcv_hwa);
  BRN_DEBUG("DHT (Node:%s) : Sende ein Packet an DHT-Node %s",_me.s().c_str(), eth_add.s().c_str() );
  BRN_DEBUG("DHT SRC: %s DEST: %s",_me.s().c_str(), eth_add.s().c_str() );

  unsigned int j = (unsigned int ) ( _min_jitter +( random() % ( _jitter ) ) );
  packet_queue.push_back( BufferedPacket(out_packet, j ));

  j = get_min_jitter_in_queue();

//  BRN_DEBUG("FalconDHT: Timer after %d ms", j );
  _sendbuffer_timer.schedule_after_msec(j);

  BRN_DEBUG("DHT (Node:%s) : Packet buffered", _me.s().c_str());
}

void
FalconDHT::send_packet_to_node_direct(Packet *send_packet,uint8_t *rcv_hwa )
{
  uint16_t body_lenght = send_packet->length();
  body_lenght = htons(body_lenght);

  WritablePacket *out_packet = send_packet->push( sizeof(click_ether) + 6 ); //space for click_ether und BRNProtokoll(6) 
  uint8_t *packet_data = (uint8_t *)out_packet->data();

  packet_data[sizeof(click_ether) + 0] = BRN_PORT_DHT;
  packet_data[sizeof(click_ether) + 1] = BRN_PORT_DHT;
  memcpy(&packet_data[sizeof(click_ether) + 2], &body_lenght,2);
  packet_data[sizeof(click_ether) + 4] = 9;
  packet_data[sizeof(click_ether) + 5] = 0;
  
  click_ether *ether = (click_ether *)out_packet->data();

  memcpy(ether->ether_dhost, rcv_hwa, 6 );
  memcpy(ether->ether_shost, _me.data(), 6 );
  ether->ether_type = 0x8680; /*0*/

  out_packet->set_ether_header(ether);
  out_packet->set_user_anno_c( BRN_DSR_PAYLOADTYPE_KEY, BRN_DSR_PAYLOADTYPE_DHT );

  EtherAddress eth_add(rcv_hwa);
  BRN_DEBUG("DHT (Node:%s) : Sende ein Packet an DHT-Node %s",_me.s().c_str(), eth_add.s().c_str() );
  BRN_DEBUG("DHT SRC: %s DEST: %s",_me.s().c_str(), eth_add.s().c_str() );

  output( 0 ).push( out_packet );
 
  BRN_DEBUG("DHT (Node:%s) : Packet sent", _me.s().c_str());
}

void
FalconDHT::sendNodeInfo(DHTnodeList *neighbors, uint8_t type)
{
  BRN_DEBUG("DHT : sendNodeInfo");

  for ( int n = 0; n < neighbors->size(); n++ ) 
    sendNodeInfoTo(&((*neighbors)[n].ether_addr), type);
}


void
FalconDHT::sendNodeInfo(Vector<EtherAddress> *neighbors, uint8_t type)
{
  BRN_DEBUG("DHT : sendNodeInfo");

  for ( int n = 0; n < neighbors->size(); n++ ) 
    sendNodeInfoTo(&((*neighbors)[n]), type);
}


Packet *
FalconDHT::createInfoPacket( uint8_t type )
{
  int i;

  WritablePacket *dht_packet = DHTPacketUtil::new_dht_packet();

  struct dht_packet_header *dht_header;
  uint8_t *dht_payload;

  struct dht_routing_header *routing_header;

  struct dht_node_info *_node_info;

  if ( dht_members.size() > 128 )
  {
    BRN_FATAL("DHT: sendNodeInfo: macht die Grenzen von 128 DHT-Knoten weg");
    dht_packet->kill();
    return NULL;
  }
  else
  {
    DHTPacketUtil::set_header(dht_packet, DHT, DHT, 0, ROUTING , 0 );

    dht_packet = dht_packet->put( sizeof(struct dht_routing_header)
                                 + ( dht_members.size() * sizeof(struct dht_node_info) ) ); //alle Ethernetadressen
    dht_header = (struct dht_packet_header *)dht_packet->data();

    dht_payload = (uint8_t *)dht_packet->data();
    routing_header = ( struct dht_routing_header * )&dht_payload[ sizeof(struct dht_packet_header) ];

    memcpy(dht_header->sender_hwa,_me.data(),6);  
    routing_header->_type = type;
    routing_header->_num_send_nodes = dht_members.size();
    routing_header->_num_dht_nodes = dht_members.size();
     
    dht_payload = &dht_payload[ sizeof(struct dht_packet_header) + sizeof(struct dht_routing_header) ];
    _node_info = (struct dht_node_info *)dht_payload;

    for ( i = 0 ; i < dht_members.size(); i++ )
    {
      assert( ( i >=  0 ) && ( i < dht_members.size() ) );
       _node_info[i]._hops = dht_members[i].hops; 
      memcpy( _node_info[i]._ether_addr, dht_members[i].ether_addr.data() , 6);
    }

    return (dht_packet);
  }    
}


void
FalconDHT::sendNodeInfoTo(EtherAddress *eth_addr, uint8_t type)
{

  if ( memcmp( eth_addr->data(), _me.data(), 6 ) == 0 )
  {
    BRN_DEBUG("DHT: ERROR: ich schick mir selbst was");
    return;
  }

 Packet *dht_packet = createInfoPacket(type);
 if (dht_packet != NULL)
 	send_packet_to_node(dht_packet, eth_addr->data());
 
}

void
FalconDHT::sendNodeInfoToAll(uint8_t type)
{   
  uint8_t bcast_add[] = { 255,255,255,255,255,255 };

  Packet *dht_info_packet = createInfoPacket(type);
  if (dht_info_packet == NULL) return;

  uint16_t body_lenght = dht_info_packet->length();
  body_lenght = htons(body_lenght);

  WritablePacket *dht_packet = dht_info_packet->push(20); //space for DMAC(6),SMAC(6),TYPE(2),BRNProtokoll(6)

  uint8_t *ether = dht_packet->data();
  uint16_t prot_type = 0x8680;

  memcpy(&ether[0],bcast_add,6);
  memcpy(&ether[6],_me.data(),6);
  memcpy(&ether[12], &prot_type,2);
  ether[14] = 7;
  ether[15] = 7;
  memcpy(&ether[16], &body_lenght,2);
  ether[18] = 9;
  ether[19] = 0;

  click_ether *ether_he = (click_ether *)dht_packet->data();
  
  dht_packet->set_ether_header(ether_he);

  BRN_DEBUG("DHT (Node:%s) : Sende ein Packet an DHT via Broadcast",_me.s().c_str() );

  unsigned int j = (unsigned int ) ( _min_jitter +( random() % ( _jitter ) ) );
  packet_queue.push_back( BufferedPacket(dht_packet, j ));

  j = get_min_jitter_in_queue();

//  BRN_DEBUG("FalconDHT: Timer after %d ms", j );

  _sendbuffer_timer.schedule_after_msec(j);

//output( 0 ).push( dht_packet );
 
  BRN_DEBUG("DHT (Node:%s) : Packet buffered",_me.s().c_str());
}

void
FalconDHT::dht_intern_packet(Packet *dht_packet)
{
  int i,j;

  uint8_t *dht_payload;
  struct dht_packet_header *dht_header;
  struct dht_routing_header *routing_header;
  struct dht_node_info *_node_info;
  
  Vector<DHTnode> new_dht_members;
 
  dht_payload = (uint8_t *)dht_packet->data();

  dht_header = (struct dht_packet_header *)dht_payload;

  if ( ( dht_header->code & ROUTING ) == ROUTING )
  {
    BRN_DEBUG("DHT: DHT-Dataoperation over DHT-Port (new Routinginfos)");

    routing_header = (struct dht_routing_header *)&dht_payload[ sizeof(struct dht_packet_header) ];

    dht_payload = &dht_payload[ sizeof(struct dht_packet_header) + sizeof(struct dht_routing_header) ];

//new DHT-Member

    bool new_info = false, new_member = false;

    for ( i = 0; i < (signed)( dht_packet->length() - sizeof(struct dht_packet_header) - sizeof(struct dht_routing_header) ); i += sizeof(struct dht_node_info) )
    {
      _node_info = (struct dht_node_info *)&dht_payload[i];

      for ( j = 0; j < dht_members.size(); j++ )
      {
        assert( ( j >=  0 ) && ( j < dht_members.size() ) );

        if ( memcmp( dht_members[j].ether_addr.data(), _node_info[0]._ether_addr, 6 ) == 0 ) break;  //look here cmp correct
      }
	
      if ( j == dht_members.size() )                               // new
      {
        new_info = true;
        new_member = true;

        dht_members.push_back( DHTnode( EtherAddress(_node_info[0]._ether_addr),_node_info[0]._hops + 1, &_net_address, &_subnet_mask ));
        new_dht_members.push_back( DHTnode( EtherAddress(_node_info[0]._ether_addr),_node_info[0]._hops + 1, &_net_address, &_subnet_mask ));

        if ( _node_info[0]._hops == 0 )                             // hops is 0, this is a neighbor !!
        dht_neighbors.push_back( DHTnode( EtherAddress(_node_info[0]._ether_addr),_node_info[0]._hops + 1, &_net_address, &_subnet_mask ));  //new_neighbor
      }
      else
      {
        assert( ( j >=  0 ) && ( j < dht_members.size() ) );

        if ( dht_members[j].hops > ( _node_info[0]._hops + 1 ) )   // less hops
        {
          new_info = true;
        
// wenn nachbar dazukommt dann auch new_member = true ???????
          if ( _node_info[0]._hops == 0 )                          // hops is 0, this is a neighbor !!
            dht_neighbors.push_back( DHTnode( EtherAddress(_node_info[0]._ether_addr),_node_info[0]._hops + 1, &_net_address, &_subnet_mask ));  //old new_neighbor
           
          assert( ( j >=  0 ) && ( j < dht_members.size() ) );

          dht_members[j].hops = _node_info[0]._hops + 1;
        }
      }
    }
 
    if ( new_info && new_member )                  //verbesserter Hopcount führt NICHT zum senden von Paketen
    {
      if ( new_member ) click_qsort(dht_members.begin(), dht_members.size(), sizeof(DHTnode), bucket_sorter);

      sendNodeInfoToAll(ROUTE_INFO);                //via Broadcast

    }

    if ( routing_header->_type == ROUTE_REQUEST )
    {
      EtherAddress *neighbor_ether = new EtherAddress(dht_header->sender_hwa);
      BRN_DEBUG("DHT: Sende Antwort auf Route_Request an %s",neighbor_ether->s().c_str());

      sendNodeInfoTo( neighbor_ether , ROUTE_REPLY);
      delete neighbor_ether;
    }
    else if ( routing_header->_type == ROUTE_REPLY )
    {
      for ( i = ( forward_list.size() - 1 ); i >= 0; i-- )
      {
        assert( ( i >=  0 ) && ( i < forward_list.size() ) );

        if ( ( forward_list[i]._sender == DHT ) && ( memcmp( forward_list[i]._ether_add, dht_header->sender_hwa, 6 ) == 0 ) )
        {
          uint8_t *fwd_dht_payload = (uint8_t *)forward_list[i]._fwd_packet->data();
          struct dht_packet_header *fwd_dht_header = (struct dht_packet_header *)fwd_dht_payload;
    
          if ( ( fwd_dht_header->code & ROUTING ) == ROUTING )
          {
            struct dht_routing_header *fwd_routing_header;
            fwd_routing_header = (struct dht_routing_header *)&fwd_dht_payload[ sizeof(struct dht_packet_header) ];
            if ( fwd_routing_header->_type == ROUTE_REQUEST )
            {
              BRN_DEBUG("DHT: Loesche Request vom erhaltenen ROUTE-REPLY");

              assert( ( i >=  0 ) && ( i < forward_list.size() ) );
  
              forward_list[i]._fwd_packet->kill();
              delete (forward_list[i]._ether_add);

              forward_list.erase(forward_list.begin() + i);
              break;
            }
          }
        }
      }
    }
  
    dht_packet->kill();

    if ( new_member )
    {
      setup_fingertable();
      falcon_dht_balance(&new_dht_members);
    }

    new_dht_members.clear();

  }
  else if ( ( dht_header->code & DHT_DATA ) == DHT_DATA )
  {
    if ( ( dht_header->flags & DHT_DATA_OK ) == DHT_DATA_OK )
    {
      BRN_DEBUG("DHT: DHT-operation over DHT-Port (ack for new infos)");
   
      for(int i = ( dht_list.size() - 1 ); i >= 0; i-- )
      {
        assert( ( i >=  0 ) && ( i < dht_list.size() ) );
      
        if ( ( dht_list[i]._is_relocated == true ) && ( dht_list[i]._relocated_id == dht_header->id ) )
        {
          if ( dht_list[i].key_len != 0 ) delete[] dht_list[i].key_data;
          if ( dht_list[i].value_len != 0 ) delete[] dht_list[i].value_data;
          dht_list.erase(dht_list.begin() + i);
        }
      }

      for( int i = ( forward_list.size() - 1 ); i >= 0 ; i-- )
      {
        assert( ( i >=  0 ) && ( i < forward_list.size() ) );
              
        if ( ( forward_list[i]._sender == DHT ) && ( forward_list[i]._id == dht_header->id ) )
        {
          forward_list[i]._fwd_packet->kill();
          delete (forward_list[i]._ether_add);

          forward_list.erase(forward_list.begin() + i);

          break;
        }
      }
    }
    else
    {
      BRN_DEBUG("DHT: DHT-Dataoperation over DHT-Port (new infos)");

      insert_new_info(dht_packet);

      WritablePacket *dht_packet_ack = DHTPacketUtil::new_dht_packet();

      DHTPacketUtil::set_header(dht_packet_ack, DHT, DHT, DHT_DATA_OK , DHT_DATA , dht_header->id );

      send_packet_to_node( dht_packet_ack, dht_header->sender_hwa );
    }

    dht_packet->kill();
   
  }
  else  //FORWARDING or Local Operation
  {
    EtherAddress *sender_ether = new EtherAddress(dht_header->sender_hwa);

    BRN_DEBUG("DHT: DHT-operation over DHT-Port (DHT-OPERATION)");
    BRN_DEBUG("DHT (Node:%s) : Hab ein Packet (DHT-Operation) von %s",_me.s().c_str(), sender_ether->s().c_str() );

    if ( *sender_ether == _me ) 
    { 
      int i;

      BRN_DEBUG("DHT: ich hab die antwort auf meine Frage . forward_list.size %d",forward_list.size());

      for ( i = ( forward_list.size() - 1 ); i >= 0 ; i-- )
      {
        assert( ( i >=  0 ) && ( i < forward_list.size() ) );
        
        if ( ( forward_list[i]._id == dht_header->id ) && ( forward_list[i]._sender == dht_header->prim_sender ) )
        {
          assert( ( i >=  0 ) && ( i < forward_list.size() ) );

          dht_header->receiver = forward_list[i]._sender;

          BRN_DEBUG("DHT (Node:%s) : Loesche %d Element aus forwardliste",_me.s().c_str(),i );

          forward_list[i]._fwd_packet->kill();
          delete (forward_list[i]._ether_add);

          forward_list.erase(forward_list.begin() + i);

          BRN_DEBUG("DHT (Node:%s) : Unpack and pack ! Sende ein Packet an ARP/DHCP",_me.s().c_str() );

          DHTPacketUtil::dht_unpack_payload(dht_packet);
          DHTPacketUtil::dht_pack_payload(dht_packet);

          output( 1 ).push( dht_packet );
          break;
        }
      }
    }
    else
    {
      BRN_DEBUG("DHT: Got DHT-operation from other Node");

      dht_operation(dht_packet);
    }

    delete sender_ether;
  }
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//+++++++++++++++++++++++++++++++++++++++ F A L C O N ++++++++++++++++++++++++++++++++++++++++++//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
bool
FalconDHT::i_am_the_real_one(DHTentry *_dht_entry)
{

  bool zero_jump_pre_me = ( memcmp( dht_members[myself].md5_digest, dht_members[predecessor].md5_digest , 16 ) < 0 );
  
  for ( int i = 0; i < dht_members.size(); i++ )
  {
    if ( ( i != (signed)myself ) && ( i != (signed)predecessor ) )
    {
      assert( ( i >=  0 ) && ( i < dht_members.size() ) );
      assert( ( (int)myself >=  0 ) && ( myself < (unsigned int)dht_members.size() ) );
      assert( ( (int)predecessor >=  0 ) && ( predecessor < (unsigned int)dht_members.size() ) );
      
      if ( ( ! zero_jump_pre_me ) && 
          ( memcmp( dht_members[myself].start_ip_range.data(), dht_members[i].start_ip_range.data(), 4 ) >= 0 ) &&
          ( memcmp( dht_members[predecessor].start_ip_range.data(), dht_members[i].start_ip_range.data(), 4 ) < 0 ) )
      {
        BRN_DEBUG("DHT: Predecessor ist falsch");
        return ( false );
      }

      if ( ( zero_jump_pre_me ) && 
           ( ( memcmp( dht_members[predecessor].start_ip_range.data(), dht_members[i].start_ip_range.data() , 4 ) < 0 )  ||
            ( memcmp( dht_members[myself].start_ip_range.data(), dht_members[i].start_ip_range.data() , 4 ) >= 0 ) ) )
      {
        BRN_DEBUG("DHT: Predecessor ist falsch");
        return(false);
      }
    }
  }  


  if ( _dht_entry->key_type == TYPE_IP )
  {
      assert( ( (int)myself >=  0 ) && ( myself < (unsigned int)dht_members.size() ) );
      assert( ( (int)predecessor >=  0 ) && ( predecessor < (unsigned int)dht_members.size() ) );


    if ( ( ! zero_jump_pre_me ) && 
           ( memcmp( dht_members[myself].start_ip_range.data(), _dht_entry->key_data, 4 ) >= 0 ) &&
           ( memcmp( dht_members[predecessor].start_ip_range.data(), _dht_entry->key_data, 4 ) < 0 ) ) 
    return ( true );

    if ( ( zero_jump_pre_me ) && 
             ( ( memcmp( dht_members[predecessor].start_ip_range.data(), _dht_entry->key_data, 4 ) < 0 )  ||
               ( memcmp( dht_members[myself].start_ip_range.data(), _dht_entry->key_data, 4 ) >= 0 ) ) )
    return ( true );

  }
  else
  {
    BRN_DEBUG("DHT: kein IP ! Fehler !!!!?!");
  }

  BRN_DEBUG("DHT: Key liegt nicht zwischen mir und meinem Vorg�ger");
  return(false);
}

EtherAddress *
FalconDHT::find_corresponding_node(DHTentry *_dht_entry)
{
  int i;

  bool zero_jump;
  bool fwd_zero_jump;
 
  DHTnode *best_node;

  assert( ( (int)myself >=  0 ) && ( myself < (unsigned int)dht_members.size() ) );
  assert( ( (int)predecessor >=  0 ) && ( predecessor < (unsigned int)dht_members.size() ) );
  assert( ( (int)successor >=  0 ) && ( successor < (unsigned int)dht_members.size() ) );

  zero_jump = ( memcmp( dht_members[myself].md5_digest, dht_members[predecessor].md5_digest , 16 ) < 0 );
  fwd_zero_jump = ( memcmp( dht_members[myself].md5_digest, dht_members[successor].md5_digest , 16 ) > 0 );


  if ( _dht_entry->key_type == TYPE_IP )
  {

    BRN_DEBUG("DHT: Cor_node: Type ist IP");
 
//finaler Knoten ??
    if ( ( ! zero_jump ) && 
           ( memcmp( dht_members[myself].start_ip_range.data(), _dht_entry->key_data, 4 ) >= 0 ) &&
           ( memcmp( dht_members[predecessor].start_ip_range.data(), _dht_entry->key_data, 4 ) < 0 ) ) 
    return ( &dht_members[myself].ether_addr );

    if ( ( zero_jump ) && 
             ( ( memcmp( dht_members[predecessor].start_ip_range.data(), _dht_entry->key_data, 4 ) < 0 )  ||
               ( memcmp( dht_members[myself].start_ip_range.data(), _dht_entry->key_data, 4 ) >= 0 ) ) )
    return ( &dht_members[myself].ether_addr );

           
//es ist mein Nachfolgerknoten ??

    if ( ( ! fwd_zero_jump ) && 
           ( memcmp( dht_members[myself].start_ip_range.data(), _dht_entry->key_data, 4 ) < 0 ) &&
           ( memcmp( dht_members[successor].start_ip_range.data(), _dht_entry->key_data, 4 ) >= 0 ) )
    {
      BRN_DEBUG("DHT_DSR: Es ist mein Nachfolger");
      return ( &dht_members[successor].ether_addr );
    } 

    if ( ( fwd_zero_jump ) && 
             ( ( memcmp( dht_members[myself].start_ip_range.data(), _dht_entry->key_data, 4 ) < 0 )  ||
               ( memcmp( dht_members[successor].start_ip_range.data(), _dht_entry->key_data, 4 ) >= 0 ) ) )
    {
      BRN_DEBUG("DHT_DSR: Es ist mein Nachfolger");
      return ( &dht_members[successor].ether_addr );
    }

//es ist ein knoten aus meiner Fingertable ??
    //suche den letzten der Kleiner ist
    // mit zero_jump  

    assert( ( (int)myself >=  0 ) && ( myself < (unsigned int)dht_members.size() ) );
    assert( ( (int)predecessor >=  0 ) && ( predecessor < (unsigned int)dht_members.size() ) );
    assert( ( (int)successor >=  0 ) && ( successor < (unsigned int)dht_members.size() ) );

    for ( i = 0; i < finger_table.size(); i++ )
    {
      
      assert( ( i >=  0 ) && ( i < finger_table.size() ) );
    
//    fwd_zero_jump = ( memcmp( dht_members[myself].md5_digest, dht_members[i].md5_digest , 16 ) > 0 );
      fwd_zero_jump = ( memcmp( dht_members[myself].md5_digest, finger_table[i].md5_digest , 16 ) > 0 );

      if ( ( ! fwd_zero_jump ) && 
           ( memcmp( dht_members[myself].start_ip_range.data(), _dht_entry->key_data, 4 ) < 0 ) &&
//         ( memcmp( dht_members[i].start_ip_range.data(), _dht_entry->key_data, 4 ) >= 0 ) )
           ( memcmp( finger_table[i].start_ip_range.data(), _dht_entry->key_data, 4 ) >= 0 ) )
      {
         BRN_DEBUG("DHT: Hab einen besseren in meiner Fingertable");
         break;
      }

      if ( ( fwd_zero_jump ) && 
             ( ( memcmp( dht_members[myself].start_ip_range.data(), _dht_entry->key_data, 4 ) < 0 )  ||
//             ( memcmp( dht_members[i].start_ip_range.data(), _dht_entry->key_data, 4 ) >= 0 ) ) )
               ( memcmp( finger_table[i].start_ip_range.data(), _dht_entry->key_data, 4 ) >= 0 ) ) )
      {
        BRN_DEBUG("DHT: Hab einen besseren in meiner Fingertable");
        break;
      }
    }
     
    if ( i == 0 ) return( &dht_members[successor].ether_addr );
    //else return( &finger_table[i-1].ether_addr );
    else best_node = &finger_table[i-1];

//es ist ein knoten aus meiner Nachbarschaft ??
    //suche den letzten der Kleiner ist
    // mit zero_jump  
    for ( i = 0; i < dht_neighbors.size(); i++ )
    {
      assert( ( i >=  0 ) && ( i < dht_neighbors.size() ) );

      fwd_zero_jump = ( memcmp( best_node->start_ip_range.data(), _dht_entry->key_data , 4 ) > 0 );

      if ( ( ! fwd_zero_jump ) && 
           ( memcmp( dht_neighbors[i].start_ip_range.data() , best_node->start_ip_range.data() , 4 ) > 0 ) &&
           ( memcmp( dht_neighbors[i].start_ip_range.data() , _dht_entry->key_data , 4 ) < 0 ) )
      {
        BRN_DEBUG("DHT: Hab einen besseren in meiner Nachbarschaft");
        best_node = &dht_neighbors[i];
      }

      if ( ( fwd_zero_jump ) && 
             ( ( memcmp( dht_neighbors[i].start_ip_range.data() , best_node->start_ip_range.data() , 4 ) > 0 )  ||
               ( memcmp( dht_neighbors[i].start_ip_range.data() , _dht_entry->key_data , 4 ) <  0 ) ) )
      {
        BRN_DEBUG("DHT: Hab einen besseren in meiner Nachbarschaft");
        best_node = &dht_neighbors[i];
      }
    }


//es ist ein knoten aus den Fingertables meiner Fingertable ??
    //suche den letzten der Kleiner ist
    // mit zero_jump  
    for ( i = 0; i < extra_finger_table.size(); i++ )
    {
      fwd_zero_jump = ( memcmp( best_node->start_ip_range.data(), _dht_entry->key_data , 4 ) > 0 );

      assert( ( i >=  0 ) && ( i < extra_finger_table.size() ) );

      if ( ( ! fwd_zero_jump ) && 
           ( memcmp( extra_finger_table[i].start_ip_range.data() , best_node->start_ip_range.data() , 4 ) > 0 ) &&
           ( memcmp( extra_finger_table[i].start_ip_range.data() , _dht_entry->key_data , 4 ) < 0 ) )
      {
        BRN_DEBUG("DHT: Hab einen besseren in Fingertable meiner Nachbarschaft");
        best_node = &extra_finger_table[i];
      }

      if ( ( fwd_zero_jump ) && 
             ( ( memcmp( extra_finger_table[i].start_ip_range.data() , best_node->start_ip_range.data() , 4 ) > 0 )  ||
               ( memcmp( extra_finger_table[i].start_ip_range.data() , _dht_entry->key_data , 4 ) <  0 ) ) )
      {
        BRN_DEBUG("DHT: Hab einen besseren in Fingertable meiner Nachbarschaft");
        best_node = &extra_finger_table[i];
      }
    }

    return( &(best_node->ether_addr) );


/*  // vorher: infos werden aus globaler sicht geholt
    for ( i = 0; i < dht_members.size(); i++ )
      if ( memcmp( dht_members[i].start_ip_range.data(),_dht_entry->key_data,4 ) > 0 ) break;
*/
  }
  else
  {
if ( _debug ) { 
    i = _dht_entry->key_type;
    BRN_DEBUG("DHT: Cor_node: Type ist NICHT (!!!) IP");
    BRN_DEBUG("DHT: Cor_node: Type ist %d",i);
}

    md5_byte_t md5_digest[16];
    md5_state_t state;
    
    MD5::md5_init(&state);
    MD5::md5_append(&state, (const md5_byte_t *)_dht_entry->key_data,_dht_entry->key_len ); //strlen()
    MD5::md5_finish(&state, md5_digest);

    assert( ( (int)successor >=  0 ) && ( successor < (unsigned int)dht_members.size() ) );
    assert( ( (int)myself >=  0 ) && ( myself < (unsigned int)dht_members.size() ) );
    assert( ( (int)predecessor >=  0 ) && ( predecessor < (unsigned int)dht_members.size() ) );

    if ( ( ! zero_jump ) && 
           ( memcmp( dht_members[myself].md5_digest , md5_digest , 16 ) <= 0 ) &&
           ( memcmp( dht_members[predecessor].md5_digest, md5_digest, 16 ) > 0 ) ) 
    return ( &dht_members[myself].ether_addr );

    if ( ( zero_jump ) && 
             ( ( memcmp( dht_members[predecessor].md5_digest , md5_digest, 16 ) > 0 )  ||
               ( memcmp( dht_members[myself].md5_digest , md5_digest, 16 ) <= 0 ) ) )
    return ( &dht_members[myself].ether_addr );

           
//es ist mein Nachfolgerknoten ??

    if ( ( ! fwd_zero_jump ) && 
           ( memcmp( dht_members[myself].md5_digest , md5_digest, 16 ) > 0 ) &&
           ( memcmp( dht_members[successor].md5_digest , md5_digest, 16 ) <= 0 ) ) 
    return ( &dht_members[successor].ether_addr );

    if ( ( fwd_zero_jump ) && 
             ( ( memcmp( dht_members[myself].md5_digest , md5_digest, 16 ) > 0 )  ||
               ( memcmp( dht_members[successor].md5_digest , md5_digest, 16 ) <= 0 ) ) )
    return ( &dht_members[successor].ether_addr );



    //suche den letzten der Kleiner ist
    // mit zero_jump  
    for ( i = 0; i < finger_table.size(); i++ )
    {
      assert( ( (int)successor >=  0 ) && ( successor < (unsigned int)dht_members.size() ) );
      assert( ( (int)myself >=  0 ) && ( myself < (unsigned int)dht_members.size() ) );
      assert( ( i >=  0 ) && ( i <  finger_table.size() ) );

//    fwd_zero_jump = ( memcmp( dht_members[myself].md5_digest, dht_members[i].md5_digest , 16 ) < 0 );
      fwd_zero_jump = ( memcmp( dht_members[myself].md5_digest, finger_table[i].md5_digest , 16 ) < 0 );
      
      if ( ( ! fwd_zero_jump ) && 
           ( memcmp( dht_members[myself].md5_digest , md5_digest, 16 ) > 0 ) &&
//         ( memcmp( dht_members[i].md5_digest , md5_digest, 16 ) <= 0 ) ) 
           ( memcmp( finger_table[i].md5_digest , md5_digest, 16 ) <= 0 ) ) 
           break;

      if ( ( fwd_zero_jump ) && 
             ( ( memcmp( dht_members[myself].md5_digest , md5_digest, 16 ) > 0 )  ||
//             ( memcmp( dht_members[i].md5_digest , md5_digest, 16 ) <= 0 ) ) )
               ( memcmp( finger_table[i].md5_digest , md5_digest, 16 ) <= 0 ) ) )
         break;

    }
     
    if ( i == 0 ) return( &dht_members[successor].ether_addr );
    else return( &finger_table[i-1].ether_addr );


/*  // vorher: infos werden aus globaler sicht geholt
    for ( i = 0; i < dht_members.size(); i++ )
      if ( memcmp( dht_members[i].md5_digest , md5_digest ,16 ) > 0 ) break;
*/
  }

//  node_num = ( i + dht_members.size() - 1 ) % dht_members.size();
//  return( &dht_members[node_num].ether_addr);   

    return( &dht_members[myself].ether_addr);   
}

bool
FalconDHT::my_finger_is_better(EtherAddress *cur_src , EtherAddress *cur_dest , EtherAddress *cor_node ,DHTentry *_dht_entry)
{
  BRN_DEBUG("DHT_DSR: SRC, DEST, COR");
  DHTnode src_dht_node = DHTnode(*cur_src, 0 , &_net_address , &_subnet_mask);
  DHTnode dest_dht_node = DHTnode(*cur_dest, 0 , &_net_address , &_subnet_mask);
  DHTnode cor_dht_node = DHTnode(*cor_node, 0 , &_net_address , &_subnet_mask);

  md5_byte_t dht_entry_md5_digest[16];
  md5_state_t state;

  assert( ( (int)successor >=  0 ) && ( successor < (unsigned int)dht_members.size() ) );
  assert( ( (int)myself >=  0 ) && ( myself < (unsigned int)dht_members.size() ) );


  MD5::md5_init(&state);
  MD5::md5_append(&state, (const md5_byte_t *)_dht_entry->key_data,_dht_entry->key_len );
  MD5::md5_finish(&state, dht_entry_md5_digest);

  bool zero_jump;
  
// liegt schluessel zwischen quelle und ziel ?
  zero_jump = ( memcmp( src_dht_node.md5_digest, dest_dht_node.md5_digest, 16 ) > 0 );

  if ( zero_jump ) BRN_DEBUG("DHT_DSR: ZERO_JUMP");
  else BRN_DEBUG("DHT_DSR: NO ZERO_JUMP");

  if ( _dht_entry->key_type == TYPE_IP )
  {
    BRN_DEBUG("DHT_DSR: KEY: %s",IPAddress(_dht_entry->key_data).s().c_str());

// dest is final-dest (key between src und dest ?)
    if ( ( ! zero_jump ) && 
           ( memcmp( dest_dht_node.start_ip_range.data(), _dht_entry->key_data, 4 ) >= 0 ) &&
           ( memcmp( src_dht_node.start_ip_range.data(), _dht_entry->key_data, 4 ) < 0 ) ) 
    return ( false );

    if ( ( zero_jump ) && 
             ( ( memcmp( src_dht_node.start_ip_range.data(), _dht_entry->key_data, 4 ) < 0 )  ||
               ( memcmp( dest_dht_node.start_ip_range.data(), _dht_entry->key_data, 4 ) >= 0 ) ) )
    return ( false );

    if ( cor_dht_node.ether_addr == dht_members[successor].ether_addr )
    {
       BRN_DEBUG("DHT_DSR: COR ist mein Nachfolger");
     
       bool fwd_zero_jump_me = ( memcmp( dht_members[myself].md5_digest, dht_members[successor].md5_digest , 16 ) > 0 );

       if ( ( ! fwd_zero_jump_me ) && 
           ( memcmp( dht_members[myself].start_ip_range.data(), _dht_entry->key_data, 4 ) < 0 ) &&
           ( memcmp( dht_members[successor].start_ip_range.data(), _dht_entry->key_data, 4 ) >= 0 ) )
       {
         BRN_DEBUG("DHT_DSR: Es ist mein Nachfolger");
         return ( true );
       } 

       if ( ( fwd_zero_jump_me ) && 
                ( ( memcmp( dht_members[myself].start_ip_range.data(), _dht_entry->key_data, 4 ) < 0 )  ||
                ( memcmp( dht_members[successor].start_ip_range.data(), _dht_entry->key_data, 4 ) >= 0 ) ) )
       {
         BRN_DEBUG("DHT_DSR: Es ist mein Nachfolger");
         return ( true );
       }
    }
   
    bool fwd_zero_jump = ( memcmp( dest_dht_node.start_ip_range.data(), _dht_entry->key_data , 4 ) > 0 );


    if ( ( ! fwd_zero_jump ) && 
           ( memcmp( cor_dht_node.start_ip_range.data(), _dht_entry->key_data, 4 ) < 0 ) &&
           ( memcmp( cor_dht_node.start_ip_range.data(), dest_dht_node.start_ip_range.data(), 4 ) > 0 ) ) 
    return ( true );

    if ( ( fwd_zero_jump ) && 
             ( ( memcmp( cor_dht_node.start_ip_range.data() , _dht_entry->key_data, 4 ) < 0 )  ||
               ( memcmp( cor_dht_node.start_ip_range.data() , dest_dht_node.start_ip_range.data() , 4 ) > 0 ) ) )
    return ( true );

    return(false);

  }
  else
  {
/*
    if ( ( ! zero_jump ) && 
           ( memcmp( dht_members[myself].md5_digest , md5_digest , 16 ) <= 0 ) &&
           ( memcmp( dht_members[predecessor].md5_digest, md5_digest, 16 ) > 0 ) ) 
    return ( &dht_members[myself].ether_addr );

    if ( ( zero_jump ) && 
             ( ( memcmp( dht_members[predecessor].md5_digest , md5_digest, 16 ) > 0 )  ||
               ( memcmp( dht_members[myself].md5_digest , md5_digest, 16 ) <= 0 ) ) )
    return ( &dht_members[myself].ether_addr );

           
//es ist mein Nachfolgerknoten ??

    if ( ( ! fwd_zero_jump ) && 
           ( memcmp( dht_members[myself].md5_digest , md5_digest, 16 ) > 0 ) &&
           ( memcmp( dht_members[successor].md5_digest , md5_digest, 16 ) <= 0 ) ) 
    return ( &dht_members[successor].ether_addr );

    if ( ( fwd_zero_jump ) && 
             ( ( memcmp( dht_members[myself].md5_digest , md5_digest, 16 ) > 0 )  ||
               ( memcmp( dht_members[successor].md5_digest , md5_digest, 16 ) <= 0 ) ) )
    return ( &dht_members[successor].ether_addr );



    //suche den letzten der Kleiner ist
    // mit zero_jump  
    for ( i = 0; i < finger_table.size(); i++ )
    {
      fwd_zero_jump = ( memcmp( dht_members[myself].md5_digest, dht_members[i].md5_digest , 16 ) < 0 );

      if ( ( ! fwd_zero_jump ) && 
           ( memcmp( dht_members[myself].md5_digest , md5_digest, 16 ) > 0 ) &&
           ( memcmp( dht_members[i].md5_digest , md5_digest, 16 ) <= 0 ) ) 
         break;

      if ( ( fwd_zero_jump ) && 
             ( ( memcmp( dht_members[myself].md5_digest , md5_digest, 16 ) > 0 )  ||
               ( memcmp( dht_members[i].md5_digest , md5_digest, 16 ) <= 0 ) ) )
         break;

    }
   */
    return(false);     
  }

  return(false);
}


unsigned int
FalconDHT::get_succ(unsigned int id)
{
  unsigned int ac_succ = 0;
  int i;
  unsigned int node_id;
  unsigned int best_node_id = 0;

  for ( i = 0; i < dht_members.size(); i++ ) 
  {
    node_id = dht_members[i].md5_digest[0] * 256 + dht_members[i].md5_digest[1];

    if ( node_id >= id )
    {
      ac_succ = i;
      best_node_id = node_id;
      break;  //suche den ersten der Kleiner/gleich ist
    }
  }

  if ( i < dht_members.size() ) //gabs einer der kleiner ist, dann suche den groessten, der kleiner ist
  {
    i++;

    for ( ; i < dht_members.size(); i++ )
    {
      assert( ( i >=  0 ) && ( i < dht_members.size() ) );

      node_id = dht_members[i].md5_digest[0] * 256 + dht_members[i].md5_digest[1];

      if ( ( node_id >= id ) && ( node_id < best_node_id ) )
      {
        ac_succ = i;
        best_node_id = node_id;
      }
    }
  }
  else  // wir suchen den kleinsten
  {
    ac_succ = 0;
    best_node_id = dht_members[0].md5_digest[0] * 256 + dht_members[0].md5_digest[1];


    for ( i = 1; i < dht_members.size(); i++ )
    {
      assert( ( i >=  0 ) && ( i < dht_members.size() ) );

      node_id = dht_members[i].md5_digest[0] * 256 + dht_members[i].md5_digest[1];

      if ( node_id < best_node_id )
      { 
        ac_succ = i;
        best_node_id = node_id;
      }
    }
  }

  return(ac_succ);
}


int
FalconDHT::setup_fingertable()
{
  unsigned int finger_pos;
  unsigned int succ_node;
  int i;

  if ( dht_members.size() > 0 )
  {

    finger_table.clear();

    for( i = 0; i < (signed)max_fingers; i++ )
    {
      finger_pos = _me_md5_digest[0] * 256 + _me_md5_digest[1];
      finger_pos = ( finger_pos + ( 1 << i ) ) % max_range;
      succ_node = get_succ( finger_pos );
    
      assert( ( succ_node < (unsigned int)dht_members.size() ) );

      finger_table.push_back( DHTnode( dht_members[succ_node].ether_addr , dht_members[succ_node].hops, &_net_address, &_subnet_mask ) );
    }

    for ( i = 0; i < dht_members.size(); i++ )
      if ( dht_members[i].ether_addr == _me ) break;

    myself = i;
    successor = ( i + 1 ) % dht_members.size();
    predecessor = ( i + dht_members.size() - 1 ) % dht_members.size();
    
  }

  assert( myself < (unsigned int) dht_members.size() );
  assert( successor < (unsigned int) dht_members.size() );
  assert( predecessor < (unsigned int) dht_members.size() );

  linkprop_table_change = true;

  return(0);  
}




void
FalconDHT::falcon_dht_balance(Vector<DHTnode> *new_members)
{
  int i,j;

  WritablePacket *dht_packet = NULL;
  int packet_size;
  int index = -1;

  struct dht_packet_header *dht_header;
  uint8_t *dht_payload;

  struct dht_data_header *data_header;

  BRN_DEBUG("DHT: Baclance");
  BRN_DEBUG("DHT: New Member: %d DHT: %d",new_members->size(),dht_list.size());

  for( i = 0; i < new_members->size(); i++ )
  {
    assert( ( i >=  0 ) && ( i < new_members->size() ) );

    if ( (*new_members)[i].ether_addr != _me )
    {
      packet_size = 0;
      for ( j = 0; j < dht_list.size(); j++ )
      {
        if ( (*new_members)[i].ether_addr == *find_corresponding_node(&dht_list[j]) )
        {
          if ( ( packet_size + dht_list[j].key_len + dht_list[j].value_len ) > MAX_DHT_PACKET_SIZE )
          {
            BRN_DEBUG("DHT: Infos werden verschoben nach %s", (*new_members)[i].ether_addr.s().c_str());

            forward_packet_to_node( dht_packet, &((*new_members)[i].ether_addr) );
            packet_size = 0;
          }
	  
          if ( packet_size == 0 )
          {
            dht_packet = DHTPacketUtil::new_dht_packet();

            dht_packet_id++;
            DHTPacketUtil::set_header(dht_packet, DHT, DHT, 0, DHT_DATA , dht_packet_id );
            dht_packet = dht_packet->put( sizeof(struct dht_data_header) );

            packet_size = sizeof(struct dht_packet_header) + sizeof(struct dht_data_header);

            dht_header = (struct dht_packet_header *)dht_packet->data();

            dht_payload = (uint8_t *)dht_packet->data();
            data_header = ( struct dht_data_header * )&dht_payload[ sizeof(struct dht_packet_header) ];

            dht_payload = &dht_payload[ sizeof(struct dht_packet_header) + sizeof(struct dht_data_header) ];
 
            data_header->_type = 0;
            data_header->_num_dht_entries = 0;

            index = 0;
          }

          dht_packet = dht_packet->put( 3 + dht_list[j].value_len + dht_list[j].key_len ); //<----------
          dht_header = (struct dht_packet_header *)dht_packet->data();
          dht_payload = (uint8_t *)dht_packet->data();
          data_header = ( struct dht_data_header * )&dht_payload[ sizeof(struct dht_packet_header) ];
          dht_payload = &dht_payload[ sizeof(struct dht_packet_header) + sizeof(struct dht_data_header)];

          data_header->_num_dht_entries++;
 
          assert( ( j >=  0 ) && ( j < dht_list.size() ) );
  
          dht_payload[index] = dht_list[j].key_type;

          dht_payload[index + 1 ] = dht_list[j].key_len;
          memcpy( &dht_payload[index + 2 ], dht_list[j].key_data , dht_list[j].key_len );
          index += ( 2 + dht_list[j].key_len );
 
          dht_payload[index] = dht_list[j].value_len;
          memcpy( &dht_payload[index + 1 ], dht_list[j].value_data , dht_list[j].value_len );

          BRN_DEBUG("DHT: dht pack");
          DHTPacketUtil::dht_pack_payload(&dht_payload[index + 1 ], dht_list[j].value_len );

          index += ( 1 + dht_list[j].value_len );

          packet_size += ( 3 + dht_list[j].value_len + dht_list[j].key_len ); 

          dht_list[j]._is_relocated = true;
          dht_list[j]._relocated_id = dht_packet_id;
        }
      }  

      if ( packet_size != 0 )
      {
        BRN_DEBUG("DHT: Infos werden verschoben nach %s", (*new_members)[i].ether_addr.s().c_str());
        forward_packet_to_node( dht_packet, &((*new_members)[i].ether_addr) );
      }
    }

    else
    {
       BRN_DEBUG("DHT: Don't move to me");
    }
  }

  BRN_DEBUG("DHT: Baclance end");
}

void
FalconDHT::insert_new_info(Packet *dht_packet)
{
  int index,j;

  DHTentry _new_entry;

  uint8_t key_len,key_type,value_len;

  struct dht_packet_header *dht_header;
  uint8_t *dht_payload;

  struct dht_data_header *data_header;

  dht_header = (struct dht_packet_header *)dht_packet->data();

  dht_payload = (uint8_t *)dht_packet->data();
  data_header = ( struct dht_data_header * )&dht_payload[ sizeof(struct dht_packet_header) ];
       
  dht_payload = &dht_payload[ sizeof(struct dht_packet_header) + sizeof(struct dht_data_header) ];
  index = 0;
 
  int num_infos = data_header->_num_dht_entries;

  BRN_DEBUG("DHT: Anzahl der Infos: %d",num_infos);

  for ( int i = 0; i < num_infos; i++ )
  {
    key_type = dht_payload[index];

    BRN_DEBUG("DHT: New Info with Type: %d",key_type);

    key_len = dht_payload[index + 1];
    
    for ( j = 0; j < dht_list.size(); j++ )
    {
      if ( ( dht_list[j].key_type == key_type ) && ( dht_list[j].key_len == key_len ) )
        if ( memcmp ( dht_list[j].key_data, &dht_payload[index + 2],  key_len ) == 0 )
        {
#ifndef DHT_FALCON_SILENT 
           BRN_DEBUG("DHT: merge: Key found ! Key not insert");
#endif
           break;
        }
    }

    if ( j == dht_list.size() )
    {

      BRN_DEBUG("DHT: new Entry");
      _new_entry._relocated_id = 0;
      _new_entry._is_relocated = false;

   
      _new_entry.key_type = key_type;

      BRN_DEBUG("DHT: Type: %d",key_type);

      _new_entry.key_len = key_len;
      _new_entry.key_data = (uint8_t*) new uint8_t [key_len];
      memcpy( _new_entry.key_data , &dht_payload[index + 2] , _new_entry.key_len );

      index += 2 + key_len;
      value_len = dht_payload[index];
  
      _new_entry.value_len = value_len;
      _new_entry.value_data = (uint8_t*) new uint8_t [value_len];
      memcpy( _new_entry.value_data , &dht_payload[index + 1] , _new_entry.value_len );

      BRN_DEBUG("DHT: dht unpack");

      DHTPacketUtil::dht_unpack_payload(_new_entry.value_data, _new_entry.value_len);

      push_backs++;

      dht_list.push_back(_new_entry);
    }
    else
    {
      index += 2 + key_len;
      value_len = dht_payload[index];
    }

    index += ( 1 + value_len );
  }

  BRN_DEBUG("DHT: Balance");

  falcon_dht_balance(&dht_members);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//+++++++++++++++++++++++++++++++++++++++ R O U T I N G ++++++++++++++++++++++++++++++++++++++++//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
void
FalconDHT::init_routing(EtherAddress *me)
{
  char *ether_add;
  ether_add = (char *)_me.data();

  dht_members.push_back( DHTnode(*me, 0, &_net_address, &_subnet_mask));
  DHTentry::calculate_md5((const char*)DHTnode::convert_ether2hex(me->data()).c_str(),
                   strlen((const char*)DHTnode::convert_ether2hex(me->data()).c_str()), _me_md5_digest);

   
  setup_fingertable();

}

void
FalconDHT::lookup_timer_hook()
{
  node_detection_time = ( node_detection_time + 1 ) %  NODE_DETECTION_INTERVAL;

  if ( node_detection_time == 0 )
  {
//    if(_debug)
  //    BRN_DEBUG("DHT (Node:%s) : Ich kenne %d DHT-Knoten",_me.s().c_str(), dht_members.size()  );
    nodeDetection();
  }

  packet_retransmission();

  _lookup_timer.schedule_after_msec(BRN_FDHT_LOOKUP_TIMER_INTERVAL);

}

void
FalconDHT::packet_retransmission()
{
  int i;
  struct timeval _time_now;

  int retransmission_time;

  click_gettimeofday(&_time_now);

  if ( forward_list.size() > 0 )
   BRN_DEBUG("DHT (Node:%s) : Retransmission. Size: %d",_me.s().c_str(), forward_list.size() );

  Packet *fwd_packet;

  for ( i = ( forward_list.size() - 1 ); i >= 0; i-- )
  {
    uint8_t *dht_p_test = (uint8_t *)forward_list[i]._fwd_packet->data();

    struct dht_packet_header *dht_h_test = (struct dht_packet_header *)dht_p_test;

    if ( ( ( dht_h_test->code & ROUTING ) == ROUTING ) || ( ( dht_h_test->code & DHT_DATA ) == DHT_DATA ) ) 
    {
      retransmission_time = RETRANSMISSION_TIME;
    }
    else
    {
      retransmission_time = ( forward_list[i]._retry + 1 ) * 500;
      if ( retransmission_time > RETRANSMISSION_TIME ) retransmission_time = RETRANSMISSION_TIME;
    }

    if ( diff_in_ms(_time_now, forward_list[i]._send_time) > retransmission_time )
    {
      BRN_DEBUG("DHT (Node:%s) : Retransmission. Timediff: %d",_me.s().c_str(), diff_in_ms(_time_now, forward_list[i]._send_time) );

      assert( ( i >= 0 ) && ( i < forward_list.size() ) );

      forward_list[i]._send_time.tv_sec = _time_now.tv_sec;
      forward_list[i]._retry++;

      if ( forward_list[i]._retry <= MAX_RETRANSMISSIONS )
      {
        fwd_packet = forward_list[i]._fwd_packet->clone();
        send_packet_to_node( fwd_packet, forward_list[i]._ether_add->data() );
      }
      else
      {
        uint8_t *dht_payload = (uint8_t *)forward_list[i]._fwd_packet->data();

        struct dht_packet_header *dht_header = (struct dht_packet_header *)dht_payload;

        if ( ( dht_header->code & ROUTING ) == ROUTING )  //waiting for routereply
        {
          BRN_DEBUG("DHT (Node:) : no Reply ! remove Node ");

          for ( int l = (dht_members.size() - 1); l >= 0; l-- )
            if ( dht_members[l].ether_addr == *forward_list[i]._ether_add )
            {
              dht_members.erase( dht_members.begin() + l );
	      setup_fingertable();
              break;
            }
        }
        else if ( ( dht_header->code & DHT_DATA ) == DHT_DATA )
        {
          BRN_DEBUG("DHT (Node:) : no ack for new Infos ! restore !");

          for(int i_2 = 0; i_2 < dht_list.size(); i_2++ )
          {
            if ( ( dht_list[i_2]._is_relocated == true ) && ( dht_list[i_2]._relocated_id == dht_header->id ) )
            {
              dht_list[i_2]._is_relocated = false;
              dht_list[i_2]._relocated_id = 0;
            }
          }
        }
        else
        {
          BRN_DEBUG("DHT (Node:) : no answer for DHT-Operation ! waiting again");

          forward_list[i]._retry = 0;
          fwd_packet = forward_list[i]._fwd_packet->clone();
          send_packet_to_node( fwd_packet, forward_list[i]._ether_add->data() );
        }
         
        if ( ( ( dht_header->code & ROUTING ) == ROUTING ) || ( ( dht_header->code & DHT_DATA ) == DHT_DATA ) )
        {
          delete forward_list[i]._ether_add;
          forward_list[i]._fwd_packet->kill();
          forward_list.erase( forward_list.begin() + i );
        }
      }
    }
  }
}

void
FalconDHT::requestForLostNodes(Vector<EtherAddress> *lost_neighbors)
{
  for ( int i = (lost_neighbors->size() - 1 ); i >= 0; i-- ) {
  	Packet * p = createInfoPacket(ROUTE_REQUEST);
  	if (p != NULL)
    	forward_packet_to_node(p, &((*lost_neighbors)[i]) );
  }
}

//Neighbor-Detection
				
void
FalconDHT::nodeDetection()
{
  Vector<EtherAddress> new_neighbors;

  Vector<EtherAddress> neighbors;
  
  if ( _linkstat == NULL ) return;
  
  _linkstat->get_neighbors(&neighbors);
 
  int j,i,k,l;
 
// new Nodes
  for ( i = 0 ; i < neighbors.size(); i++ )
  {
    for ( j = 0; j < my_neighbors.size(); j++ )
      if ( neighbors[i] == my_neighbors[j] ) break;
    
    
    if ( j == my_neighbors.size() )   // neighbor is new
    {
      new_neighbors.push_back( neighbors[i] );
      my_neighbors.push_back( neighbors[i] );
    }
  }

// removed Nodes
  Vector<EtherAddress> lost_neighbors;

  for ( j = ( my_neighbors.size() - 1 ); j >= 0; j-- )
  {
    i = 0;
    for ( ; i < neighbors.size(); i++ )
    {
      if ( my_neighbors[j] == neighbors[i] ) break;
    }

    if ( i == neighbors.size() )             // my_neighbor is not neighbor anymore, so delete
    {
       lost_neighbors.push_back( my_neighbors[j] );

       for ( k = 0; k < dht_neighbors.size(); k++)  if ( dht_neighbors[k].ether_addr == my_neighbors[j] ) break;

       if ( k < dht_neighbors.size() )                          // erase neighbor, set hop to MAX_HOPS in dht_member
       {
         
         assert( ( k >=0 ) && ( k < dht_neighbors.size() ) );
       
         dht_neighbors.erase(dht_neighbors.begin() + k);

      	 for ( l = 0; l < dht_members.size(); l++ )
	 {
           if ( dht_members[l].ether_addr ==  my_neighbors[j] )
           {
             dht_members[l].hops = MAX_HOPS;
             break;
           }
	 }
       }
      
       assert( ( j >=0 ) && ( j < my_neighbors.size() ) );

       my_neighbors.erase(my_neighbors.begin() + j);
    }
  }
 
  if ( new_neighbors.size() > 0 ) 
    sendNodeInfoToAll(ROUTE_REQUEST);  //send infos to new_neighbours

  if ( lost_neighbors.size() > 0 )
  {
    BRN_DEBUG("DHT: LOST A NEIGHBOR ! _____________________----------------------"); 
    requestForLostNodes(&lost_neighbors);
  }
 
  lost_neighbors.clear();
  new_neighbors.clear();
  neighbors.clear();

}

unsigned int
FalconDHT::getNextDhtEntries(uint8_t **value)
{
  struct dht_node_info *_ac_node_info;
  int i;
  if ( linkprop_table_change )
  {
    for ( i = 0; i < finger_table.size(); i++ )
    {
      _ac_node_info = (struct dht_node_info *)&linkprob_info[i * sizeof(struct dht_node_info)];
      _ac_node_info->_hops = finger_table[i].hops;
      memcpy(&_ac_node_info->_ether_addr[0],finger_table[i].ether_addr.data(),6);
    }
  
    *value = &linkprob_info[0];

    char buffer[250];

    for ( int index = 0;  index < (signed)( max_fingers * 7 ); index ++ )
      sprintf(&buffer[ index * 2 ],"%02x",linkprob_info[index]);
    
    BRN_DEBUG("DHT: send Linkprop-Info: %s", buffer);

    linkprop_table_change = false;

    return( i * sizeof(struct dht_node_info) );
  }

  return(0);

}

void
FalconDHT::processDhtPayloadFromLinkProbe(uint8_t *ptr,unsigned int dht_payload_size)
{
  struct dht_node_info *_ac_node_info;
  int count,i,j;

  if ( dht_payload_size > 0 )
  {
    count = dht_payload_size / sizeof(struct dht_node_info);
  
    char* buffer = new char[ ( 2 * dht_payload_size ) + 1];

    for ( int index = 0;  index < (signed)dht_payload_size; index ++ )
      sprintf(&buffer[ index * 2 ],"%02x",ptr[index]);

    buffer[dht_payload_size] = '\0';
    BRN_DEBUG("DHT: got Linkprop-Info: %s",buffer);


    for ( i = 0; i < count; i++ )
    {
      _ac_node_info = (struct dht_node_info *)&ptr[ i * sizeof(struct dht_node_info)];
  
      BRN_DEBUG("DHT_LINKPROP: Checke node %s", 
        EtherAddress(_ac_node_info->_ether_addr).s().c_str());

      for ( j = 0; j < extra_finger_table.size(); j++ )
      {
        if ( memcmp( extra_finger_table[j].ether_addr.data(), _ac_node_info->_ether_addr , 6) == 0 )
        {
          BRN_DEBUG("DHT_LINKPROP: den kannte ich schon");
          break;
        }
      }

      if ( j == extra_finger_table.size() )
      {
        BRN_DEBUG("DHT_LINKPROP: den kannte ich noch nicht");
        extra_finger_table.push_back(DHTnode(EtherAddress(_ac_node_info->_ether_addr) , _ac_node_info->_hops  , &_net_address , &_subnet_mask));
        BRN_DEBUG("DHT_LINKPROP: eingefgt");
      }
    }

    delete[] buffer;
  }

  return;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//+++++++++++++++++++++++++++++++++++++++++ T I M E R ++++++++++++++++++++++++++++++++++++++++++//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
void
FalconDHT::queue_timer_hook()
{
  struct timeval curr_time;
  click_gettimeofday(&curr_time);

  for ( int i = ( packet_queue.size() - 1 ) ; i >= 0; i--)
  {
    if ( diff_in_ms( packet_queue[i]._send_time, curr_time ) <= 0 )
    {
      Packet *out_packet = packet_queue[i]._p;
      packet_queue.erase(packet_queue.begin() + i);
      BRN_DEBUG("FalconDHT: Queue: Next Packet out!");
      output( 0 ).push( out_packet );
      break;
    }
  }

  int j = get_min_jitter_in_queue();
  if ( j < _min_dist ) j = _min_dist;

//    BRN_DEBUG("FalconDHT: Timer after %d ms", j );
 
  _sendbuffer_timer.schedule_after_msec(j);
}  

long
FalconDHT::diff_in_ms(timeval t1, timeval t2)
{
  long s, us, ms;

  while (t1.tv_usec < t2.tv_usec) {
    t1.tv_usec += 1000000;
    t1.tv_sec -= 1;
  }

  s = t1.tv_sec - t2.tv_sec;

  us = t1.tv_usec - t2.tv_usec;
  ms = s * 1000L + us / 1000L;

  return ms;
}

unsigned int
FalconDHT::get_min_jitter_in_queue()
{
  struct timeval _next_send;
  struct timeval _time_now;
  long next_jitter;

  if ( packet_queue.size() == 0 ) return( 100 );
  else
  {
    _next_send.tv_sec = packet_queue[0]._send_time.tv_sec;
    _next_send.tv_usec = packet_queue[0]._send_time.tv_usec;

    for ( int i = 1; i < packet_queue.size(); i++ )
    {
      if ( ( _next_send.tv_sec > packet_queue[i]._send_time.tv_sec ) ||
           ( ( _next_send.tv_sec == packet_queue[i]._send_time.tv_sec ) && (  _next_send.tv_usec > packet_queue[i]._send_time.tv_usec ) ) )
      {
        _next_send.tv_sec = packet_queue[i]._send_time.tv_sec;
        _next_send.tv_usec = packet_queue[i]._send_time.tv_usec;
      }
    }
   
    click_gettimeofday(&_time_now);

    next_jitter = diff_in_ms(_next_send, _time_now);
   
    if ( next_jitter <= 1 ) return( 1 );
    else return( next_jitter );   

  }
}    

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//+++++++++++++++++++++++++++++++++++++++ H A N D L E R ++++++++++++++++++++++++++++++++++++++++//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
static int 
write_dht_nodes(const String &in_s, Element *, void *,
		      ErrorHandler *)
{
  String s = cp_uncomment(in_s);
/*  bool debug;
  if (!cp_bool(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  id->_debug = debug;*/
  return 0;
}

enum { 
  H_ROUTING_INFO,
  H_TABLE_CONTENT,
  H_STARTUP_TIME,
  H_DEBUG,
  H_FAKE_ARP,
  H_ETHER_ADDRESS,
  H_NETWORK_ADDRESS,
  H_SUBNET_MASK,
  H_MINIMAL_JITTER,
  H_MAXIMAL_JITTER,
  H_JITTER
};

static String
read_param(Element *e, void *thunk)
{
  FalconDHT *fdht = (FalconDHT *)e;
  
  switch ((uintptr_t) thunk)
  {
    case H_ROUTING_INFO :	return ( fdht->routing_info( ) );
    case H_TABLE_CONTENT : return ( fdht->table_info( ) );
    case H_STARTUP_TIME : return String(fdht->_startup_time);
    case H_DEBUG : return String(fdht->_startup_time);
    case H_FAKE_ARP : return String(fdht->_fake_arp_for_simulator);
    case H_ETHER_ADDRESS : return fdht->_me.s();
    case H_NETWORK_ADDRESS: return fdht->_net_address.s();
    case H_SUBNET_MASK: return fdht->_subnet_mask.s();
    case H_MINIMAL_JITTER: return String(fdht->_min_jitter);
    case H_MAXIMAL_JITTER: return String(fdht->_jitter+fdht->_min_jitter);
    case H_JITTER: return String(fdht->_min_dist);
    default: return String();
  }
  return String();
}

static int 
write_param(const String &in_s, Element *e, void * thunk,
          ErrorHandler *errh)
{
  FalconDHT *de = (FalconDHT *)e;
  switch ((uintptr_t) thunk) {
    case H_DEBUG:
    {
      String s = cp_uncomment(in_s);
      int debug;
      if (!cp_integer(s, &debug)) 
        return errh->error("debug parameter must be an integer value between 0 and 4");
      de->_debug = debug;
      break;
    }
    case H_STARTUP_TIME:
    {
      String s = cp_uncomment(in_s);
      int startup_time;
      if (!cp_integer(s, &startup_time)) 
        return errh->error("startup_time parameter must be an integer");
      de->_startup_time = startup_time;
      break;
    }
    case H_FAKE_ARP:
    {
      String s = cp_uncomment(in_s);
      bool fake_arp;
      if (!cp_bool(s, &fake_arp)) 
        return errh->error("fake_arp parameter must be bool");
      de->_fake_arp_for_simulator = fake_arp;
      break;
    }
    case H_ETHER_ADDRESS:
    {
      String s = cp_uncomment(in_s);
      EtherAddress arg;
      if (!cp_ethernet_address(s, &arg)) 
        return errh->error("_me parameter must be an integer");
      de->_me = arg;
      break;
    }
    case H_NETWORK_ADDRESS:
    {
      String s = cp_uncomment(in_s);
      IPAddress arg;
      if (!cp_ip_address(s, &arg)) 
        return errh->error("_net_address parameter must be an integer");
      de->_net_address = arg;
      break;
    }
    case H_SUBNET_MASK:
    {
      String s = cp_uncomment(in_s);
      IPAddress arg;
      if (!cp_ip_address(s, &arg)) 
        return errh->error("_subnet_mask parameter must be an integer");
      de->_subnet_mask = arg;
      break;
    }
    case H_MINIMAL_JITTER:
    {
      String s = cp_uncomment(in_s);
      int debug;
      if (!cp_integer(s, &debug)) 
        return errh->error("_min_jitter parameter must be an integer value between 0 and 4");
      de->_min_jitter = debug;
      break;
    }
    case H_MAXIMAL_JITTER:
    {
      String s = cp_uncomment(in_s);
      int debug;
      if (!cp_integer(s, &debug)) 
        return errh->error("max_jitter parameter must be an integer value between 0 and 4");
      de->_jitter = debug - de->_min_jitter;
      break;
    }
    case H_JITTER:
    {
      String s = cp_uncomment(in_s);
      int debug;
      if (!cp_integer(s, &debug)) 
        return errh->error("_min_dist parameter must be an integer value between 0 and 4");
      de->_min_dist = debug;
      break;
    }
  }
  return 0;
}

void
FalconDHT::add_handlers()
{
  add_read_handler("routing_info", read_param , (void *)H_ROUTING_INFO);
  add_read_handler("table_content", read_param , (void *)H_TABLE_CONTENT);
  add_write_handler("dht_nodes", write_dht_nodes, 0);


  add_read_handler("ether_address", read_param, (void *) H_ETHER_ADDRESS);
  add_write_handler("ether_address", write_param, (void *) H_ETHER_ADDRESS);

  add_read_handler("network_address", read_param, (void *) H_NETWORK_ADDRESS);
  add_write_handler("network_address", write_param, (void *) H_NETWORK_ADDRESS);

  add_read_handler("subnet_mask", read_param, (void *) H_SUBNET_MASK);
  add_write_handler("subnet_mask", write_param, (void *) H_SUBNET_MASK);

  add_read_handler("startup_time", read_param, (void *) H_STARTUP_TIME);
  add_write_handler("startup_time", write_param, (void *) H_STARTUP_TIME);

  add_read_handler("minimal_jitter", read_param, (void *) H_MINIMAL_JITTER);
  add_write_handler("minimal_jitter", write_param, (void *) H_MINIMAL_JITTER);

  add_read_handler("maximal_jitter", read_param, (void *) H_MAXIMAL_JITTER);
  add_write_handler("maximal_jitter", write_param, (void *) H_MAXIMAL_JITTER);

  add_read_handler("min_jitter_between_2_packets", read_param, (void *) H_JITTER);
  add_write_handler("min_jitter_between_2_packets", write_param, (void *) H_JITTER);

  add_read_handler("debug", read_param, 0);  
  add_write_handler("debug", write_param, (void *) H_DEBUG);

  add_read_handler("fake_arp_for_simulator", read_param, (void *) H_FAKE_ARP);
  add_write_handler("fake_arp_for_simulator", write_param, (void *) H_FAKE_ARP);

}

String 
FalconDHT::routing_info(void)
{
  StringAccum sa;

  char digest[16*2 + 1];
  int hops;

  sa << "Routing Info ( Node: " << _me.s() << " )\n";
  sa << "IP-Net: " << _net_address.s() << "\tMask: " << _subnet_mask.s() << "\n";
  sa << "Neighbors:\n";
  for (int i = 0; i < my_neighbors.size(); i++ )
  {
    sa << "  " << my_neighbors[i].s() << "\n";
  }

  sa << "DHT-Neighbors:\n";
  for (int i = 0; i < dht_neighbors.size(); i++ )
  {
    dht_neighbors[i].printDigest(digest);
    hops = dht_neighbors[i].hops;
    sa << "  " << dht_neighbors[i].ether_addr.s() << "\t" << hops << "\t" << digest << "\t" << dht_neighbors[i].start_ip_range.s() << "\n";
  }

  unsigned int finger_pos,node_pos;

  sa << "DHT-Members:\n";
  for (int i = 0; i < dht_members.size(); i++ )
  {
    node_pos = ( dht_members[i].md5_digest[0] * 256 ) + dht_members[i].md5_digest[1];

    dht_members[i].printDigest(digest);
    hops = dht_members[i].hops;
    sa << "  " << dht_members[i].ether_addr.s() << "\t" << hops << "\t" << digest << "\t" << dht_members[i].start_ip_range.s() << "\t" << node_pos << "\n";
  }

  
  finger_pos = _me_md5_digest[0] * 256 + _me_md5_digest[1];

  sa << "Finger-Table (My position is " << finger_pos << " ):\n";

  for (int i = 0; i < finger_table.size(); i++ )
  {
    finger_pos = _me_md5_digest[0] * 256 + _me_md5_digest[1];
    finger_pos = ( finger_pos + ( 1 << i ) ) % max_range;
    node_pos = finger_table[i].md5_digest[0] * 256 + finger_table[i].md5_digest[1];

    finger_table[i].printDigest(digest);
    hops = finger_table[i].hops;
    
    sa << "  " << i << "\t" << finger_pos << "\t" << node_pos << "\t" << finger_table[i].ether_addr.s() << "\t" << hops << "\t" << digest << "\n";
  }
  
  return sa.take_string();
}

String 
FalconDHT::table_info(void)
{
  StringAccum sa;
  IPAddress *_ip;
  EtherAddress *_mac;
  EtherAddress *cor_node;

  uint32_t lease;
  uint32_t *lease_p;

  struct timeval _time_now;
  click_gettimeofday(&_time_now);
  uint32_t rel_time;

  sa << "Table Info ( Node: " << _me.s() << " )\n";
  sa << "Push_backs: " << push_backs << "\n";
  sa << "Message forwards: " << msg_forward << "\n";
  for (int i = 0; i < dht_list.size(); i++ )
  {
    if ( dht_list[i].key_type == TYPE_IP )
    {
      _ip = new IPAddress(dht_list[i].key_data);
      _mac = new EtherAddress(&dht_list[i].value_data[3]);
      lease_p = (uint32_t*)&dht_list[i].value_data[12];
      lease = *lease_p;
      rel_time = lease - _time_now.tv_sec;

      cor_node = find_corresponding_node(&dht_list[i]);
      sa << " IP: " << _ip->s() << "\tMAC: " << _mac->s() << "\tLease: " << rel_time << "\tNode: " << cor_node->s() << "\n";
      delete _ip;
      delete _mac;
    }
    else
    {
      cor_node = find_corresponding_node(&dht_list[i]);
      sa << " Type unbekannt" << "\tNode: " << cor_node->s() << "\n";
    }

  }

  return sa.take_string();
}

#include <click/vector.cc>
template class Vector<FalconDHT::DHTnode>;
template class Vector<FalconDHT::DHTentry>;
template class Vector<FalconDHT::ForwardInfo>;
template class Vector<FalconDHT::BufferedPacket>;
CLICK_ENDDECLS
ELEMENT_REQUIRES(brn_common dht_packet_util dhcp_packet_util)
EXPORT_ELEMENT(FalconDHT)
