#include <click/config.h>
#include <click/element.hh>
#include <click/etheraddress.hh>

#include "elements/brn2/standard/brn_md5.hh"

#include "dhtnode.hh"
#include "dhtnodelist.hh"


CLICK_DECLS

extern "C" {
  static int bucket_sorter(const void *va, const void *vb, void */*thunk*/) {
      DHTnode *a = *((DHTnode **)va), *b = *((DHTnode **)vb);

      return MD5::hexcompare(a->_md5_digest, b->_md5_digest);
  }
}

extern "C" {
  static int last_ping_sorter(const void *va, const void *vb, void */*thunk*/) {
    DHTnode *a = *((DHTnode **)va), *b = *((DHTnode **)vb);

    return ( a->_last_ping > b->_last_ping );
  }
}

extern "C" {
  static int age_sorter(const void *va, const void *vb, void */*thunk*/) {
    DHTnode *a = *((DHTnode **)va), *b = *((DHTnode **)vb);

    return ( a->_age > b->_age );
  }
}

DHTnodelist::DHTnodelist()
{
}

DHTnodelist::~DHTnodelist()
{
  _nodelist.clear();
  _nodemap_ea.clear();
}

int DHTnodelist::add_dhtnode(DHTnode *_new_node)
{
  DHTnode *node;

  if ( _new_node != NULL )
  {
    node = get_dhtnode(_new_node);
    if ( node != NULL ) erase_dhtnode(&(node->_ether_addr));

    _nodelist.push_back(_new_node);
    _nodemap_ea.insert(_new_node->_ether_addr,_new_node);
  }

  return 0;
}

DHTnode*
DHTnodelist::swap_dhtnode(DHTnode *_node, int i)
{
  DHTnode *old = NULL;

  if ( ( _node != NULL ) && ( i >= 0 ) && ( i < _nodelist.size() ) ) {
    old = _nodelist[i];
    _nodelist[i] = _node;
  }

  return old;
}

int
DHTnodelist::get_index_dhtnode(DHTnode *_search_node)
{
  for( int i = 0; i < _nodelist.size(); i++)
    if ( memcmp(_nodelist[i]->_ether_addr.data(), _search_node->_ether_addr.data(), 6) == 0 ) return i;

  return -1;
}

int
DHTnodelist::erase_dhtnode(EtherAddress *_etheradd)
{
  DHTnode *node = _nodemap_ea.find(*_etheradd);

  if ( node ) {
    _nodemap_ea.erase(*_etheradd);

    for( int i = 0; i < _nodelist.size(); i++)
      if ( memcmp(_nodelist[i]->_ether_addr.data(), _etheradd->data(), 6) == 0 )
      {
        node = _nodelist[i];
        delete node;

        _nodelist.erase(_nodelist.begin() + i);
        break;
      }
  }

  return 0;
}

void
DHTnodelist::remove_dhtnode(int i)
{
  if ( ( i < _nodelist.size() ) && ( i >= 0 ) ) {
    _nodemap_ea.erase(_nodelist[i]->_ether_addr);
    _nodelist.erase(_nodelist.begin() + i);
  }
}

void
DHTnodelist::remove_dhtnode(DHTnode *node)
{
  remove_dhtnode(get_index_dhtnode(node));
}

void DHTnodelist::sort()
{
  click_qsort(_nodelist.begin(), _nodelist.size(), sizeof(DHTnode*), bucket_sorter);
}

void DHTnodelist::sort_last_ping()
{
  click_qsort(_nodelist.begin(), _nodelist.size(), sizeof(DHTnode*), last_ping_sorter);
}

void DHTnodelist::sort_age()
{
  click_qsort(_nodelist.begin(), _nodelist.size(), sizeof(DHTnode*), age_sorter);
}

void DHTnodelist::clear()
{
  _nodelist.clear();
  _nodemap_ea.clear();
}

void DHTnodelist::clear(int start_index, int end_index)
{
  if ( end_index > _nodelist.size() ) end_index = _nodelist.size();

  for( int i = end_index - 1; i >= start_index; i--) {
    _nodemap_ea.erase(_nodelist[i]->_ether_addr);
    _nodelist.erase(_nodelist.begin() + i);
  }
}

void DHTnodelist::del()
{
  DHTnode *node;

  for( int i = _nodelist.size()-1; i >= 0; i--)
  {
    node = _nodelist[i];
    delete node;
  }

  clear();
}

DHTnode*
DHTnodelist::get_dhtnode_oldest_age()
{
  DHTnode *oldestnode = NULL;
  DHTnode *acnode = NULL;

  if ( _nodelist.size() > 0 ) {
    oldestnode = _nodelist[0];
    for( int i = 1; i < _nodelist.size(); i++ )
    {
      acnode = _nodelist[i];
      if ( acnode->_age < oldestnode->_age ) // < since we don't store the age, it more the birthday
        oldestnode = acnode;
    }
  }

  return oldestnode;
}

DHTnode*
DHTnodelist::get_dhtnode_oldest_ping()
{
  DHTnode *oldestnode = NULL;
  DHTnode *acnode = NULL;

  if ( _nodelist.size() > 0 ) {
    oldestnode = _nodelist[0];
    for( int i = 1; i < _nodelist.size(); i++ )
    {
      acnode = _nodelist[i];
      if ( acnode->_last_ping < oldestnode->_last_ping ) { // < since we don't store the age, its the date
        //click_chatter("%s < %s",acnode->_last_ping.unparse().c_str(),oldestnode->_last_ping.unparse().c_str());
        oldestnode = acnode;
      }
    }
  }

  return oldestnode;
}

DHTnodelist *
DHTnodelist::get_dhtnodes_oldest_age(int number)
{
  DHTnodelist *newlist = new DHTnodelist();

  for( int i = 1; i < _nodelist.size(); i++ )    //copy to new list
    newlist->add_dhtnode(_nodelist[i]);          //TODO: try to copy the vector (Performance)

  newlist->sort_age();

  for ( int i = newlist->size() - 1; i >= number; i-- )
    newlist->remove_dhtnode(i);

  return newlist;
}

DHTnodelist *
DHTnodelist::get_dhtnodes_oldest_ping(int number)
{
  DHTnodelist *newlist = new DHTnodelist();

  for( int i = 1; i < _nodelist.size(); i++ )    //copy to new list
    newlist->add_dhtnode(_nodelist[i]);          //TODO: try to copy the vector (Performance)

  newlist->sort_last_ping();

  for ( int i = newlist->size() - 1; i >= number; i-- )
    newlist->remove_dhtnode(i);

  return newlist;
}

bool
DHTnodelist::contains(DHTnode *node)
{
  if ( node == NULL ) return true;

  return ( get_dhtnode(node) != NULL );
}

#include <click/vector.cc>
template class Vector<DHTnode*>;

CLICK_ENDDECLS
ELEMENT_REQUIRES(DHTnode)
ELEMENT_PROVIDES(DHTnodelist)
