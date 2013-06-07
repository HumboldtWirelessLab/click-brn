#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>

#include "clustering.hh"

CLICK_DECLS

Clustering::Clustering()
{
}

Clustering::~Clustering()
{
}

void
Clustering::init()
{
  BRNElement::init();
  _own_cluster._cluster_id = 0;
  _own_cluster._clusterhead = EtherAddress();
}

String
Clustering::clustering_info()
{
  this->clustering_process();

  StringAccum sa;
  sa << "<CLUSTERING_INFO time=\"" << Timestamp::now() << "\">"
	 << "\n\t<KNODE>"
	 << "\n\t\t<NAME>" << _node_identity->getNodeName() << "</NAME>"
	 << "\n\t\t<ETHERADDRESS>" << _node_identity->getMasterAddress()->unparse() << "</ETHERADDRESS>"
	 << "\n\t</KNODE>"
	 << "\n\t<CLUSTER>"
	 << "\n\t\t<ID>" << _own_cluster._cluster_id << "</ID>"
	 << "\n\t\t<CLUSTERHEAD>" << _own_cluster._clusterhead.unparse() << "</CLUSTERHEAD>"
	 << "\n\t\t<MEMBER>";
  	  for(Vector<EtherAddress>::iterator i=_own_cluster._member.begin(); i!= _own_cluster._member.end(); i++) {
  		  sa << "\n\t\t\t<ETHERADDRESS>" << (*i).unparse() << "\n\t\t\t</ETHERADDRESS>";
  	  }
  sa << "\n\t\t</MEMBER>"
  	 << "\n\t</CLUSTER>";
	  for(ClusterListIter cli=_known_clusters.begin(); cli!= _known_clusters.end(); cli++) {
  		  sa << "\n\t<OTHERCLUSTER>"
  			 << "\n\t\t<ID>" << (*cli)->_cluster_id << "</ID>"
  			 << "\n\t\t<CLUSTERHEAD>" << (*cli)->_clusterhead.unparse() << "</CLUSTERHEAD>"
  			 << "\n\t\t<MEMBER>";
  		  	  for(Vector<EtherAddress>::iterator i=(*cli)->_member.begin(); i!= (*cli)->_member.end(); i++) {
  		  		  sa << "\n\t\t\t<ETHERADDRESS>" << (*i).unparse() << "\n\t\t\t</ETHERADDRESS>";
  		  	  }
  		  sa << "\n\t\t</MEMBER>"
  		     << "\n\t</OTHERCLUSTER>";
  	  }
  sa <<	"\n</CLUSTERING_INFO>";

  return sa.take_string();
}

static String
read_clustering_info(Element *e, void */*thunk*/)
{
  return ((Clustering *)e)->clustering_info();
}

static int
write_clusterhead(const String &in_s, Element *e, void */*vparam*/, ErrorHandler */*errh*/)
{
  EtherAddress ch;
  Clustering *f = (Clustering *)e;
  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  cp_ethernet_address(args[0], &ch); //TODO: exception handle
  f->_own_cluster._clusterhead = ch;

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
