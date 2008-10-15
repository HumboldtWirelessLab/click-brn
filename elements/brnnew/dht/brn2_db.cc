#include <click/config.h>
#include "brn2_db.hh"

CLICK_DECLS

BRNDB::BRNDB(Vector<String> _col_names,Vector<int> _col_types)
{
  int i;
  //for( i = 0; i < _col_names.size()
}

BRNDB::~BRNDB()
{
}

/*

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
//      click_chatter("DHT: overwrite with subkey");
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
    
//      click_chatter("DHT: overwrite with sub-key 2");
//      click_chatter("DHT: New: len %d type %d index %d",new_key_len,new_key_type,new_key_valueindex);
    
    
            for( index = DHT_PAYLOAD_OVERHEAD; index < ( dht_list[i].value_len - DHT_PAYLOAD_OVERHEAD );)
            {
              val_len = dht_list[i].value_data[index + 2];

              if ( dht_list[i].value_data[index] == 0 ) //found subkey
              {
                //subkey found, compare key : if equal : overwrite and exit : else jump over
    if ( ( dht_list[i].value_data[index + 2] == new_key_len ) && (dht_list[i].value_data[index + 1] == new_key_type ) )
    {
    
//      click_chatter("Found one subkey");  
    
      if ( memcmp( &(dht_list[i].value_data[index + 3]), &(_dht_entry->value_data[new_key_valueindex + 3]), new_key_len ) == 0 )
      {
//        click_chatter("it's the same");
      
//        click_chatter("%c == %c",dht_list[i].value_data[index + 3],_dht_entry->value_data[new_key_valueindex + 3]);
      
        old_index_start = index;
        old_index_end = index + new_key_len + DHT_PAYLOAD_OVERHEAD;
        
        //search for next subkey or end
        while ( ( old_index_end < ( dht_list[i].value_len - DHT_PAYLOAD_OVERHEAD ) ) && ( dht_list[i].value_data[old_index_end] != 0 ) ) 
        {
         
//         click_chatter("%d jump",old_index_end);
          old_index_end += dht_list[i].value_data[old_index_end + 2] + DHT_PAYLOAD_OVERHEAD;
//         click_chatter("%d jumpdest",old_index_end);
          
        }
        
        if ( old_index_end >= ( dht_list[i].value_len - DHT_PAYLOAD_OVERHEAD ) ) // subkey was the last
        {
//           click_chatter("subkey was last");
        
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
//           click_chatter("subkey middler");
         
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
//        click_chatter("dht: but its another one");
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
      
//      click_chatter("dht:Subkey not found! insert");
      
      if ( index >= ( dht_list[i].value_len - DHT_PAYLOAD_OVERHEAD ) )
      {
              //subkey not found : insert
        
//        click_chatter("insert");
        
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
//        click_chatter("DHT: overwrite with subkey");
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
    
//        click_chatter("DHT: overwrite with sub-key 2");
//        click_chatter("DHT: New: len %d type %d index %d",new_key_len,new_key_type,new_key_valueindex);
    
    
              for( index = DHT_PAYLOAD_OVERHEAD; index < ( dht_list[i].value_len - DHT_PAYLOAD_OVERHEAD );)
              {
                val_len = dht_list[i].value_data[index + 2];

                if ( dht_list[i].value_data[index] == 0 ) //found subkey
                {
                  //subkey found, compare key : if equal : overwrite and exit : else jump over
          if ( ( dht_list[i].value_data[index + 2] == new_key_len ) && (dht_list[i].value_data[index + 1] == new_key_type ) )
      {
    
//        click_chatter("Found one subkey");  
    
        if ( memcmp( &(dht_list[i].value_data[index + 3]), &(_dht_entry->value_data[new_key_valueindex + 3]), new_key_len ) == 0 )
        {
//          click_chatter("it's the same");
      
//          click_chatter("%c == %c",dht_list[i].value_data[index + 3],_dht_entry->value_data[new_key_valueindex + 3]);
      
          old_index_start = index;
          old_index_end = index + new_key_len + DHT_PAYLOAD_OVERHEAD;
        
          //search for next subkey or end
          while ( ( old_index_end < ( dht_list[i].value_len - DHT_PAYLOAD_OVERHEAD ) ) && ( dht_list[i].value_data[old_index_end] != 0 ) ) 
          {
         
//            click_chatter("%d jump",old_index_end);
            old_index_end += dht_list[i].value_data[old_index_end + 2] + DHT_PAYLOAD_OVERHEAD;
//            click_chatter("%d jumpdest",old_index_end);
          
          }
        
          if ( old_index_end >= ( dht_list[i].value_len - DHT_PAYLOAD_OVERHEAD ) ) // subkey was the last
          {
//            click_chatter("subkey was last");
        
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
//            click_chatter("subkey middler");
         
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
//          click_chatter("dht: but its another one");
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

*/
#include <click/vector.cc>
template class Vector<BRNDB::DBrow>;
template class Vector<String>;
template class Vector<int>;
template class Vector<char*>;


CLICK_ENDDECLS
ELEMENT_PROVIDES(BRNDB)
