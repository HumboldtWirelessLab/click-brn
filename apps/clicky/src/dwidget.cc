#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include "dwidget.hh"
#include "dstyle.hh"
#include "wdiagram.hh"
#include "ddecor.hh"
#include <click/userutils.hh>
#include <click/confparse.hh>
#include <click/bitvector.hh>
#include <clicktool/processingt.hh>
#include <clicktool/elementmap.hh>
#include "scopechain.hh"
#include <list>
#include <math.h>
#include "crouter.hh"
#include "whandler.hh"
#include "transform.hh"
extern "C" {
#include "support.h"
}
namespace clicky {

static inline void cairo_rel_line_to_point(cairo_t *cr, const point &p)
{
    cairo_rel_line_to(cr, p.x(), p.y());
}

static inline void cairo_curve_to_points(cairo_t *cr, const point &p0, const point &p1, const point &p2)
{
    cairo_curve_to(cr, p0.x(), p0.y(), p1.x(), p1.y(), p2.x(), p2.y());
}

dcontext::dcontext(crouter *cr_, PangoLayout *pl_, cairo_t *cairo_,
		   unsigned generation_, int scale_step_, double scale_)
    : cr(cr_), pl(pl_), cairo(cairo_), generation(generation_),
      scale_step(scale_step_), scale(scale_), penumbra(1)
{
}

unsigned dcontext::step_generation()
{
    static unsigned the_generation;
    return ++the_generation;
}

void dcontext::set_font_description(const String &font)
{
    if (pl_font != font) {
	PangoFontDescription *font_desc = pango_font_description_from_string(font.c_str());
	pango_layout_set_font_description(pl, font_desc);
	pango_font_description_free(font_desc);
	pl_font = font;
    }
}


String dwidget::unparse() const
{
    if (_type == dw_elt)
	return static_cast<const delt *>(this)->flat_name();
    else {
	const dconn *c = static_cast<const dconn *>(this);
	StringAccum sa;
	sa << c->from_elt()->flat_name() << '[' << c->from_port()
	   << "]->[" << c->to_port() << ']' << c->to_elt()->flat_name();
	return sa.take_string();
    }
}


delt *delt::create(ElementT *e, delt *parent,
		   crouter *cr, ProcessingT *processing,
		   HashTable<String, delt *> &collector,
		   ScopeChain &chain, int &z_index)
{
    // see also create_split()
    delt *de = new delt(parent, z_index);
    z_index++;

    de->_e = e;
    de->_processing_code = processing->decorated_processing_code(e);
    de->_flow_code = processing->flow_code(e);

    de->_flat_name = chain.flat_name(e->name());
    de->_flat_config = chain.resolved_config(e->config());
    collector[de->_flat_name] = de;

    ElementClassT *resolved_type = chain.resolved_type(e);
    de->_primitive = resolved_type->primitive();

    // initial styles
    int x;
    de->_dess = cr->ccss()->elt_size_style(cr, de, &x);
    de->_dess_sensitivity = x;
    de->_des = cr->ccss()->elt_style(cr, de, &x);
    de->_des_sensitivity = x;

    if (resolved_type->cast_router()) {
	chain.enter_element(e);
	ProcessingT subprocessing(*processing, e);
	de->create_elements(cr, subprocessing.router(), &subprocessing,
			    collector, chain, z_index);
	chain.pop_element();
    }

    return de;
}

delt *delt::create_split(int split_type, int *z_index_ptr)
{
    int z_index;
    if (z_index_ptr) {
	z_index = *z_index_ptr;
	(*z_index_ptr)++;
    } else
	z_index = this->z_index();

    delt *se = new delt(_parent, z_index);

    se->_e = _e;
    se->_resolved_router = _resolved_router;
    se->_processing_code = _processing_code;
    se->_flow_code = _flow_code;
    se->_flat_name = _flat_name;
    se->_flat_config = _flat_config;

    se->_split_type = split_type;
    if (!_split)
	_split = this;
    se->_split = _split;
    _split = se;

    _parent->_elt.push_back(se);
    return se;
}

delt::~delt()
{
    ddecor::free_list(_decor);
    while (_elt.size()) {
	delete _elt.back();
	_elt.pop_back();
    }
    while (_conn.size()) {
	delete _conn.back();
	_conn.pop_back();
    }
    delete[] _portoff[0];
    delete[] _portoff[1];
    delete[] _port_text_offsets;
}

void delt::create_elements(crouter *cr, RouterT *router,
			   ProcessingT *processing,
			   HashTable<String, delt *> &collector,
			   ScopeChain &chain, int &z_index)
{
    assert(!_resolved_router && _elt.size() == 0);
    _resolved_router = router;

    // create elements (_elt[i]->eindex() == i)
    // make sure first nelements() slots are occupied
    _elt.resize(router->nelements(), 0);
    for (int i = 0; i < router->nelements(); ++i) {
	delt *e = delt::create(router->element(i), this,
			       cr, processing, collector, chain, z_index);
	// Can't combine with previous statement in case _elt changes!
	_elt[i] = e;
    }

    // style children
    for (int i = 0; i < _resolved_router->nelements(); ++i)
	_elt[i]->reccss(cr, dsense_always, &z_index);
}

void delt::create_connections(std::vector<delt_conn> &cc, crouter *cr, int &z_index) const
{
    bool input_output = (root() || _des->display != dedisp_expanded);

    // add local connections
    for (RouterT::conn_iterator it = _resolved_router->begin_connections();
	 it != _resolved_router->end_connections(); ++it)
	if (input_output || (it->from_eindex() != 0 && it->to_eindex() != 1))
	    cc.push_back(delt_conn(_elt[it->from_eindex()], it->from_port(),
				   _elt[it->to_eindex()], it->to_port()));

    // add expanded child connections
    for (std::vector<delt *>::const_iterator it = _elt.begin();
	 it != _elt.end(); ++it)
	if ((*it)->_elt.size() && (*it)->_display == dedisp_expanded)
	    (*it)->create_connections(cc, cr, z_index);
	else if ((*it)->_elt.size() && (*it)->_display == dedisp_open)
	    (*it)->create_connections(cr, z_index);
}

void delt::create_connections(crouter *cr, int &z_index)
{
    for (std::vector<dconn *>::iterator it = _conn.begin();
	 it != _conn.end(); ++it)
	delete *it;
    _conn.clear();

    if (!root() && _des->display != dedisp_open)
	return;

    // create initial connections
    std::vector<delt_conn> cc;
    create_connections(cc, cr, z_index);

    // expand connections
    Bitvector bv;
    while (cc.size()) {
	delt_conn c = cc.back();
	cc.pop_back();

	if (c.from->display() == dedisp_none
	    || c.to->display() == dedisp_none)
	    continue;

	if (c.from->display() == dedisp_expanded) {
	    RouterT *subr = c.from->_resolved_router;
	    for (RouterT::conn_iterator it = subr->begin_connections_to(PortT(subr->element(1), c.from_port));
		 it != subr->end_connections(); ++it)
		cc.push_back(delt_conn(c.from->_elt[it->from_eindex()],
				       it->from_port(), c.to, c.to_port));
	    continue;
	}

	if (c.to->display() == dedisp_expanded) {
	    RouterT *subr = c.to->_resolved_router;
	    for (RouterT::conn_iterator it = subr->begin_connections_from(PortT(subr->element(0), c.to_port));
		 it != subr->end_connections(); ++it)
		cc.push_back(delt_conn(c.from, c.from_port,
				       c.to->_elt[it->to_eindex()],
				       it->to_port()));
	    continue;
	}

	if (c.from->eindex() == 0 && !c.from->parent()->root()
	    && c.from->parent()->display() == dedisp_expanded) {
	    delt *pp = c.from->parent()->parent();
	    RouterT *subr = pp->_resolved_router;
	    for (RouterT::conn_iterator it = subr->begin_connections_to(PortT(c.from->parent()->_e, c.from_port));
		 it != subr->end_connections(); ++it)
		cc.push_back(delt_conn(pp->_elt[it->from_eindex()], it->from_port(), c.to, c.to_port));
	    continue;
	}

	if (c.to->eindex() == 1 && !c.to->parent()->root()
	    && c.to->parent()->display() == dedisp_expanded) {
	    delt *pp = c.to->parent()->parent();
	    RouterT *subr = pp->_resolved_router;
	    for (RouterT::conn_iterator it = subr->begin_connections_from(PortT(c.to->parent()->_e, c.to_port));
		 it != subr->end_connections(); ++it)
		cc.push_back(delt_conn(c.from, c.from_port, pp->_elt[it->to_eindex()], it->to_port()));
	    continue;
	}

	if (c.from->display() != dedisp_passthrough
	    && c.to->display() == dedisp_passthrough) {
	    ProcessingT::forward_flow(c.to->_flow_code, c.to_port, &bv, c.to->noutputs());
	    RouterT *subr = c.to->_e->router();
	    for (int p = 0; p < c.to->noutputs(); ++p)
		if (bv[p]) {
		    for (RouterT::conn_iterator it = subr->begin_connections_from(PortT(c.to->_e, p));
			 it != subr->end_connections(); ++it)
			cc.push_back(delt_conn(c.from, c.from_port,
					       c.to->parent()->_elt[it->to_eindex()],
					       it->to_port()));
		}
	    continue;
	}

	if (!dedisp_visible(c.from->display())
	    || !dedisp_visible(c.to->display()))
	    continue;

	_conn.push_back(new dconn(c.from, c.from_port, c.to, c.to_port, z_index));
	z_index++;
    }
}


String delt::display_name() const
{
    delt *p = parent();
    // can't depend on p->_display being set yet
    if (!p->root() && p->_des->display == dedisp_expanded)
	return p->display_name() + "/" + name();
    else
	return name();
}


/*****
 *
 * SCC layout
 *
 */

class delt::layoutelt : public rectangle { public:
    int scc;
    std::vector<int> scc_contents;

    bool visited;
    int active;

    int posrel;
    int compact_prev;
    int compact_next;

    rectangle rgroup;

