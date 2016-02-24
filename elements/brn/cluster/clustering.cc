#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>

#include "clustering.hh"

CLICK_DECLS

Clustering::Clustering(): _linkstat(NULL), _node_identity(NULL), _own_cluster(NULL), _known_clusters()
{

}

Clustering::~Clustering()
{

}

void
Clustering::init()
{
  BRNElement::init();
}

String
Clustering::clustering_info()
{
  StringAccum sa;

  if(_own_cluster!=NULL) {
	  sa << "<CLUSTERING_INFO time=\"" << Timestamp::now() << "\">"
		 << "\n\t<KNODE>"
		 << "\n\t\t<NAME>" << _node_identity->getNodeName() << "</NAME>"
		 << "\n\t\t<ETHERADDRESS>" << _node_identity->getMasterAddress()->unparse() << "</ETHERADDRESS>"
		 << "\n\t</KNODE>";
/*
	  sa << "\n\t<CLUSTER>"
		 << "\n\t\t<ID>" << _own_cluster->_cluster_id << "</ID>"
		 << "\n\t\t<CLUSTERHEAD>" << _own_cluster->_clusterhead.unparse() << "</CLUSTERHEAD>"
		 << "\n\t\t<MEMBER>";
		 for(Vector<EtherAddress>::iterator j=_own_cluster->_member.begin(); j!= _own_cluster->_member.end(); j++) {
			  sa << "\n\t\t\t<ETHERADDRESS>" << (*j).unparse() << "</ETHERADDRESS>";
		  }
		  sa << "\n\t\t</MEMBER>"
		 << "\n\t</CLUSTER>";
*/

	  for(ClusterListIter i=_known_clusters.begin(); i!=_known_clusters.end(); ++i) {
		  if((*i)->_cluster_id==_own_cluster->_cluster_id ) {
			  sa << "\n\t<CLUSTER>"
				 << "\n\t\t<ID>" << (*i)->_cluster_id << "</ID>"
				 << "\n\t\t<CLUSTERHEAD>" << (*i)->_clusterhead.unparse() << "</CLUSTERHEAD>"
				 << "\n\t\t<MEMBER>";
				  for(Vector<EtherAddress>::iterator j=(*i)->_member.begin(); j!= (*i)->_member.end(); ++j) {
					  sa << "\n\t\t\t<ETHERADDRESS>" << (*j).unparse() << "</ETHERADDRESS>";
				  }
			  sa << "\n\t\t</MEMBER>"
				 << "\n\t</CLUSTER>";
		  }
	  }

	  for(ClusterListIter cli=_known_clusters.begin(); cli!= _known_clusters.end(); ++cli) {
		  if((*cli)->_cluster_id!=_own_cluster->_cluster_id ) {
			  sa << "\n\t<OTHERCLUSTER>"
				 << "\n\t\t<ID>" << (*cli)->_cluster_id << "</ID>"
				 << "\n\t\t<CLUSTERHEAD>" << (*cli)->_clusterhead.unparse() << "</CLUSTERHEAD>"
				 << "\n\t\t<MEMBER>";
				  for(Vector<EtherAddress>::iterator i=(*cli)->_member.begin(); i!= (*cli)->_member.end(); ++i) {
					  sa << "\n\t\t\t<ETHERADDRESS>" << (*i).unparse() << "</ETHERADDRESS>";
				  }
			  sa << "\n\t\t</MEMBER>"
				 << "\n\t</OTHERCLUSTER>";
		  }
	  }
	  sa <<	"\n</CLUSTERING_INFO>";
  }

  return sa.take_string();
}

void Clustering::readClusterInfo( EtherAddress node, uint32_t cID, EtherAddress cHead ) {
	// remove old info
	for( ClusterListIter i=_known_clusters.begin(); i!=_known_clusters.end(); ++i ) {
		for( Vector<EtherAddress>::iterator j=(*i)->_member.begin(); j!=(*i)->_member.end(); ++j ) {
			if( (*j).hashcode()==node.hashcode() ) {
				if( (*i)->_cluster_id != cID ) {
					(*i)->_member.erase( j );
					break;
				}
			}
		}
	}

	// insert new info
	bool inserted = false;

	for( ClusterListIter i=_known_clusters.begin(); i!=_known_clusters.end(); ++i ) {
		if( (*i)->_cluster_id == cID ) {
			bool found = false;

			for( Vector<EtherAddress>::iterator j=(*i)->_member.begin(); j!=(*i)->_member.end(); ++j ) {
				StringAccum st;

				if( (*j).hashcode()==node.hashcode() ) {
					found = true;
					inserted=true;
				}
			}
			if(!found) {
				(*i)->_member.push_back( node );
				inserted=true;
				break;
			}
		}
	}

	if(!inserted) {
		Cluster *tmp = new Cluster();
		tmp->_cluster_id = cID;
		tmp->_clusterhead = cHead;
		tmp->_member.push_back( node );
		_known_clusters.push_back( tmp );
	}

	// TODO: delete empty cluster
	for( ClusterList::iterator i=_known_clusters.begin(); i!=_known_clusters.end(); ++i ) {
		if( (*i)->_member.size()==0 ) {
		    Cluster *t = (*i);
			i = _known_clusters.erase( i );
			--i;
			delete t;
		}
	}
}

static String
read_clustering_info(Element *e, void */*thunk*/)
{
  return (dynamic_cast<Clustering *>(e))->clustering_info();
}

static int
write_clusterhead(const String &in_s, Element *e, void */*vparam*/, ErrorHandler */*errh*/)
{
  EtherAddress ch;
  Clustering *f = reinterpret_cast<Clustering *>(e);
  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  cp_ethernet_address(args[0], &ch); //TODO: exception handle
  f->_own_cluster->_clusterhead = ch;

  return 0;
}

void
Clustering::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("clustering_info", read_clustering_info, (void *) 0);
  add_write_handler("clusterhead", write_clusterhead, (void *) 0);
}

ELEMENT_PROVIDES(Clustering)
CLICK_ENDDECLS
