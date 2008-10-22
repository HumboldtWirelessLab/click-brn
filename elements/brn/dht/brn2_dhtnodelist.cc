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
  DHTnode *node;

  if ( _new_node != NULL )
  {
    node = get_dhtnode(_new_node);
    if ( node != NULL ) erase_dhtnode(&(node->_ether_addr));

    _nodelist.push_back(_new_node);
  }

  return 0;
}

DHTnode* DHTnodelist::get_dhtnode(DHTnode *_search_node)
{
  return get_dhtnode(&(_search_node->_ether_addr));
}

DHTnode* DHTnodelist::get_dhtnode(EtherAddress *_etheradd)
{
  int i;

  for( i = 0; i < _nodelist.size(); i++)
    if ( memcmp(_nodelist[i]->_ether_addr.data(), _etheradd->data(), 6) == 0 ) break;

  if ( i < _nodelist.size() )
    return _nodelist[i];

  return NULL;
}

int DHTnodelist::erase_dhtnode(EtherAddress *_etheradd)
{
  int i;

  for( i = 0; i < _nodelist.size(); i++)
    if ( memcmp(_nodelist[i]->_ether_addr.data(), _etheradd->data(), 6) == 0 )
    {
      _nodelist.erase(_nodelist.begin() + i);
      break;
    }

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