    layoutelt(int n, double width, double height)
	: rectangle(0, 0, width, height), scc(n), visited(false), active(-1),
	  posrel(n), compact_prev(-1), compact_next(-1),
	  rgroup(0, 0, width, height) {
    }
};

void delt::layout_one_scc(RouterT *router, std::vector<layoutelt> &layinfo, const Bitvector &connlive, int scc)
{
    const std::vector<int> &scc_contents = layinfo[scc].scc_contents;

    // find compact connections
    for (std::vector<int>::const_iterator iter = scc_contents.begin();
	 iter != scc_contents.end(); ++iter) {
	ElementT *x = router->element(*iter);
	int c;
	if (x->ninputs() != 1
	    || (c = router->find_connection_id_to(PortT(x, 0))) < 0
	    || !connlive[c])
	    continue;
	const PortT &out = router->connection(c).from();
	if (router->find_connection_id_from(out) < 0
	    || out.port != 0
	    || out.element->noutputs() > 2)
	    continue;
	if (out.element->noutputs() == 2) {
	    const String &pc = out.element->type()->processing_code();
	    if (pc != "a/ah" && pc != "l/lh")
		continue;
	    // check that x[1] has <= 1 connection (!= 1 should not happen,
	    // but erroneous configurations could easily make it)
	    if (router->find_connection_id_from(PortT(out.element, 1)) == -2)
		continue;
	}

	// actually position
	int partner = out.eindex();
	layinfo[*iter].posrel = layinfo[partner].posrel;
	layinfo[*iter]._x = layinfo[partner].center_x() - layinfo[*iter].width()/2;
	if (out.element->noutputs() == 2)
	    layinfo[*iter]._y = layinfo[partner].y2() + 10;
	else
	    layinfo[*iter]._y = layinfo[partner].y2() + 4;
	layinfo[layinfo[*iter].posrel].rgroup |= layinfo[*iter];
	layinfo[partner].compact_next = *iter;
	layinfo[*iter].compact_prev = partner;
    }

    // lay out compact connections
    for (std::vector<int>::const_iterator iter = scc_contents.begin();
	 iter != scc_contents.end(); ++iter)
	while (layinfo[layinfo[*iter].posrel].posrel != layinfo[*iter].posrel) {
	    int g = layinfo[*iter].posrel, g2 = layinfo[g].posrel;
	    layinfo[*iter].shift(layinfo[g].x(), layinfo[g].y());
	    layinfo[g].rgroup |= layinfo[*iter];
	    layinfo[*iter].posrel = g2;
	}

    //
    // print SCCs
#if 0
    for (std::vector<int>::const_iterator iter = scc_contents.begin();
	 iter != scc_contents.end(); ++iter)
	if (layinfo[*iter].posrel == *iter) {
	    fprintf(stderr, "COMPACT %d :", *iter);
	    for (std::vector<int>::const_iterator iterx = scc_contents.begin(); iterx != scc_contents.end(); ++iterx)
		if (layinfo[*iterx].posrel == *iter)
		    fprintf(stderr, " %s", router->ename(*iterx).c_str());
	    fprintf(stderr, "\n");
	}
#endif
}

void delt::position_contents_scc(RouterT *router)
{
    // initialize DFS
    Bitvector connlive(router->nconnections(), true);
    std::vector<layoutelt> layinfo;
    for (int i = 0; i < router->nelements(); i++)
	layinfo.push_back(layoutelt(i, _elt[i]->width(), _elt[i]->height()));

    // DFS to find strongly connected components and break cycles
    std::vector<int> connpath, eltpath, connpos;
    Vector<int> conns;

    for (int i = 0; i < router->nelements(); ++i) {
	if (layinfo[i].visited)
	    continue;
	assert(connpath.size() == 0 && eltpath.size() == 0 && connpos.size() == 0);
	eltpath.push_back(i);
	connpath.push_back(-1);
	connpos.push_back(-1);
	layinfo[i].active = 0;
	assert(layinfo[i].scc == i);
	int cur_scc = i;

	// loop over connections
	while (connpos.size()) {
	    router->find_connections_from(router->element(eltpath.back()), conns);

	  more_connections:
	    // skip dead connections
	    assert(connpos.back() < conns.size());
	    do {
		++connpos.back();
	    } while (connpos.back() < conns.size() && !connlive[conns[connpos.back()]]);

	    // no more connections: backtrack
	    if (connpos.back() == conns.size()) {
		layinfo[eltpath.back()].active = -1;
		layinfo[eltpath.back()].visited = true;
		layinfo[eltpath.back()].scc = cur_scc;
		connpos.pop_back();
		eltpath.pop_back();
		connpath.pop_back();
		continue;
	    }

	    // live connection
	    int to_eindex = router->connection(conns[connpos.back()]).to_eindex();

	    // break cycle
	    if (layinfo[to_eindex].active >= 0) {
		// example eltpath:    0  1  2
		// example connpath: -1 c# c#: split last
		std::vector<int>::iterator cycle_begin = eltpath.begin() + layinfo[to_eindex].active;
		std::vector<int>::iterator eltpath_max = std::max_element(cycle_begin, eltpath.end());
		int killconn;
		if (eltpath_max + 1 != eltpath.end())
		    killconn = connpath[eltpath_max - eltpath.begin() + 1];
		else
		    killconn = conns[connpos.back()];
		connlive[killconn] = false;
		goto more_connections;
	    }

	    // connect components
	    if (layinfo[to_eindex].visited) {
		cur_scc = MIN(cur_scc, layinfo[to_eindex].scc);
		goto more_connections;
	    } else {
		layinfo[to_eindex].active = eltpath.size();
		connpath.push_back(conns[connpos.back()]);
		eltpath.push_back(to_eindex);
		connpos.push_back(-1);
		continue;
	    }
	}
    }

    // assign SCCs
    for (int i = 0; i < router->nelements(); ++i) {
	assert(layinfo[i].scc <= i);
	layinfo[i].scc = layinfo[layinfo[i].scc].scc;
	layinfo[layinfo[i].scc].scc_contents.push_back(i);
    }

    // lay out SCCs
    for (int i = 0; i < router->nelements(); ++i)
	if (layinfo[i].scc == i)
	    layout_one_scc(router, layinfo, connlive, i);
}


/*****
 *
 * Layout
 *
 */

int delt::orientation() const
{
    return _des->orientation;
}

bool delt::vertical() const
{
    return side_vertical(_des->orientation);
}

double delt::shadow(double scale, int side) const
{
    if (_des->shadow_style == dshadow_none
	|| (_des->shadow_style == dshadow_drop && (side == 0 || side == 3)))
	return 0;
    else if (_des->shadow_style == dshadow_unscaled_outline)
	return _des->shadow_width / scale;
    else
	return _des->shadow_width;
}

double delt::min_width() const
{
    return 18;			// XXX
}

double delt::min_height() const
{
    return 18;			// XXX
}

const char *cp_double(const char *begin, const char *end, double *result)
{
    const char *s = begin;
    if (s != end && (*s == '+' || *s == '-'))
	++s;
    if (s == end
	|| (*s == '.' && (s + 1 == end || !isdigit((unsigned char) s[1])))
	|| (*s != '.' && !isdigit((unsigned char) *s)))
	return begin;
    while (s != end && isdigit((unsigned char) *s))
	++s;
    if (s != end && *s == '.')
	++s;
    while (s != end && isdigit((unsigned char) *s))
	++s;
    if (s != end && (*s == 'e' || *s == 'E')) {
	const char *t = s + 1;
	if (t != end && (*t == '+' || *t == '-'))
	    ++t;
	while (t != end && isdigit((unsigned char) *t))
	    ++t;
	if (t != s + 1
	    && (t != s + 2 || isdigit((unsigned char) s[1])))
	    s = t;
    }
    *result = strtod(begin, 0);
    return s;
}

const char *delt::parse_connection_dot(delt *e1, const HashTable<int, delt *> &z_index_lookup, const char *s, const char *end)
{
    int eport, oz_index, oeport;
    delt *e2 = 0;
    Vector<point> route;

    if (s + 2 >= end || s[0] != ':' || s[1] != 'o' || !isdigit((unsigned char) s[2]))
	return s;
    s = cp_integer(s + 2, end, 10, &eport);
    if (s + 2 < end && s[0] == ':' && (s[1] == 's' || s[1] == 'e'))
	s += 2;
    s = cp_skip_space(s, end);
    if (s + 1 >= end || s[0] != '-' || s[1] != '>')
	return s;
    s = cp_skip_space(s + 2, end);
    if (s + 1 >= end || s[0] != 'n' || !isdigit((unsigned char) s[1]))
	return s;
    s = cp_integer(s + 1, end, 10, &oz_index);
    if (oz_index < 0 || !(e2 = z_index_lookup.get(oz_index)))
	return s;
    if (s + 2 >= end || s[0] != ':' || s[1] != 'i' || !isdigit((unsigned char) s[2]))
	return s;
    s = cp_integer(s + 2, end, 10, &oeport);
    if (s + 2 < end && s[0] == ':' && (s[1] == 'n' || s[1] == 'w'))
	s += 2;
    s = cp_skip_space(s, end);
    if (s >= end || s[0] != '[')
	return s;
    ++s;

  skip_to_p:
    while (s != end && *s != 'p' && *s != ';') {
	for (int quote = 0; s != end && (quote || (*s != ',' && *s != ']' && *s != ';')); ++s)
	    if (*s == '\"')
		quote = !quote;
	if (s != end)
	    s = cp_skip_space(s + 1, end);
    }
    if (s == end || *s == ';')
	return s;
    if (s + 2 >= end || s[1] != 'o' || s[2] != 's') {
	++s;
	goto skip_to_p;
    }
    s = cp_skip_space(s + 3, end);
    if (s == end || *s != '=')
	return s;
    s = cp_skip_space(s + 1, end);
    if (s != end && *s == '"')
	++s;
    s = cp_skip_space(s, end);

    const char *t;
    point origin(0, 0);
    int count = 0;
    while (s != end && *s != '"') {
	double x, y;
	int e = 0;
	if (s + 1 < end && *s == 'e' && s[1] == ',')
	    e = 1, s += 2;
	if ((t = cp_double(s, end, &x)) == s || t == end || *t != ',')
	    return t;
	s = t + 1;
	if ((t = cp_double(s, end, &y)) == s)
	    return t;
	s = cp_skip_space(t, end);
	point p(x * 100. / 72, -y * 100. / 72);
	if (e)
	    /* nada */;
	else if (++count <= 1)
	    origin = p;
	else
	    route.push_back(p - origin);
    }

    if (route.size() > 0 && (route.size() % 3) == 0) {
	for (std::vector<dconn *>::iterator ci = _conn.begin();
	     ci != _conn.end(); ++ci)
	    if ((*ci)->_elt[1] == e1 && (*ci)->_elt[0] == e2
		&& (*ci)->_port[1] == eport && (*ci)->_port[0] == oeport) {
		(*ci)->_route.swap(route);
		route.clear();
		break;
	    }
	if (route.size() != 0)
	    fprintf(stderr, "couldn't find connection %s[%d] -> [%d]%s\n", e1->name().c_str(), eport, oeport, e2->name().c_str());
    }

    return s;
}

char flow_split_char(const String &str, int port, bool isoutput)
{
    const char *s = str.begin(), *end = str.end(), *slash = find(s, end, '/');
    assert(s < slash && slash < end);
    if (isoutput)
	s = slash + 1;
    else
	end = slash;
    return s[std::min(port, end - s - 1)];
}

static void ports_dot(StringAccum &sa, int nports, char c)
{
    for (int p = 0; p < nports; ++p)
	 sa << (p ? "|<" : "<") << c << p << ">";
}

void delt::unparse_contents_dot(StringAccum &sa, crouter *cr, HashTable<int, delt *> &z_index_lookup) const
{
    delt fake_child(const_cast<delt *>(this), 0);
    ref_ptr<delt_size_style> gdess = cr->ccss()->elt_size_style(cr, &fake_child);
    double txsep = gdess->margin[1] + gdess->margin[3];
    double tysep = gdess->margin[0] + gdess->margin[2];

    for (std::vector<delt *>::size_type i = 0; i < _elt.size(); ++i) {
	delt *e = _elt[i];
	if (e->display() == dedisp_expanded) {
	    e->unparse_contents_dot(sa, cr, z_index_lookup);
	    continue;
	} else if (!e->visible())
	    continue;

	sa << "n" << e->z_index();
	assert(z_index_lookup.get(e->z_index()) == 0);
	z_index_lookup[e->z_index()] = e;

	double w = e->width() + (e->_dess->margin[1] + e->_dess->margin[3] - txsep);
	double h = e->height() + (e->_dess->margin[0] + e->_dess->margin[2] - tysep);
	sa << " [width=" << (w/100) << ",height=" << (h/100)
	   << ",fixedsize=true,label=\"{{";
	ports_dot(sa, e->ninputs(), 'i');
	sa << "}|" << e->display_name() << "|{";
	ports_dot(sa, e->noutputs(), 'o');
	sa << "}}\"];\n";

	e->_x = e->_y = 0;
    }
}

void delt::create_bbox_contents(double bbox[4], double mbbox[4], bool include_compound_ports) const
{
    for (std::vector<delt *>::const_iterator n = _elt.begin();
	 n != _elt.end(); ++n)
	if ((*n)->display() == dedisp_expanded)
	    (*n)->create_bbox_contents(bbox, mbbox, false);
	else if ((*n)->visible()
		 || (!root() && include_compound_ports && n < _elt.begin() + 2)) {
	    const double *m = (*n)->_dess->margin;
	    bbox[0] = std::min(bbox[0], (*n)->_y);
	    bbox[1] = std::max(bbox[1], (*n)->_x + (*n)->_width);
	    bbox[2] = std::max(bbox[2], (*n)->_y + (*n)->_height);
	    bbox[3] = std::min(bbox[3], (*n)->_x);
	    mbbox[0] = std::min(mbbox[0], (*n)->_y - m[0]);
	    mbbox[1] = std::max(mbbox[1], (*n)->_x + (*n)->_width + m[1]);
	    mbbox[2] = std::max(mbbox[2], (*n)->_y + (*n)->_height + m[2]);
	    mbbox[3] = std::min(mbbox[3], (*n)->_x - m[3]);
	}
}

void delt::shift_contents(double dx, double dy) const
{
    for (std::vector<delt *>::const_iterator n = _elt.begin();
	 n != _elt.end(); ++n)
	if ((*n)->display() == dedisp_expanded)
	    (*n)->shift_contents(dx, dy);
	else if ((*n)->visible()) {
	    (*n)->_x += dx;
	    (*n)->_y += dy;
	}
}

void delt::position_contents_dot(crouter *cr, ErrorHandler *errh)
{
    delt fake_child(this, 0);
    ref_ptr<delt_size_style> gdess = cr->ccss()->elt_size_style(cr, &fake_child);
    double gxsep = std::max(gdess->margin[1], gdess->margin[3]);
    double gysep = std::max(gdess->margin[0], gdess->margin[2]);

    StringAccum sa;
    sa << "digraph {\n"
       << "nodesep=" << (gxsep / 100) << ";\n"
       << "ranksep=" << (gysep / 100) << ";\n"
       << "node [shape=record];\n"
       << "edge [arrowsize=0.2,headclip=true,tailclip=true];\n";
    if (_contents_width)
	sa << "size=\"" << (_contents_width / 100) << "," << (_contents_height / 100) << "\";\n"
	   << "ratio=compress;\n";

    HashTable<int, delt *> z_index_lookup;

    if (!root()) {
	delt *ein = _elt[0], *eout = _elt[1];
	assert(_elt.size() >= 2 && ein->name() == "input" && !ein->visible()
	       && eout->name() == "output" && !eout->visible());
	if (_e->ninputs()) {
	    z_index_lookup[ein->z_index()] = ein;
	    sa << "{ rank=source; n" << ein->z_index() << " [";
	    if (_contents_width) {
		if (vertical())
		    sa << "width=" << (_contents_width / 100) << ",height=0,fixedsize=true,";
		else
		    sa << "height=" << (_contents_height / 100) << ",width=0,fixedsize=true,";
	    }
	    sa << "label=\"{{}|{";
	    ports_dot(sa, _e->ninputs(), 'o');
	    sa << "}}\"]; }\n";
	}
	if (_e->noutputs()) {
	    z_index_lookup[eout->z_index()] = eout;
	    sa << "{ rank=sink; n" << eout->z_index() << " [";
	    if (_contents_width) {
		if (vertical())
		    sa << "width=" << (_contents_width / 100) << ",height=0,fixedsize=true,";
		else
		    sa << "height=" << (_contents_height / 100) << ",width=0,fixedsize=true,";
	    }
	    sa << "label=\"{{";
	    ports_dot(sa, _e->noutputs(), 'i');
	    sa << "}|{}}\"]; }\n";
	}
    }

    unparse_contents_dot(sa, cr, z_index_lookup);

    for (std::vector<dconn *>::iterator ci = _conn.begin(); ci != _conn.end(); ++ci) {
	delt *eout = (*ci)->_elt[1], *ein = (*ci)->_elt[0];
	sa << 'n' << eout->z_index()
	   << ':' << 'o' << (*ci)->_port[1] << ':'
	   << (eout->vertical() ? 's' : 'e')
	   << " -> n" << ein->z_index()
	   << ':' << 'i' << (*ci)->_port[0] << ':'
	   << (ein->vertical() ? 'n' : 'w') << ";\n";
    }
    sa << "}\n";

    //fprintf(stderr, "%s\n", sa.c_str());
    String result;
    {
	StringAccum outsa(shell_command_output_string("dot", sa.take_string(), errh));
	char *outs = outsa.begin(), *outend = outsa.end();
	for (char *s = outsa.begin(); s != outend; )
	    if (*s == '\\' && s + 1 < outend && s[1] == '\n')
		s += 2;
	    else if (*s == '\\' && s + 2 < outend && s[1] == '\r' && s[1] == '\n')
		s += 3;
	    else if (*s == '\\' && s + 1 < outend && s[1] == '\r')
		s += 2;
	    else
		*outs++ = *s++;
	outsa.adjust_length(outs - outsa.end());
	result = outsa.take_string();
    }
    //fprintf(stderr, "%s\n", result.c_str());
    //shell_command_output_string("dot -Tps > /tmp/x.ps", result, errh);

    const char *end = result.end();
    for (const char *s = result.begin(); s != end; ) {
	s = cp_skip_space(s, end);
	if (s + 1 >= end || *s != 'n' || !isdigit((unsigned char) s[1])) {
	    if (s != end && (*s == '{' || *s == '}')) {
		++s;
		continue;
	    }
	  skip_to_semicolon:
	    while (s != end && *s != ';')
		++s;
	    if (s != end)
		++s;
	    continue;
	}

	int z_index = 0;
	delt *e1;
	s = cp_integer(s + 1, end, 10, &z_index);
	while (s != end && isspace((unsigned char) *s))
	    ++s;
	if (z_index < 0 || !(e1 = z_index_lookup.get(z_index)))
	    goto skip_to_semicolon;

	if (s == end)
	    goto skip_to_semicolon;
	else if (*s == ':') {
	    s = parse_connection_dot(e1, z_index_lookup, s, end);
	    goto skip_to_semicolon;
	} else if (*s != '[')
	    goto skip_to_semicolon;

      skip_to_p:
	while (s != end && *s != 'p' && *s != ';') {
	    for (int quote = 0; s != end && (quote || (*s != ',' && *s != ']' && *s != ';')); ++s)
		if (*s == '\"')
		    quote = !quote;
	    if (s != end)
		s = cp_skip_space(s + 1, end);
	}
	if (s == end || *s == ';')
	    goto skip_to_semicolon;
	if (s + 2 >= end || s[1] != 'o' || s[2] != 's') {
	    ++s;
	    goto skip_to_p;
	}
	s = cp_skip_space(s + 3, end);
	if (s == end || *s != '=')
	    goto skip_to_semicolon;
	s = cp_skip_space(s + 1, end);
	if (s != end && *s == '"')
	    ++s;
	s = cp_skip_space(s, end);
	double x, y;
	const char *t;
	if ((t = cp_double(s, end, &x)) == s)
	    goto skip_to_semicolon;
	s = cp_skip_space(t, end);
	if (s == end || *s != ',')
	    goto skip_to_semicolon;
	s = cp_skip_space(s + 1, end);
	if ((t = cp_double(s, end, &y)) == s)
	    goto skip_to_semicolon;
	e1->_x = x * 100. / 72 - e1->_width / 2;
	if (e1->_dess->margin[1] != e1->_dess->margin[3])
	    e1->_x -= e1->_dess->margin[1] - e1->_dess->margin[3];
	e1->_y = -y * 100. / 72 - e1->_height / 2;
	if (e1->_dess->margin[0] != e1->_dess->margin[2])
	    e1->_y -= e1->_dess->margin[2] - e1->_dess->margin[0];
	goto skip_to_semicolon;
    }

    bool first_time = _contents_width == 0;

    double bbox[4], mbbox[4];
    bbox[0] = mbbox[0] = bbox[3] = mbbox[3] = 1000000;
    bbox[1] = mbbox[1] = bbox[2] = mbbox[2] = -1000000;
    create_bbox_contents(bbox, mbbox, first_time);

    if (!root() && !first_time)
	for (int i = 0; i < 4; ++i) {
	    double delta = fabs(bbox[i] - mbbox[i]) - _dess->padding[i];
	    if (delta > 0)
		bbox[i] += (rectangle::side_greater(i) ? delta : -delta);
	}

    // when laying out the root, consider connections as well as elements
    // to calculate the bounding box
    if (root())
	for (std::vector<dconn *>::iterator it = _conn.begin();
	     it != _conn.end(); ++it)
	    if ((*it)->layout()) {
		bbox[0] = std::min(bbox[0], (*it)->_y);
		bbox[1] = std::max(bbox[1], (*it)->_x + (*it)->_width);
		bbox[2] = std::max(bbox[2], (*it)->_y + (*it)->_height);
		bbox[3] = std::min(bbox[3], (*it)->_x);
		mbbox[0] = std::min(mbbox[0], (*it)->_y);
		mbbox[1] = std::max(mbbox[1], (*it)->_x + (*it)->_width);
		mbbox[2] = std::max(mbbox[2], (*it)->_y + (*it)->_height);
		mbbox[3] = std::min(mbbox[3], (*it)->_x);
	    }

    _contents_width = std::max(bbox[1] - bbox[3], 0.);
    _contents_height = std::max(bbox[2] - bbox[0], 0.);
    shift_contents(-bbox[3], -bbox[0]);

    if (!root() && first_time)
	position_contents_dot(cr, errh);
}


/*****
 *
 *
 *
 */

bool delt::reccss(crouter *cr, int change, int *z_index_ptr)
{
    int x;
    bool redraw = false;
    ref_ptr<delt_size_style> old_dess = _dess;
    ref_ptr<delt_style> old_des = _des;
    String old_markup = _markup;

    if (change & (_dess_sensitivity | dsense_always)) {
	_dess = cr->ccss()->elt_size_style(cr, this, &x);
	_dess_sensitivity = x;
	if (old_dess != _dess)
	    redraw = true;
    }

    if (change & (_des_sensitivity | dsense_always)) {
	_des = cr->ccss()->elt_style(cr, this, &x);
	_des_sensitivity = x;
	if (old_des != _des)
	    redraw = true;
    }

    if ((change & (_markup_sensitivity | dsense_always))
	|| (_des != old_des && _des->text != old_des->text)) {
	_markup = parse_markup(_des->text, cr, -1, &x);
	_markup_sensitivity = x;
    }

    if ((change & dsense_always) || _des->font != old_des->font
	|| _markup != old_markup) {
	_markup_width = _markup_height = -1024;
	redraw = true;
    }

    if ((change & dsense_always) || _des != old_des) {
	_display = _des->display;
	if (_split_type == desplit_inputs && _display != dedisp_vsplit)
	    _display = dedisp_none;
	if (_display == dedisp_expanded && primitive())
	    _display = dedisp_open;
	if (dedisp_visible(_display)
	    && (_parent->root()		// NB _parent->_display is not yet set
		|| dedisp_children_visible(_parent->_des->display)))
	    _visible = true;
	else
	    _visible = false;
	// "input" and "output" are always invisible, but displayed
	if (_e->tunnel() && !_parent->root()
	    && (this == _parent->_elt[0] || this == _parent->_elt[1])) {
	    _visible = false;
	    _display = dedisp_open;
	}
    }

    if ((!old_des || _des->decorations != old_des->decorations
	 || (!_decor && _des->decorations))
	&& _visible) {
	ddecor::free_list(_decor);
	String s = _des->decorations;
	while (String dname = cp_pop_spacevec(s))
	    if (String dtype = cr->ccss()->vstring("style", dname, cr, this)) {
		if (dtype == "fullness")
		    _decor = new dfullness_decor(dname, cr, this, _decor);
		else if (dtype == "activity")
		    _decor = new dactivity_decor(dname, cr, this, _decor);
	    }
    }

    if (_display == dedisp_vsplit && !_split) {
	delt *se = create_split(desplit_inputs, z_index_ptr);
	se->reccss(cr, dsense_always);
	redraw = true;
    } else if (_display == dedisp_fsplit && !_split) {
	Bitvector map(256);
	assert(!_split_type);
	int portno = 0, max_ports = ninputs();
	for (const char *s = _des->flow_split.begin(); s != _des->flow_split.end(); ++s)
	    if (*s == '/')
		portno = 0, max_ports = noutputs();
	    else if (portno++ >= max_ports)
		/* do nothing */;
	    else if (!map[(unsigned char) *s]) {
		map[(unsigned char) *s] = true;
		if (!_split_type)
		    _split_type = (unsigned char) *s;
		else {
		    delt *se = create_split((unsigned char) *s, z_index_ptr);
		    se->reccss(cr, dsense_always);
		    redraw = true;
		}
	    }
	if (!_split) {		// flowsplit didn't actually split anything
	    _split_type = 0;
	    _display = dedisp_closed;
	}
    }

    return redraw;
}



/*****
 *
 *
 *
 */

void delt::layout_contents(dcontext &dcx)
{
    for (size_t i = 0; i != _elt.size(); ++i)
	_elt[i]->layout(dcx);

    if (root() || _display == dedisp_open)
	position_contents_dot(dcx.cr, dcx.cr->error_handler());
    //position_contents_scc(router);
    //position_contents_first_heuristic(router);
}

void delt::layout_ports(dcontext &dcx)
{
    // XXX layout_ports
    delete[] _portoff[0];
    delete[] _portoff[1];
    delete[] _port_text_offsets;
    _portoff[0] = _portoff[1] = 0;
    _port_text_offsets = 0;
    dcss_set *dcs = dcx.cr->ccss();
    int poff = 0;

    for (int isoutput = 0; isoutput < 2; ++isoutput) {
	ref_ptr<dport_style> dps = dcs->port_style(dcx.cr, this, isoutput, 0, 0);
	_ports_length[isoutput] = 2 * dps->edge_padding;
	if (!_e->nports(isoutput)
	    || (_display == dedisp_vsplit && isoutput == (_split_type != 0)))
	    continue;
	if (_e->nports(isoutput) > 1)
	    _portoff[isoutput] = new double[_e->nports(isoutput) + 1];
	double text_offset = 0;
	double tm = dps->edge_padding;
	for (int p = 0; p < _e->nports(isoutput); ++p, ++poff) {
	    if (p)
		dps = dcs->port_style(dcx.cr, this, isoutput, p, 0);
	    double l;
	    if (dps->shape == dpshape_triangle)
		l = dps->length - 2;
	    else
		l = dps->length + 4;
	    if (dps->text) {
		String markup = parse_markup(dps->text, dcx.cr, p, 0);
		if (dcx.pl_font != dps->font)
		    dcx.set_font_description(dps->font);
		pango_layout_set_width(dcx, -1);
		pango_layout_set_markup(dcx, markup.data(), markup.length());
		PangoRectangle rect;
		pango_layout_get_pixel_extents(dcx, NULL, &rect);
		l = std::max(l, (double) rect.width + 2);
		text_offset = std::max(text_offset, (double) rect.height + 1);
	    }
	    _ports_length[isoutput] += l;
	    double old_tm = tm;
	    tm += dps->margin[_des->orientation] + l / 2;
	    if (_e->nports(isoutput) > 1)
		_portoff[isoutput][p] = tm;
	    tm += dps->margin[_des->orientation ^ 2] + l / 2;
	    if (old_tm + 0.1 > tm)
		tm = old_tm + 0.1;
	}
	if (_e->nports(isoutput) > 1)
	    _portoff[isoutput][_e->nports(isoutput)] = tm + dps->edge_padding;
	if (text_offset && !_port_text_offsets) {
	    _port_text_offsets = new double[2];
	    _port_text_offsets[0] = _port_text_offsets[1] = 0;
	}
	if (text_offset)
	    _port_text_offsets[isoutput] = text_offset;
    }
}

static void append_markup_quote(StringAccum &sa, const String &str,
				int precision)
{
    const char *s = str.begin();
    while (s != str.end() && isspace((unsigned char) *s))
	++s;
    const char *last = s;
    for (; s != str.end() && precision != 0; ++s) {
	if (precision > 0)
	    // XXX UTF-8
	    --precision;
	if (*s == '<' || *s == '>') {
	    sa.append(last, s);
	    sa << (*s == '<' ? "&lt;" : "&gt;");
	    last = s + 1;
	}
    }
    sa.append(last, s);
    if (s != str.end())
	sa.append("...", 3);
}

String delt::parse_markup(const String &text, crouter *cr,
			  int port, int *sensitivity)
{
    if (sensitivity)
	*sensitivity = 0;

    StringAccum sa;
    const char *last = text.begin(), *send = text.end();
    for (const char *s = text.begin(); s != send; ++s)
	if (*s == '%' && s + 1 != send) {
	    sa.append(last, s);

	    enum { pm_width = 0, pm_precision = 1, pm_specifier = 2 };
	    int vals[3], altflag = 0, which = 0;
	    vals[0] = vals[1] = vals[2] = -1;
	    const char *pct = s;
	    for (++s; s != send; ++s)
		if (isdigit((unsigned char) *s))
		    vals[which] = (vals[which] >= 0 ? 10 * vals[which] : 0)
			+ *s - '0';
		else if (*s == '*') {
		    if (which == pm_specifier)
			vals[which] = port;
		} else if (*s == '.')
		    which = pm_precision;
		else if (*s == ':')
		    which = pm_specifier;
		else if (*s == '#')
		    altflag = 1;
		else
		    break;

	    if (s == send) {
	      invalid_format:
		last = s = pct;
		continue;
	    }

	    if (*s == 'n')
		append_markup_quote(sa, display_name(), vals[pm_precision]);
	    else if (*s == 'c')
		append_markup_quote(sa, type_name(), vals[pm_precision]);
	    else if (*s == 'f')
		append_markup_quote(sa, flat_name(), vals[pm_precision]);
	    else if (*s == 'C') {
		String c = _e->configuration();
		if (vals[pm_specifier] >= 0) {
		    Vector<String> conf;
		    cp_argvec(c, conf);
		    if (conf.size() > vals[pm_specifier])
			c = conf[vals[pm_specifier]];
		    else
			c = String();
		}
		if (!altflag)
		    append_markup_quote(sa, c, vals[pm_precision]);
		else if (c) {
		    sa << '(';
		    append_markup_quote(sa, c, vals[pm_precision]);
		    sa << ')';
		}
	    } else if (*s == '{') {
		const char *n = s + 1;
		for (++s; s != send && *s != '}'; ++s)
		    /* nada */;
		if (s == send || n == s)
		    goto invalid_format;
		handler_value *hv = cr->hvalues().find_placeholder(flat_name() + "." + text.substring(n, s), hflag_notify_delt);
		if (hv) {
		    if (hv->have_hvalue())
			append_markup_quote(sa, hv->hvalue(), vals[pm_precision]);
		    else {
			if (altflag && hv->refreshable())
			    hv->set_flags(cr, hv->flags() | hflag_autorefresh);
			if (hv->refreshable())
			    hv->refresh(cr);
			if (vals[pm_width] > 0)
			    sa.append_fill('?', vals[pm_width]);
		    }
		    if (sensitivity)
			*sensitivity |= dsense_handler;
		}
	    } else
		goto invalid_format;

	    last = s + 1;
	}
    sa.append(last, send);
    return sa.take_string();
}

void delt::dimension_markup(dcontext &dcx)
{
    pango_layout_set_width(dcx, -1);
    if (_des->font != dcx.pl_font)
	dcx.set_font_description(_des->font);
    pango_layout_set_markup(dcx, _markup.data(), _markup.length());
    PangoRectangle rect;
    pango_layout_get_pixel_extents(dcx, NULL, &rect);
    _markup_width = rect.width * _dess->scale;
    _markup_height = rect.height * _dess->scale;
}

void delt::layout(dcontext &dcx)
{
    assert(_des && _dess && _width == 0 && _height == 0);

    // get contents width and height
    if (_elt.size() && dedisp_children_visible(_display))
	layout_contents(dcx);

    // exit if not visible
    if (!dedisp_visible(_display))
	return;

    // get text extents
    dimension_markup(dcx);
    // get port position
    layout_ports(dcx);

    // get element width and height
    double xwidth, xheight;

    if (_des->style == destyle_queue
	&& _contents_height == 0 && side_vertical(_des->orientation)) {
	xwidth = _markup_height;
	xheight = _markup_width + _dess->padding[0] + _dess->padding[2];
    } else {
	xwidth = MAX(_markup_width, _contents_width);
	xheight = _markup_height + _dess->padding[0] + _dess->padding[2];
	if (_contents_height)
	    xheight += _contents_height;
    }
    if (_port_text_offsets && side_vertical(_des->orientation))
	xheight += _port_text_offsets[0] + _port_text_offsets[1];
    else if (_port_text_offsets)
	xwidth += _port_text_offsets[0] + _port_text_offsets[1];
    xwidth = MAX(xwidth, _dess->min_width);
    xheight = MAX(xheight, _dess->min_height);

    // analyze port positions
    double xportlen = MAX(_ports_length[0], _ports_length[1]) * _dess->scale;

    if (_des->orientation == 0)
	_width = ceil(MAX(xwidth + _dess->padding[1] + _dess->padding[3],
			  xportlen));
    else {
	_width = ceil(xwidth + _dess->padding[1] + _dess->padding[3]);
	xheight = MAX(xheight, xportlen);
    }

    if (xheight > _dess->min_height && _dess->height_step > 0)
	xheight = _dess->min_height + ceil((xheight - _dess->min_height) / _dess->height_step) * _dess->height_step;
    _height = xheight;

    // adjust by border width and fix to integer boundaries
    _width = ceil(_width + 2 * _dess->border_width);
    _height = ceil(_height + 2 * _dess->border_width);
}


void delt::layout_compound_ports_copy(delt *compound, bool isoutput)
{
    // XXX layout_ports
    delete[] _portoff[!isoutput];
    _portoff[!isoutput] = 0;
    _ports_length[!isoutput] = compound->_ports_length[isoutput];
    if (compound->_portoff[isoutput]) {
	assert(_e->nports(!isoutput) == compound->_e->nports(isoutput));
	int n = _e->nports(!isoutput);
	_portoff[!isoutput] = new double[n + 1];
	memcpy(_portoff[!isoutput], compound->_portoff[isoutput], (n + 1) * sizeof(double));
    }
}

void delt::layout_compound_ports(crouter *cr)
{
    assert(_elt.size() >= 2 && _elt[0]->name() == "input" && _elt[1]->name() == "output" && !_elt[0]->visible() && !_elt[1]->visible());

    _elt[0]->_x = _x;
    _elt[0]->_y = _y - 10;
    _elt[0]->_width = _width;
    ref_ptr<dport_style> dps = cr->ccss()->port_style(cr, this, false, 0, 0);
    _elt[0]->_height = 10 + dps->width;
    _elt[0]->layout_compound_ports_copy(this, false);

    _elt[1]->_x = _x;
    dps = cr->ccss()->port_style(cr, this, true, 0, 0);
    _elt[1]->_y = _y + _height - dps->width;
    _elt[1]->_width = _width;
    _elt[1]->_height = 10;
    _elt[1]->layout_compound_ports_copy(this, true);
}

void delt::layout_complete(dcontext &dcx, double dx, double dy)
{
    if (dedisp_visible(_display)) {
	dx += _dess->padding[3];
	dy += _markup_height + _dess->padding[0];
    }

    for (std::vector<delt *>::iterator ci = _elt.begin();
	 ci != _elt.end(); ++ci) {
	double unadjusted_x = dx, unadjusted_y = dy;
	if ((*ci)->visible()) {
	    unadjusted_x += (*ci)->_x;
	    unadjusted_y += (*ci)->_y;
	    (*ci)->_x = floor(unadjusted_x);
	    (*ci)->_y = floor(unadjusted_y);
	}
	if ((*ci)->_elt.size() && dedisp_children_visible((*ci)->_display))
	    (*ci)->layout_complete(dcx, unadjusted_x, unadjusted_y);
    }

    if (dedisp_visible(_display) && _elt.size())
	layout_compound_ports(dcx.cr);

    if (root() || dedisp_children_visible(_display))
	for (std::vector<dconn *>::iterator it = _conn.begin();
	     it != _conn.end(); ++it)
	    (*it)->layout();
}

void delt::layout_main(dcontext &dcx)
{
    delt fake_child(this, 0);
    _des = dcx.cr->ccss()->elt_style(dcx.cr, &fake_child);
    _dess = dcx.cr->ccss()->elt_size_style(dcx.cr, &fake_child);
    _display = dedisp_expanded;
    layout_contents(dcx);
    layout_complete(dcx, _dess->margin[3], _dess->margin[0]);
    assign(0, 0, _contents_width + _dess->margin[1] + _dess->margin[3],
	   _contents_height + _dess->margin[0] + _dess->margin[2]);
}

void delt::insert_all(rect_search<dwidget> &rects)
{
    if (visible())
	rects.insert(this);
    if (dedisp_children_visible(_display)) {
	for (std::vector<delt *>::iterator ci = _elt.begin();
	     ci != _elt.end(); ++ci)
	    (*ci)->insert_all(rects);
	for (std::vector<dconn *>::iterator ci = _conn.begin();
	     ci != _conn.end(); ++ci)
	    if ((*ci)->visible())
		rects.insert(*ci);
    }
}


/*****
 *
 *
 *
 */

bool dconn::layout()
{
    if (!visible())
	return false;
    point op = _elt[1]->output_position(_port[1], 0);
    point ip = _elt[0]->input_position(_port[0], 0);
    if (_elt[1]->orientation() == 0 && _elt[0]->orientation() == 0) {
	_x = MIN(op.x(), ip.x() - 3);
	_y = MIN(op.y(), ip.y() - 12);
	_width = MAX(op.x(), ip.x() + 3) - _x;
	_height = MAX(op.y() + 5, ip.y()) - _y;
    } else {
	_x = MIN(op.x(), ip.x() - 12);
	_y = MIN(op.y() - 2, ip.y() - 12);
	_width = MAX(op.x() + 5, ip.x() + 3) - _x;
	_height = MAX(op.y() + 5, ip.y() + 3) - _y;
    }
    if (_route.size()) {
	affine t = affine::mapping(point(0, 0), op, _route.back(), ip);
	for (const point *pp = _route.begin(); pp != _route.end(); ++pp)
	    *this |= t * *pp;
    }
    return true;
}

/*****
 *
 * recalculating bounds
 *
 */

void delt::layout_recompute_bounds()
{
    if (_elt.size()) {
	assign(0, 0, 0, 0);
	union_bounds(*this, false);
	_contents_width = _width;
	_contents_height = _height;
	expand(_dess->margin[0], _dess->margin[1],
	       _dess->margin[2], _dess->margin[3]);
    }
}

void delt::union_bounds(rectangle &r, bool self) const
{
    if (self && visible())
	r |= *this;
    if (dedisp_children_visible(_display)) {
	for (std::vector<delt *>::const_iterator it = _elt.begin();
	     it != _elt.end(); ++it)
	    (*it)->union_bounds(r, true);
	for (std::vector<dconn *>::const_iterator ci = _conn.begin();
	     ci != _conn.end(); ++ci)
	    if ((*ci)->visible())
		r |= **ci;
    }
}

void delt::remove(rect_search<dwidget> &rects, rectangle &bounds)
{
    if (!visible())
	return;

    bounds |= *this;
    rects.remove(this);

    delt *p = _parent;
    while (!p->root() && p->display() == dedisp_expanded)
	p = p->parent();
    for (std::vector<dconn *>::iterator it = p->_conn.begin();
	 it != p->_conn.end(); ++it)
	if ((*it)->_elt[1]->_e == _e || (*it)->_elt[0]->_e == _e) {
	    bounds |= **it;
	    rects.remove(*it);
	}

    if (_parent && _elt.size() && _display == dedisp_open) {
	_elt[0]->remove(rects, bounds);
	_elt[1]->remove(rects, bounds);
    }
}

void delt::insert(rect_search<dwidget> &rects, crouter *cr,
		  rectangle &bounds)
{
    if (!visible())
	return;

    bounds |= *this;
    rects.insert(this);

    delt *p = _parent;
    while (!p->root() && p->display() == dedisp_expanded)
	p = p->parent();
    for (std::vector<dconn *>::iterator it = p->_conn.begin();
	 it != p->_conn.end(); ++it)
	if ((*it)->_elt[1]->_e == _e || (*it)->_elt[0]->_e == _e) {
	    (*it)->layout();
	    bounds |= **it;
	    rects.insert(*it);
	}

    if (_parent && _elt.size() && _display == dedisp_open) {
	layout_compound_ports(cr);
	_elt[0]->insert(rects, cr, bounds);
	_elt[1]->insert(rects, cr, bounds);
    }
}

dconn *delt::find_connection(bool isoutput, int port)
{
    delt *e = find_port_container(isoutput, port);
    delt *p = _parent;
    while (!p->root() && p->display() == dedisp_expanded)
	p = p->_parent;
    for (std::vector<dconn *>::iterator it = p->_conn.begin();
	 it != p->_conn.end(); ++it)
	if ((*it)->elt(isoutput) == e
	    && (*it)->port(isoutput) == port)
	    return *it;
    return 0;
}

/*****
 *
 * drawing
 *
 */

double delt::hard_port_position(bool isoutput, int port,
				double side_length) const
{
    assert(_e->nports(isoutput) > 1);

    return side_length * _portoff[isoutput][port]
		 / _portoff[isoutput][_e->nports(isoutput)];
}

delt *delt::find_flow_split(bool isoutput, int port)
{
    int c = flow_split_char(_des->flow_split, port, isoutput);
    return find_split(c);
}

point delt::input_position(int port, dport_style *dps, bool here) const
{
    if (_display == dedisp_vsplit && _split && _split->visible()
	&& _split_type != desplit_inputs && !here)
	return _split->input_position(port, 0, true);
    if (_display == dedisp_fsplit && _split && !here) {
	int c = flow_split_char(_des->flow_split, port, false);
	if (_split_type != c)
	    return find_split(c)->input_position(port, dps, true);
    }

    double off = port_position(false, port, side_length(_des->orientation ^ 1));

    double pos = side(_des->orientation);
    if (dps) {
	double dd = (_des->style == destyle_queue ? 0 : _dess->border_width);
	if (dps->shape == dpshape_triangle)
	    dd = std::max(dd - dps->border_width / 2, dps->border_width / 2);
	pos += (side_greater(_des->orientation ^ 2) ? dd : -dd);
    }

    if (side_horizontal(_des->orientation))
	return point(pos, _y + off);
    else
	return point(_x + off, pos);
}

point delt::output_position(int port, dport_style *dps, bool here) const
{
    if (_display == dedisp_vsplit && _split && _split->visible()
	&& _split_type == desplit_inputs && !here)
	return _split->output_position(port, 0, true);
    if (_display == dedisp_fsplit && _split && !here) {
	int c = flow_split_char(_des->flow_split, port, true);
	if (_split_type != c)
	    return find_split(c)->output_position(port, dps, true);
    }

    double off = port_position(true, port, side_length(_des->orientation ^ 1));

    double pos = side(_des->orientation ^ 2);
    if (dps) {
	double dd = _dess->border_width;
	if (dps->shape == dpshape_triangle)
	    dd = std::max(dd - dps->border_width / 2, dps->border_width / 2);
	pos += (side_greater(_des->orientation ^ 2) ? -dd : dd);
    }

    if (side_horizontal(_des->orientation))
	return point(pos, _y + off);
    else
	return point(_x + off, pos);
}

static inline void cairo_set_border(cairo_t *cr, const double *color,
				    int style, double width)
{
    if (color)
	cairo_set_source_rgba(cr, color[0], color[1], color[2], color[3]);
    double dash[2];
    if (style == dborder_dashed) {
	dash[0] = 4 * width;
	dash[1] = 3 * width;
	cairo_set_dash(cr, dash, 2, 0);
    } else if (style == dborder_dotted) {
	if (width <= 2) {
	    dash[0] = 0.5 * width;
	    dash[1] = 1.5 * width;
	} else
	    dash[0] = dash[1] = width;
	cairo_set_dash(cr, dash, 2, 0);
    } else
	cairo_set_dash(cr, 0, 0, 0);
    cairo_set_line_width(cr, width);
}

void delt::draw_port(dcontext &dcx, dport_style *dps, point p,
		     int port, bool isoutput, double opacity)
{
    // align position
    if (dcx.scale_step == 0 && _aligned) {
	double dd = (dps->shape == dpshape_triangle ? 0.5 : 0);
	if (side_vertical(_des->orientation))
	    p._x = round(p._x - dd) + dd;
	else
	    p._y = round(p._y - dd) + dd;
    }

    cairo_matrix_t original_matrix;
    cairo_get_matrix(dcx, &original_matrix);
    cairo_translate(dcx, p.x(), p.y());
    int port_orientation = _des->orientation ^ (isoutput ? 2 : 0);
    if (port_orientation & 1)
	cairo_rotate(dcx, port_orientation * M_PI_2);

    double l = dps->length * _dess->scale;
    double w = dps->width * _dess->scale;
    if (port_orientation & 2)
	l = -l, w = -w;

    for (int i = 0; i < 4; i++) {
	if ((i > 0 && dps->border_style == dborder_none)
	    || (i > 1 && dps->border_style != dborder_inset))
	    break;

	const double *color;
	if (i == 0 && dps->border_style == dborder_inset)
	    color = white_color;
	else if (i == 0 || i == 2)
	    color = dps->color;
	else
	    color = dps->border_color;
	if (color[3] == 0)
	    continue;

	cairo_set_source_rgba(dcx, color[0], color[1], color[2],
			      color[3] * opacity);

	double offset;
	if (i > 1)
	    offset = (dps->shape == dpshape_triangle ? 2 * M_SQRT2 : 2)
		* dps->border_width;
	else
	    offset = 0;
	if (offset > fabs(l / 2))
	    continue;
	if (port_orientation & 2)
	    offset = -offset;

	if (i == 1)
	    cairo_set_border(dcx, 0, dps->border_style, dps->border_width);
	else if (i == 3)
	    cairo_set_line_width(dcx, 0.75 * dps->border_width);

	if (dps->shape == dpshape_triangle) {
	    cairo_move_to(dcx, -l / 2 + offset, 0);
	    cairo_line_to(dcx, 0, w - 1.15 * offset);
	    cairo_line_to(dcx, l / 2 - offset, 0);
	    if (i < 3)
		cairo_close_path(dcx);
	} else {
	    cairo_move_to(dcx, -l / 2 + offset, 0);
	    cairo_line_to(dcx, -l / 2 + offset, w - offset);
	    cairo_line_to(dcx, l / 2 - offset, w - offset);
	    cairo_line_to(dcx, l / 2 - offset, 0);
	    if (!(i & 1))
		cairo_close_path(dcx);
	}

	if (i & 1)
	    cairo_stroke(dcx);
	else
	    cairo_fill(dcx);
    }

    if (dps->text) {
	String markup = parse_markup(dps->text, dcx.cr, port, 0);
	if (dcx.pl_font != dps->font)
	    dcx.set_font_description(dps->font);
	pango_layout_set_alignment(dcx, PANGO_ALIGN_CENTER);
	pango_layout_set_width(dcx, -1);
	pango_layout_set_markup(dcx, markup.data(), markup.length());
	PangoRectangle rect;
	pango_layout_get_pixel_extents(dcx, NULL, &rect);
	cairo_move_to(dcx, -rect.width / 2, w + (port_orientation & 2 ? -rect.height - 1 : rect.height + 1));
	pango_cairo_show_layout(dcx, dcx);
    }

    cairo_set_matrix(dcx, &original_matrix);
}

void delt::draw_ports(dcontext &dcx)
{
    const char *pcpos = _processing_code.begin();
    int pcode;
    ref_ptr<dport_style> dps;

    if (_display != dedisp_vsplit || _split_type == desplit_inputs)
	for (int i = 0; i < _e->ninputs(); i++) {
	    pcpos = ProcessingT::processing_code_next
		(pcpos, _processing_code.end(), pcode);
	    dps = dcx.cr->ccss()->port_style(dcx.cr, this, false, i, pcode);
	    if (dps->display & dpdisp_inputs) {
		double opacity = (_display == dedisp_fsplit && flow_split_char(_des->flow_split, i, false) != _split_type ? 0.25 : 1);
		draw_port(dcx, dps.get(), input_position(i, dps.get(), true),
			  i, false, opacity);
	    }
	}
    pcpos = ProcessingT::processing_code_output(_processing_code.begin(), _processing_code.end(), pcpos);
    if (_display != dedisp_vsplit || _split_type != desplit_inputs)
	for (int i = 0; i < _e->noutputs(); i++) {
	    pcpos = ProcessingT::processing_code_next
		(pcpos, _processing_code.end(), pcode);
	    dps = dcx.cr->ccss()->port_style(dcx.cr, this, true, i, pcode);
	    if (dps->display & dpdisp_outputs) {
		double opacity = (_display == dedisp_fsplit && flow_split_char(_des->flow_split, i, true) != _split_type ? 0.25 : 1);
		draw_port(dcx, dps.get(), output_position(i, dps.get(), true),
			  i, true, opacity);
	    }
	}
}

static void cairo_jagged_edge(cairo_t *cr, double x0, double y0,
			      double x1, double y1, int spo)
{
    int n = (int) ceil(fabs(spo & 1 ? y1 - y0 : x1 - x0) / 20);
    double delta = (spo & 1 ? y1 - y0 : x1 - x0) / (2 * n);
    double shift = (rectangle::side_greater(spo) ? -5 : 5);
    for (int i = 1; i < 2*n; i++) {
	double my_shift = (i & 1 ? shift : 0);
	double x = (spo & 1 ? x0 + my_shift : x0 + i * delta);
	double y = (spo & 1 ? y0 + i * delta : y0 + my_shift);
	cairo_line_to(cr, x, y);
    }
}

void delt::draw_background(dcontext &dcx)
{
    double pos[4];
    double bwd = floor(_dess->border_width / 2);

    // figure out positions
    pos[3] = _x + bwd;
    pos[0] = _y + bwd;
    pos[1] = _x + _width - bwd;
    pos[2] = _y + _height - bwd;
    if (_des->style == destyle_queue)
	pos[_des->orientation] = side(_des->orientation);

    // background
    if (_des->background_color[3]) {
	const double *color = _des->background_color;
	cairo_set_source_rgba(dcx, color[0], color[1], color[2], color[3]);
	if (_display != dedisp_vsplit) {
	    cairo_move_to(dcx, pos[3], pos[0]);
	    cairo_line_to(dcx, pos[1], pos[0]);
	    cairo_line_to(dcx, pos[1], pos[2]);
	    cairo_line_to(dcx, pos[3], pos[2]);
	} else {
	    int spo = _des->orientation;
	    if (_split_type == desplit_inputs)
		spo = (spo + 2) & 3;
	    cairo_move_to(dcx, pos[((spo + 3) & 2) + 1], pos[spo & 2]);
	    cairo_line_to(dcx, pos[((spo + 2) & 2) + 1], pos[(spo + 3) & 2]);
	    cairo_line_to(dcx, pos[((spo + 1) & 2) + 1], pos[(spo + 2) & 2]);
	    cairo_line_to(dcx, pos[(spo & 2) + 1], pos[(spo + 1) & 2]);
	    cairo_jagged_edge(dcx, pos[(spo & 2) + 1], pos[(spo + 1) & 2],
			      pos[((spo + 3) & 2) + 1], pos[spo & 2], spo);
	}
	cairo_close_path(dcx);
	cairo_fill(dcx);
    }

    if (_decor)
	ddecor::draw_list(_decor, this, pos, dcx);

    // queue lines
    if (_des->style == destyle_queue) {
	cairo_set_border(dcx, _des->queue_stripe_color, _des->queue_stripe_style, _des->queue_stripe_width);
	int o = _des->orientation;
	double qls = _dess->queue_stripe_spacing;
	int num_lines = (int) ((side_length(o) - std::max(2.0, _dess->padding[o])) / qls);
	double xpos = pos[(o + 2) & 3] + 0.5;
	if (!side_greater(o))
	    qls = -qls, xpos -= 1;
	for (int i = 1; i <= num_lines; ++i) {
	    pos[(o + 2) & 3] = pos[o] = xpos + round(i * qls);
	    cairo_move_to(dcx, pos[((o + 3) & 2) + 1], pos[(o + 3) & 2]);
	    cairo_line_to(dcx, pos[((o + 1) & 2) + 1], pos[(o + 1) & 2]);
	}
	cairo_stroke(dcx);
    }
}

void delt::clip_to_border(dcontext &dcx) const
{
    double x1 = _x, y1 = _y;
    double x2 = _x + _width, y2 = _y + _height;
    cairo_user_to_device(dcx, &x1, &y1);
    cairo_user_to_device(dcx, &x2, &y2);
    x1 = floor(x1);
    y1 = floor(y1);
    x2 = ceil(x2) - 1;
    y2 = ceil(y2) - 1;
    cairo_device_to_user(dcx, &x1, &y1);
    cairo_device_to_user(dcx, &x2, &y2);
    cairo_rectangle(dcx, x1, y1, x2 - x1, y2 - y1);
    cairo_clip(dcx);
}

void delt::draw_text(dcontext &dcx)
{
    // text
    const double *color = _des->color;
    cairo_set_source_rgba(dcx, color[0], color[1], color[2], color[3]);
    pango_layout_set_wrap(dcx, PANGO_WRAP_CHAR);
    pango_layout_set_alignment(dcx, PANGO_ALIGN_CENTER);
    if (dcx.pl_font != _des->font)
	dcx.set_font_description(_des->font);
    if (_markup_width <= -1024)
	dimension_markup(dcx);

    double space[4];
    space[0] = space[1] = space[2] = space[3] = _dess->border_width;
    if (_port_text_offsets) {
	space[_des->orientation] += _port_text_offsets[0];
	space[_des->orientation ^ 2] += _port_text_offsets[1];
    }
    bool saved = false;
    double awidth = _width - space[1] - space[3];
    double aheight = _height - space[0] - space[2];

    if (awidth < _markup_width
	&& aheight > _markup_width
	&& !_elt.size()) {
	// vertical layout
	saved = true;
	cairo_save(dcx);
	if (_markup_height > _width - 2)
	    clip_to_border(dcx);

	double dy = MAX((awidth - _markup_height) / 2, 0);
	cairo_translate(dcx, _x + space[3] + dy, _y + _height - space[2]);
	cairo_rotate(dcx, -M_PI / 2);
	cairo_move_to(dcx, 0, 0);
	cairo_scale(dcx, _dess->scale, _dess->scale);

	pango_layout_set_width(dcx, (int) (aheight / _dess->scale * PANGO_SCALE));
	pango_layout_set_markup(dcx, _markup.data(), _markup.length());
	pango_cairo_show_layout(dcx, dcx);

    } else {
	// normal horizontal layout, no text wrapping
	if (_markup_width > awidth
	    || _markup_height > aheight) {
	    saved = true;
	    cairo_save(dcx);
	    clip_to_border(dcx);
	} else if (_dess->scale != 1) {
	    saved = true;
	    cairo_save(dcx);
	}

	pango_layout_set_width(dcx, (int) (awidth / _dess->scale * PANGO_SCALE));
	pango_layout_set_markup(dcx, _markup.data(), _markup.length());

	double name_width, name_height;
	if (awidth >= _markup_width)
	    name_width = _markup_width, name_height = _markup_height;
	else {
	    name_width = awidth;
	    PangoRectangle rect;
	    pango_layout_get_pixel_extents(dcx, NULL, &rect);
	    name_height = rect.height * _dess->scale;
	}

	double dy = MAX((aheight - name_height - _contents_height) / 2, 1);
	if (_dess->scale == 1)
	    cairo_move_to(dcx, _x + space[3], _y + dy + space[0]);
	else {
	    cairo_translate(dcx, _x + space[3], _y + dy + space[0]);
	    cairo_move_to(dcx, 0, 0);
	    cairo_scale(dcx, _dess->scale, _dess->scale);
	}
	pango_cairo_show_layout(dcx, dcx);
    }

    if (saved)
	cairo_restore(dcx);
}

void delt::draw_drop_shadow(dcontext &dcx)
{
    double shift = (_highlight & (1 << dhlt_pressed)) ? 1 : 0;
    int spo = -1;
    if (_display == dedisp_vsplit) {
	spo = _des->orientation;
	if (_split_type == desplit_inputs)
	    spo = (spo + 2) & 3;
    }
    double sw = _des->shadow_width;
    dcx.penumbra = std::max(dcx.penumbra, sw);
    if (spo != 1 && spo != 2) {
	double x0 = _x + sw - shift, y0 = _y + _height + (sw - shift) / 2;
	double x1 = _x + _width + (sw - shift) / 2, y1 = _y + sw - shift;
	cairo_set_line_width(dcx, sw - shift);
	cairo_move_to(dcx, x0, y0);
	cairo_line_to(dcx, x1, y0);
	cairo_line_to(dcx, x1, y1);
	cairo_stroke(dcx);
    } else {
	cairo_save(dcx);

	// clipping region
	double x0 = _x, y0 = _y + _height;
	double x1 = _x + _width, y1 = _y;
	cairo_move_to(dcx, x0, y0);
	if (spo == 2)
	    cairo_jagged_edge(dcx, x0, y0, x1, y0, spo);
	cairo_line_to(dcx, x1, y0);
	if (spo == 1)
	    cairo_jagged_edge(dcx, x1, y0, x1, y1, spo);
	cairo_line_to(dcx, x1, y1);
	cairo_line_to(dcx, x1 + sw, y1);
	cairo_line_to(dcx, x1 + sw, y0 + sw);
	cairo_line_to(dcx, x0, y0 + sw);
	cairo_close_path(dcx);
	cairo_clip(dcx);

	// actual path
	x0 += sw - shift;
	x1 += sw - shift;
	y0 += sw - shift;
	y1 += sw - shift;
	cairo_move_to(dcx, x0, y0);
	if (spo == 2)
	    cairo_jagged_edge(dcx, x0, y0, x1, y0, spo);
	cairo_line_to(dcx, x1, y0);
	if (spo == 1)
	    cairo_jagged_edge(dcx, x1, y0, x1, y1, spo);
	cairo_line_to(dcx, x1, y1);
	double delta = sw + 5;
	cairo_line_to(dcx, x1 - delta, y1);
	cairo_line_to(dcx, x1 - delta, y0 - delta);
	cairo_line_to(dcx, x0, y0 - delta);
	cairo_close_path(dcx);
	cairo_fill(dcx);

	cairo_restore(dcx);
    }
}

void delt::draw_outline(dcontext &dcx)
{
    // shadow
    if (_des->shadow_style != dshadow_none) {
	const double *color = _des->shadow_color;
	cairo_set_source_rgba(dcx, color[0], color[1], color[2], color[3]);
	cairo_set_dash(dcx, 0, 0, 0);
	if (_des->shadow_style == dshadow_drop)
	    draw_drop_shadow(dcx);
	else {
	    double sw = _des->shadow_width;
	    if (_des->shadow_style == dshadow_unscaled_outline)
		sw /= dcx.scale;
	    dcx.penumbra = std::max(dcx.penumbra, sw);
	    cairo_set_line_width(dcx, sw);
	    double x = _x - sw / 2, y = _y - sw / 2;
	    cairo_move_to(dcx, x, y);
	    cairo_line_to(dcx, x + _width + sw, y);
	    cairo_line_to(dcx, x + _width + sw, y + _height + sw);
	    cairo_line_to(dcx, x, y + _height + sw);
	    cairo_close_path(dcx);
	    cairo_stroke(dcx);
	}
    }

    // outline
    if (_des->border_style != dborder_none) {
	cairo_set_border(dcx, _des->border_color, _des->border_style,
			 _dess->border_width);
	double pos[4];
	double bwd = _dess->border_width / 2;
	pos[3] = _x + bwd;
	pos[0] = _y + bwd;
	pos[1] = _x + _width - bwd;
	pos[2] = _y + _height - bwd;
	int o = _des->orientation, open = (_des->style == destyle_queue);
	if (_display == dedisp_vsplit) {
	    open = (open && _split_type == desplit_inputs ? 2 : 1);
	    o = (_split_type == desplit_inputs ? (o + 2) & 3 : o);
	}
	if (open)
	    pos[o] = side(o);
	if (_des->style == destyle_queue)
	    pos[_des->orientation] = side(_des->orientation);
	cairo_move_to(dcx, pos[((o + 3) & 2) + 1], pos[o & 2]);
	cairo_line_to(dcx, pos[((o + 2) & 2) + 1], pos[(o + 3) & 2]);
	if (open == 2)
	    cairo_move_to(dcx, pos[((o + 1) & 2) + 1], pos[(o + 2) & 2]);
	else
	    cairo_line_to(dcx, pos[((o + 1) & 2) + 1], pos[(o + 2) & 2]);
	cairo_line_to(dcx, pos[(o & 2) + 1], pos[(o + 1) & 2]);
	if (!open)
	    cairo_close_path(dcx);
	cairo_stroke(dcx);

	if (_display == dedisp_vsplit) {
	    const double *color = _des->border_color;
	    cairo_set_source_rgba(dcx, color[0], color[1], color[2], color[3] * 0.25);
	    cairo_set_line_width(dcx, 0.5);
	    cairo_move_to(dcx, pos[(o & 2) + 1], pos[(o + 1) & 2]);
	    cairo_jagged_edge(dcx, pos[(o & 2) + 1], pos[(o + 1) & 2],
			      pos[((o + 3) & 2) + 1], pos[o & 2], o);
	    cairo_line_to(dcx, pos[((o + 3) & 2) + 1], pos[o & 2]);
	    cairo_stroke(dcx);
	}
    }
}

void delt::draw(dcontext &dcx)
{
    if (visible()) {
	int change = 0;
	if (_highlight != _drawn_highlight)
	    change |= dsense_highlight;
	if (dcx.generation != _generation)
	    change |= dsense_always;
	reccss(dcx.cr, change);
	_drawn_highlight = _highlight;
	_generation = dcx.generation;

	if (_highlight & (1 << dhlt_pressed))
	    cairo_translate(dcx, 1, 1);
	draw_background(dcx);
	draw_text(dcx);
	draw_ports(dcx);
	draw_outline(dcx);
	if (_decor)
	    ddecor::draw_list(_decor, this, 0, dcx);
	if (_highlight & (1 << dhlt_pressed))
	    cairo_translate(dcx, -1, -1);
    }
}

void dconn::draw(dcontext &dcx)
{
    int cdisp = change_display(_count_change);
    double width = 1;
    switch (cdisp) {
    case 0:
	cairo_set_source_rgb(dcx, 0.5, 0.5, 0.5);
	break;
    case 1:
	cairo_set_source_rgb(dcx, 0, 0, 0);
	break;
    default: {
	double scale = (double) cdisp / (sizeof(unsigned) * 8 - 1);
	cairo_set_source_rgb(dcx, scale, 0, 0);
	width = 8 * scale + 1;
	break;
    }
    }
    cairo_set_line_width(dcx, width);
    cairo_set_dash(dcx, 0, 0, 0);

    point op = _elt[1]->output_position(_port[1], 0);
    point ip = _elt[0]->input_position(_port[0], 0);
    point next_to_last;

    if (_elt[1]->vertical())
	cairo_move_to(dcx, op.x(), op.y() - 0.5);
    else
	cairo_move_to(dcx, op.x() - 0.5, op.y());

    if ((_elt[1]->vertical() && _elt[0]->vertical()
	 && fabs(ip.x() - op.x()) <= 6)
	|| (!_elt[1]->vertical() && !_elt[0]->vertical()
	    && fabs(ip.y() - op.y()) <= 6)) {
	/* no curves */
	cairo_line_to(dcx, ip.x(), ip.y());
	next_to_last = op;

    } else if (_route.size()) {
	const point *r = _route.begin();
	assert((_route.size() % 3) == 0);
	affine t = affine::mapping(point(0, 0), op, _route.back(), ip);
	for (; r != _route.end(); r += 3)
	    cairo_curve_to_points(dcx, t * r[0], t * r[1], t * r[2]);
	next_to_last = t * _route[_route.size() - 2];

    } else {
	point curvea;
	if (_elt[1]->vertical()) {
	    cairo_line_to(dcx, op.x(), op.y() + 3);
	    curvea = point(op.x(), op.y() + 10);
	} else {
	    cairo_line_to(dcx, op.x() + 3, op.y());
	    curvea = point(op.x() + 10, op.y());
	}
	if (_elt[0]->vertical()) {
	    cairo_curve_to(dcx, curvea.x(), curvea.y(),
			   ip.x(), ip.y() - 12, ip.x(), ip.y() - 7);
	    next_to_last = point(ip.x(), ip.y() - 7);
	} else {
	    cairo_curve_to(dcx, curvea.x(), curvea.y(),
			   ip.x() - 12, ip.y(), ip.x() - 7, ip.y());
	    next_to_last = point(ip.x() - 7, ip.y());
	}
	cairo_line_to(dcx, ip.x(), ip.y());

    }

    cairo_stroke(dcx);

    double epx = ip.x(), epy = ip.y();
    (_elt[0]->vertical() ? epy : epx) += 0.25;
    double angle = (ip - next_to_last).angle();
    cairo_move_to(dcx, epx, epy);
    double arrow_width = 3 * (width + 3) / 4;
    cairo_rel_line_to_point(dcx, point(-5.75, -arrow_width).rotated(angle));
    cairo_rel_line_to_point(dcx, point(+2.00, +arrow_width).rotated(angle));
    cairo_rel_line_to_point(dcx, point(-2.00, +arrow_width).rotated(angle));
    cairo_close_path(dcx);
    cairo_fill(dcx);
}

/*****
 *
 * Dragging
 *
 */

void delt::drag_prepare()
{
    if (_visible) {
	_xrect = *this;
	for (std::vector<delt *>::iterator ei = _elt.begin();
	     ei != _elt.end(); ++ei)
	    (*ei)->drag_prepare();
    }
}

void delt::drag_shift(wdiagram *d, const point &delta)
{
    if (_visible) {
	rectangle bounds = *this;
	remove(d->rects(), bounds);
	_x = _xrect._x + delta.x();
	_y = _xrect._y + delta.y();
	insert(d->rects(), d->main(), bounds);
	d->redraw(bounds);
	for (std::vector<delt *>::iterator ei = _elt.begin();
	     ei != _elt.end(); ++ei)
	    (*ei)->drag_shift(d, delta);
    }
}

void delt::drag_size(wdiagram *d, const point &delta, int direction)
{
    rectangle bounds = *this;
    remove(d->rects(), bounds);

    int vtype = direction % 3;
    int htype = direction - vtype;

    if (vtype == deg_border_top
	&& _xrect._height - delta.y() >= min_height()) {
	_y = _xrect._y + delta.y();
	_height = _xrect._height - delta.y();
    } else if (vtype == deg_border_bot
	       && _xrect._height + delta.y() >= min_height())
	_height = _xrect._height + delta.y();

    if (htype == deg_border_lft
	&& _xrect._width - delta.x() >= min_width()) {
	_x = _xrect._x + delta.x();
	_width = _xrect._width - delta.x();
    } else if (htype == deg_border_rt
	       && _xrect._width + delta.x() >= min_width())
	_width = _xrect._width + delta.x();

    _aligned = false;
    insert(d->rects(), d->main(), bounds);
    d->redraw(bounds);
}

bool delt::drag_canvas_changed(const rectangle &canvas) const
{
    return !(_xrect.x() > canvas.x() + drag_threshold
	     && x() > canvas.x() + drag_threshold
	     && _xrect.x2() < canvas.x2() - drag_threshold
	     && x2() < canvas.x2() - drag_threshold
	     && _xrect.y() > canvas.y() + drag_threshold
	     && y() > canvas.y() + drag_threshold
	     && _xrect.y2() < canvas.y2() - drag_threshold
	     && y2() < canvas.y2() - drag_threshold);
}

/*****
 *
 * gadgets
 *
 */

handler_value *delt::handler_interest(crouter *cr, const String &hname,
				      bool autorefresh,
				      int autorefresh_period,
				      bool always)
{
    return cr->hvalues().find_placeholder
	(_flat_name + "." + hname,
	 hflag_notify_delt | (autorefresh ? hflag_autorefresh : 0)
	 | (always ? hflag_always_notify_delt : 0),
	 autorefresh_period);
}

void delt::notify_read(wdiagram *d, handler_value *hv)
{
    ddecor::notify_list(_decor, d->main(), this, hv);
    if (reccss(d->main(), dsense_handler)) {
	d->redraw(*this);
	if (_split && _split->visible())
	    d->redraw(*_split);
    }
}

bool dconn::change_count(unsigned new_count)
{
    if (_count_last == ~0U && _count_change == ~0U) {
	_count_last = new_count;
	return false;
    } else {
	unsigned old_change = _count_change;
	_count_change = new_count - _count_last;
	_count_last = new_count;
	return change_display(old_change) != change_display(_count_change);
    }
}

/*****
 *
 * parts
 *
 */

int delt::find_gadget(wdiagram *d, double window_x, double window_y) const
{
    rectangle r = d->canvas_to_window(*this);
    if (!r.contains(window_x, window_y))
	return deg_none;
    //r.integer_align();

    double attach = MAX(2.0, d->scale() * _dess->border_width);
    if (d->scale_step() > -5
	&& r.width() >= 18
	&& r.height() >= 18
	&& (window_x - r.x1() < attach
	    || window_y - r.y1() < attach
	    || r.x2() - window_x - 1 < attach
	    || r.y2() - window_y - 1 < attach)) {
	int gnum = deg_element;
	if (window_x - r.x1() < 10)
	    gnum += deg_border_lft;
	else if (r.x2() - window_x < 10)
	    gnum += deg_border_rt;
	if (window_y - r.y1() < 10)
	    gnum += deg_border_top;
	else if (r.y2() - window_y < 10)
	    gnum += deg_border_bot;
	return gnum;
    }

    return deg_element;
}

}
