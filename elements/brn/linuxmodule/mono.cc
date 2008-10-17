#include <click/config.h>
#include "mono.hh"
#include <click/confparse.hh>
#include <click/error.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <linux/proc_fs.h>
#include <linux/wait.h>

// TODO move to seperate file
// redefines some kernel macros
// because g++ does not support C99 designated-initializer syntax
// see http://gcc.gnu.org/ml/gcc-patches/2004-10/msg01964.html
// this can be savely removed, if g++ accepts those

// redefining macros to compile with g++
#define DEFINE_WAIT(name)	\
	wait_queue_t name;  \
	name.xxx_private = current; \
	name.func = autoremove_wake_function; \
	LIST_HEAD_INIT((name).task_list);

#define LIST_HEAD_INIT(name) \
	name.next = &(name); \
	name.prev = &(name)


CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

// needed for static initialize
// see
// http://www.osdev.org/wiki/C_PlusPlus
/*
extern "C"
{
	int __cxa_atexit(void (*f)(void *), void *p, void *d);
	void __cxa_finalize(void *d);
};
*/
//void *__dso_handle; /*only the address of this symbol is taken by gcc*/
/*
struct object
{
	void (*f)(void*);
    void *p;
    void *d;
} object[32] = {0};
unsigned int iObject = 0;

int __cxa_atexit(void (*f)(void *), void *p, void *d)
{
	if (iObject >= 32)
		return -1;
    object[iObject].f = f;
    object[iObject].p = p;
    object[iObject].d = d;
    ++iObject;
    return 0;
}
*/
/* This currently destroys all objects */
/*
void __cxa_finalize(void *d)
{
	unsigned int i = iObject;
    for (; i > 0; --i) {
    	--iObject;
        object[iObject].f(object[iObject].p);
    }
}
*/

CLICK_DECLS

Mono::Mono()
	: brn(NULL),
	  events(NULL),
	  _event_notifier(NULL)
	  //_new_neighbor(false)
{
}

Mono::~Mono()
{
}

wait_queue_head_t wq;
bool _new_neighbor = false;
//Mono::_new_neighbor = false;

/*
struct context {
	struct proc_dir_entry* proc_entry;
	void* element;
};
*/

struct file_operations fops; 

int
Mono::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Element *e = NULL;
    
    if (cp_va_parse(conf, this, errh,
		    cpKeywords,
		    "NOTIFIER", cpElement, "event notifier", &e,
		    cpEnd) < 0)
	return -1;
    
    if (e && !(_event_notifier = (EventNotifier *)e->cast("EventNotifier")))
		return errh->error("%s is not an EventNotifier", e->name().c_str());
	
    return 0;
}

int
Mono::initialize(ErrorHandler *errh)
{
	if (_event_notifier != NULL)
		_event_notifier->add_listener(this);
	
	init_waitqueue_head(&wq);

	// create proc entries	
	brn = proc_mkdir(PROC_BRN_DIR, NULL);
	if (brn == NULL)
		return errh->error("Could not create directory %s in proc", PROC_BRN_DIR);
	
	events = proc_mkdir(PROC_EVENTS_DIR, brn);
	if (events == NULL)
		return errh->error("Could not create directory %s in brn", PROC_EVENTS_DIR);

	// create files for reading
	struct proc_dir_entry* file;
	file = create_proc(errh, 0444, PROC_FILE_NEIGHBORS, brn);
	
	if (file == NULL)
		return -1;
	file->read_proc = read_func;
	// ...

	// create files for events
	file = create_proc(errh, 0444, PROC_EVENT_NEWNEIGHBOR, events);
	if (file == NULL)
		return -1;
    //file->read_proc = event_func;
    file->read_proc = new_neighbor_event_func;
    
    
    // adding poll
	fops = *file->proc_fops;
	fops.poll = new_neighbor_event_poll;
	file->proc_fops = &fops;
	// ...
	
	
	// add some sample neighbors
	_neighbors.push_back(EtherAddress((const unsigned char *) "\001\002\000\004\005\006"));
	_neighbors.push_back(EtherAddress((const unsigned char *) "\012\006\088\097\012\023"));
	_neighbors.push_back(EtherAddress((const unsigned char *) "\000\000\000\000\000\001"));

	return 0;
}

proc_dir_entry*
Mono::create_proc(ErrorHandler *errh, mode_t mode, char *name, proc_dir_entry* dir)
{
	struct proc_dir_entry* file;
	file = create_proc_entry(name, mode, dir);

	if (file == NULL) {
		errh->error("Creating proc file %s failed.", name);
		return NULL;
	}

	// TODO
	// allocate memory
	// 
	
	/*
	struct context* con = (struct context *) kmalloc(sizeof(struct context), GFP_KERNEL);
	// TODO
	//if (con == NULL)
	
	con->proc_entry = file;
	con->element = this;
	file->data = con;
	*/
	
	file->data = this;
	
	return file;
}


