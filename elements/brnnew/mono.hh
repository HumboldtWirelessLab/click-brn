#ifndef CLICK_MONO_HH
#define CLICK_MONO_HH
#include "brnelement.hh"
#include <click/hashmap.hh>
#include <click/vector.hh>
#include "eventnotifier.hh"

CLICK_DECLS

class Mono : public BRNElement, public EventListener { public:

    Mono();
    ~Mono();

    const char *class_name() const	{ return "Mono"; }
    const char *port_count() const	{ return PORTS_0_0; }
    
	void notify(Event);

    //int configure_phase() const		{ return CONFIGURE_PHASE_FROMHOST; }
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);



  private:
  	EventNotifier *_event_notifier;
  	
  	// proc stuff
  	// dirs
    #define PROC_BRN_DIR "brn"
    #define PROC_EVENTS_DIR "events"
    
    // read/write
    #define PROC_FILE_NEIGHBORS "neighbors"
    
    // events
    #define PROC_EVENT_NEWNEIGHBOR "new_neighbor"
    
    struct proc_dir_entry* brn;
    struct proc_dir_entry* events;
    
	proc_dir_entry* create_proc(ErrorHandler *, mode_t, char *, proc_dir_entry *);

    static int read_func(char* page, char** start, off_t off, int count, int* eof, void* data);
    //static int event_func(char* page, char** start, off_t off, int count, int* eof, void* data);
    
    static unsigned int new_neighbor_event_poll(struct file *filp, poll_table *wait);
    static int new_neighbor_event_func(char* page, char** start, off_t off, int count, int* eof, void* data);
	
    
	// handling events
	typedef HashMap<String, char*> PendingEvents;
    PendingEvents _events;
	
	void add_event(char*, char*, unsigned long);
	void remove_event(String);
	inline bool have_event(String);
	char* get_event(String);
	
	//
	typedef Vector<EtherAddress> Neighbors;
    Neighbors _neighbors;
    
};

CLICK_ENDDECLS
#endif
