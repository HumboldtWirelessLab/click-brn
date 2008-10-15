#include <click/config.h>
#include <click/element.hh>
#include <click/etheraddress.hh>
#include "brn2_dhtnode.hh"
#include "brn2_dhtnodelist.hh"
#include "brn2_md5.hh"


CLICK_DECLS

class DHTnode;

extern "C" {
  static int bucket_sorter(const void *va, const void *vb) {
      DHTnode *a = (DHTnode *)va, *b = (DHTnode *)vb;

      return DHTnode::MD5helper::hexcompare(a->_md5_digest, b->_md5_digest);
  }
}

int DHTnodelist::add_dhtnode(DHTnode *_new_node)
{
  int i,index;

  /*index = -1;
  for( i = 0; ( i < _nodelist.size() ) && ( index == -1 ); i++)
  {
    if ( memcmp(_nodelist[i]->ether_addr.data(), _etheradd, 6) == 0 ) index = i;
  }
*/
  return 0;
}

DHTnode* DHTnodelist::get_dhtnode(DHTnode *_search_node)
{
  return 0;
}

DHTnode* DHTnodelist::get_dhtnode(EtherAddress *_etheradd)
{
  int i,index;

  index = -1;
  for( i = 0; ( i < _nodelist.size() ) && ( index == -1 ); i++)
  {
    if ( memcmp(_nodelist[i]->_ether_addr.data(), _etheradd->data(), 6) == 0 ) index = i;
  }

  if ( i < _nodelist.size() )
    return _nodelist[i];

  return NULL;
}

int DHTnodelist::erase_dhtnode(EtherAddress *_etheradd)
{
  return 0;
}

int DHTnodelist::size()
{
  return _nodelist.size();
}

void DHTnodelist::sort()
{
  click_qsort(_nodelist.begin(), _nodelist.size(), sizeof(DHTnode*), bucket_sorter);
}

#include <click/vector.cc>
template class Vector<DHTnode*>;

CLICK_ENDDECLS
ELEMENT_PROVIDES(DHTnodelist)