int
Mono::read_func(char* page, char** start, off_t off, int count, int* eof, void* data) {
	Mono *mono = static_cast<Mono *>(data);
	
	int i = 0;
    while (i < mono->_neighbors.size()) {
      	if (memcpy(page + off + i * 6, mono->_neighbors[i].data(), 6) == NULL)
  			break;
    	
    	++i;
    }

	if (i != mono->_neighbors.size())
		BRN_ERROR("Problem copying ether addresses");
	
	return i * 6;
}

/*
int
Mono::event_func(char* page, char** start, off_t off, int count, int* eof, void* data) {

	struct context *con = (struct context *) data; 

	struct proc_dir_entry *proc = con->proc_entry;
	String file(proc->name);

	Mono *mono = static_cast<Mono *>(con->element);
	
		int ret = 0;

		// put process to sleep
		ret = wait_event_interruptible(wq, mono->have_event(file));

		// test if we were interrupted
		if (ret != 0)
			return -ERESTARTSYS; // ?? EINTR
	
	// assert != NULL
	const char* bytes = mono->get_event(file);
	
	click_chatter("Bytes %s", bytes);
	
	// TODO
	int len = 7;
	if (memcpy(page + off, bytes, len) == NULL)
		len = 0;
	
	click_chatter("Processing event; len = %u", len);
		
	mono->remove_event(file);
		
	return len;
	
	// TODO should adjust behaviour
	// we don't honor O_NONBLOCK yet, while opening
	// return -EAGAIN;

	// TODO
	// what about concurrent readers
	// only one reader should be allowed, else code needs to be adjusted
	// events are multiplied within Mono
}
*/

unsigned int
Mono::new_neighbor_event_poll(struct file *filp, poll_table *wait) {
	//filp->private_data;
	unsigned int mask = 0;
	
	BRN_DEBUG("Executing poll");
	
	poll_wait(filp, &wq, wait);
	
	if (_new_neighbor)
		mask |= POLLIN | POLLRDNORM;
		
	return mask;
}

int
Mono::new_neighbor_event_func(char* page, char** start, off_t off, int count, int* eof, void* data) {
	Mono *mono = static_cast<Mono *>(data);
	
	// put process to sleep
	// and test if we were interrupted
	while(!_new_neighbor) {
	
		if (wait_event_interruptible(wq, _new_neighbor) != 0)
			return -ERESTARTSYS; // ?? EINTR
	}
	//
	_new_neighbor = false;
	
	String file = String(PROC_EVENT_NEWNEIGHBOR);
	
	// assert != NULL
	const char* bytes = mono->get_event(file);
	
	
	BRN_DEBUG("Bytes %s", bytes);
	
	// TODO
	int len = 6;
	if (memcpy(page + off, bytes, len) == NULL)
		len = 0;
	
	BRN_DEBUG("Processing event; len = %u", len);
		
	mono->remove_event(file);
		
	return len;
	
	// TODO should adjust behaviour
	// we don't honor O_NONBLOCK yet, while opening
	// return -EAGAIN;

	// TODO
	// implement poll to have working select
	
	// TODO
	// what about concurrent readers
	// only one reader should be allowed, else code needs to be adjusted
	// events are multiplied within Mono
}


void
Mono::add_event(char *file, char* data, unsigned long data_length) {
	BRN_DEBUG("Added event");
	
	// get memory for event
	// TODO change to kmalloc
	char* bytes = (char *) kmalloc(data_length + 1, GFP_KERNEL);
	if (data == NULL)
		return;
	
	// TODO
	// this doesn't work
	// what, if e.g. an ether address has a \0 byte
	memset(bytes, '\0', data_length + 1);
	memcpy(bytes, data, data_length);
	
	_events.insert(String(file), bytes);
	
	_new_neighbor = true;

	// ... and wake up processes
	//wake_up_interruptible(&wq);
	wake_up(&wq);
}

void
Mono::remove_event(String file) {
	char* data = get_event(file);
	if (data != NULL)
		kfree(data);
	
	_events.remove(file);
}

bool
Mono::have_event(String file) {
	return _events.find(file, NULL) != NULL;	
}

char*
Mono::get_event(String file) {
	return _events.find(file, NULL);	
}


void
Mono::notify(Event e) {
	if (e._type == NewNeighborEvent) {
		NewNeighbor *ee = static_cast<NewNeighbor *>(&e);
		
		// add event
		add_event(PROC_EVENT_NEWNEIGHBOR, (char *) ee->_eth.data(), 6);
	}
	else {
		BRN_WARN("Unhandled Event");	
	}
}

void
Mono::cleanup(CleanupStage)
{
	// remove events
	remove_proc_entry(PROC_EVENT_NEWNEIGHBOR, events);
	
	// remove files
	remove_proc_entry(PROC_FILE_NEIGHBORS, brn);
	
	// remove dirs
	remove_proc_entry(PROC_EVENTS_DIR, brn);
	remove_proc_entry(PROC_BRN_DIR, NULL);
}

ELEMENT_REQUIRES(linuxmodule)
EXPORT_ELEMENT(Mono)

#include <click/hashmap.cc>
#include <click/vector.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<String, unsigned char*>;
template class Vector<EtherAddress>;
#endif
CLICK_ENDDECLS
