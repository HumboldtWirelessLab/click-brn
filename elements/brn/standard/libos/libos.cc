#include <click/config.h>
#include <click/error.hh>
#include <click/args.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/handlercall.hh>

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <dlfcn.h>

#include "sim.h"
#include "sim-types.h"
#include "sim-init.h"

#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "libos.hh"

CLICK_DECLS

LibOS::LibOS()
  : _timer(this),
     next_pid(0)
{
  BRNElement::init();
}

LibOS::~LibOS()
{
}

int
LibOS::configure(Vector<String> &conf, ErrorHandler *errh)
{
  Args args = Args(this, errh).bind(conf);

  if (args.read_p("DEBUG", _debug)
          .complete() < 0)
      return -1;

  return 0;
}

int
LibOS::initialize(ErrorHandler *errh)
{
  BRN_DEBUG("init start");

  _timer.initialize(this);

  click_brn_srandom();

  init_libos();

  BRN_DEBUG("init end");

  return 0;
}

void
LibOS::run_timer(Timer* )
{
  BRN_DEBUG("RunTimer");
}


void
LibOS::init_libos()
{
  void *handle;
  void *symbol;

  if ((handle = dlopen("./libsim-linux.so",RTLD_LAZY)) == NULL ) {
    printf("LibOS not found.\n");
  }

  symbol = dlsym(handle, "sim_init");
  SimInit init = (SimInit) symbol;
  if (init == NULL) {
    BRN_ERROR("Oops. Can't find initialization function");
  }

  m_exported = new struct SimExported();

  imported.vprintf = &LibOS::Vprintf;
  imported.malloc = &LibOS::Malloc;
  imported.free = &LibOS::Free;
  imported.memcpy = &LibOS::Memcpy;
  imported.memset = &LibOS::Memset;
  imported.atexit = &LibOS::AtExit;
  imported.access = &LibOS::Access;
  imported.getenv = &LibOS::Getenv;
  imported.mkdir = &LibOS::Mkdir;
  imported.open = &LibOS::Open;
  imported.__fxstat = &LibOS::__Fxstat;
  imported.fseek = &LibOS::Fseek;
  imported.setbuf = &LibOS::Setbuf;
  imported.ftell = &LibOS::Ftell;
  imported.fdopen = &LibOS::FdOpen;
  imported.fread = &LibOS::Fread;
  imported.fwrite = &LibOS::Fwrite;
  imported.fclose = &LibOS::Fclose;
  imported.random = &LibOS::Random;
  imported.event_schedule_ns = &LibOS::EventScheduleNs;
  imported.event_cancel = &LibOS::EventCancel;
  imported.current_ns = &CurrentNs;
  imported.task_start = &LibOS::TaskStart;
  imported.task_wait = &LibOS::TaskWait;
  imported.task_current = &LibOS::TaskCurrent;
  imported.task_wakeup = &LibOS::TaskWakeup;
  imported.task_yield = &LibOS::TaskYield;
  imported.dev_xmit = &LibOS::DevXmit;
  imported.signal_raised = &LibOS::SignalRaised;
  imported.poll_event = &LibOS::PollEvent;

  init(m_exported, &imported, (struct SimKernel *)this);

  dlclose(handle);

  start_task = m_exported->task_create(NULL,0);
  /*struct SimSocket *sim_socket;
  int result = m_exported->sock_socket(0x7f000001, AF_INET,SOCK_STREAM,&sim_socket);
  click_chatter("result: %d",result);
*/
}

/*********************************************************************************/
/*************************** F U N C T I O N S ***********************************/
/*********************************************************************************/

int
LibOS::Vprintf (struct SimKernel *kernel, const char *str, va_list args)
{
  click_chatter("%s",__func__);
  //fprintf(stderr,str,args);
  //click_chatter(str, args);
  return 0;
}

void *
LibOS::Malloc (struct SimKernel *kernel, unsigned long size)
{
  //click_chatter("%s",__func__);
  return malloc(size);
}

void
LibOS::Free (struct SimKernel *kernel, void *ptr)
{
  //click_chatter("%s",__func__);
  free(ptr);
}

void *
LibOS::Memcpy (struct SimKernel *kernel, void *dst, const void *src, unsigned long size)
{
  //click_chatter("%s",__func__);
  return memcpy(dst, src, size);
}

void *
LibOS::Memset (struct SimKernel *kernel, void *dst, char value, unsigned long size)
{
  //click_chatter("%s",__func__);
  return memset(dst, value, size);
}

int
LibOS::AtExit (struct SimKernel *kernel, void (*function)(void))
{
  click_chatter("%s",__func__);
  return 0;
}

int
LibOS::Access (struct SimKernel *kernel, const char *pathname, int mode)
{
  click_chatter("%s",__func__);
  return 0;
}

char*
LibOS::Getenv (struct SimKernel *kernel, const char *name)
{
  click_chatter("%s",__func__);
  return NULL;
}

int
LibOS::Mkdir (struct SimKernel *kernel, const char *pathname, mode_t mode)
{
  click_chatter("%s",__func__);
  return 0;
}

int
LibOS::Open (struct SimKernel *kernel, const char *pathname, int flags)
{
  click_chatter("%s",__func__);
  return 0;
}

int
LibOS::__Fxstat (struct SimKernel *kernel, int ver, int fd, void *buf)
{
  click_chatter("%s",__func__);
  return 0;
}

int
LibOS::Fseek (struct SimKernel *kernel, FILE *stream, long offset, int whence)
{
  click_chatter("%s",__func__);
  return 0;
}

void
LibOS::Setbuf (struct SimKernel *kernel, FILE *stream, char *buf)
{
  click_chatter("%s",__func__);
  return;
}

long
LibOS::Ftell (struct SimKernel *kernel, FILE *stream)
{
  click_chatter("%s",__func__);
  return 0;
}

FILE*
LibOS::FdOpen (struct SimKernel *kernel, int fd, const char *mode)
{
  click_chatter("%s",__func__);
  return NULL;
}

size_t
LibOS::Fread (struct SimKernel *kernel, void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  click_chatter("%s",__func__);
  return 0;
}

size_t
LibOS::Fwrite (struct SimKernel *kernel, const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  click_chatter("%s",__func__);
  return 0;
}

int
LibOS::Fclose (struct SimKernel *kernel, FILE *fp)
{
  click_chatter("%s",__func__);
  return fclose (fp);
}

unsigned long
LibOS::Random (struct SimKernel *kernel)
{
  //click_chatter("%s",__func__);
  return click_random();
}

void
LibOS::EventTrampoline (void (*fn)(void *context),
                                       void *context, void (*pre_fn)(void),
                                       Vector<void*> event)
{
  click_chatter("%s",__func__);
}

void *
LibOS::EventScheduleNs (struct SimKernel *kernel, __u64 ns, void (*fn)(void *context), void *context,
                                       void (*pre_fn)(void))
{
  click_chatter("%s",__func__);
  return NULL;
}

void
LibOS::EventCancel (struct SimKernel *kernel, void *ev)
{
  click_chatter("%s",__func__);
}

uint64_t
LibOS::CurrentNs (struct SimKernel *kernel)
{
  uint64_t t = Timestamp::now().nsecval();
  click_chatter("%s: %lu",__func__,t);
  return t;
}

void
LibOS::TaskSwitch (int type, void *context)
{
  click_chatter("%s",__func__);
}

struct SimTask *
LibOS::TaskStart (struct SimKernel *kernel, void (*callback)(void *), void *context)
{
  click_chatter("%s",__func__);

  LibOS *libos = (LibOS*)kernel;

  libos->next_pid++;

  return libos->m_exported->task_create(context,libos->next_pid);
}

struct SimTask *
LibOS::TaskCurrent (struct SimKernel *kernel)
{
  click_chatter("%s",__func__);
  LibOS *libos = (LibOS*)kernel;
  return libos->start_task;
}

void
LibOS::TaskWait (struct SimKernel *kernel)
{
  click_chatter("%s",__func__);
}

int
LibOS::TaskWakeup (struct SimKernel *kernel, struct SimTask *task)
{
  click_chatter("%s",__func__);
  return 0;
}

void
LibOS::TaskYield (struct SimKernel *kernel)
{
  click_chatter("%s",__func__);
}

void
LibOS::SendMain (bool *r, void *dev, Vector<Packet*> p, const EtherAddress& d, uint16_t pro)
{
  click_chatter("%s",__func__);
}

void
LibOS::DevXmit (struct SimKernel *kernel, struct SimDevice *dev, unsigned char *data, int len)
{
  click_chatter("%s",__func__);
}

void
LibOS::SignalRaised (struct SimKernel *kernel, struct SimTask *task, int signalNumber)
{
  click_chatter("%s",__func__);
}

struct SimDevice *
LibOS::DevToDev (Vector<void*> device)
{
  click_chatter("%s",__func__);
  return NULL;
}

void
LibOS::RxFromDevice (Vector<void*> device, Vector<Packet*> p,
                                    uint16_t protocol, const EtherAddress & from,
                                    const EtherAddress &to, int type)
{
  click_chatter("%s",__func__);
}

void
LibOS::NotifyDeviceStateChange (Vector<void*> device)
{
  click_chatter("%s",__func__);
}

void
LibOS::NotifyDeviceStateChangeTask (Vector<void*> device)
{
  click_chatter("%s",__func__);
}

void
LibOS::ScheduleTaskTrampoline (void *context)
{
  click_chatter("%s",__func__);
}

void
LibOS::ScheduleTask (void *event)
{
  click_chatter("%s",__func__);
}

void
LibOS::NotifyAddDevice (Vector<void*> device)
{
  click_chatter("%s",__func__);
}

void
LibOS::NotifyAddDeviceTask (Vector<void*> device)
{
  click_chatter("%s",__func__);
}

void*
LibOS::CreateSocket (int domain, int type, int protocol)
{
  click_chatter("%s",__func__);
  return NULL;
}

int
LibOS::Close (struct SimSocket *socket)
{
  click_chatter("%s",__func__);
  return 0;
}

ssize_t
LibOS::Recvmsg (struct SimSocket *socket, struct msghdr *msg, int flags)
{
  click_chatter("%s",__func__);
  return 0;
}

ssize_t
LibOS::Sendmsg (struct SimSocket *socket, const struct msghdr *msg, int flags)
{
  click_chatter("%s",__func__);
  return 0;
}

int
LibOS::Getsockname (struct SimSocket *socket, struct sockaddr *name, socklen_t *namelen)
{
  click_chatter("%s",__func__);
  return 0;
}

int
LibOS::Getpeername (struct SimSocket *socket, struct sockaddr *name, socklen_t *namelen)
{
  click_chatter("%s",__func__);
  return 0;
}

int
LibOS::Bind (struct SimSocket *socket, const struct sockaddr *my_addr, socklen_t addrlen)
{
  click_chatter("%s",__func__);
  return 0;
}

int
LibOS::Connect (struct SimSocket *socket, const struct sockaddr *my_addr,
                               socklen_t addrlen, int flags)
{
  click_chatter("%s",__func__);
  return 0;
}

int
LibOS::Listen (struct SimSocket *socket, int backlog)
{
  click_chatter("%s",__func__);
  return 0;
}

int
LibOS::Shutdown (struct SimSocket *socket, int how)
{
  click_chatter("%s",__func__);
  return 0;
}

int
LibOS::Accept (struct SimSocket *socket, struct sockaddr *my_addr, socklen_t *addrlen, int flags)
{
  click_chatter("%s",__func__);
  return 0;
}

int
LibOS::Ioctl (struct SimSocket *socket, unsigned long request, char *argp)
{
  click_chatter("%s",__func__);
  return 0;
}

int
LibOS::Setsockopt (struct SimSocket *socket, int level, int optname,
                                  const void *optval, socklen_t optlen)
{
  click_chatter("%s",__func__);
  return 0;
}

int
LibOS::Getsockopt (struct SimSocket *socket, int level, int optname,
                                  void *optval, socklen_t *optlen)
{
  click_chatter("%s",__func__);
  return 0;
}

void
LibOS::PollEvent (int flag, void *context)
{
  click_chatter("%s",__func__);
}

/**
 * Struct used to pass pool table context between DCE and Kernel and back from Kernel to DCE
 *
 * When calling sock_poll we provide in ret field the wanted eventmask, and in the opaque field
 * the DCE poll table
 *
 * if a corresponding event occurs later, the PollEvent will be called by kernel with the DCE
 * poll table in context variable, then we will able to wake up the thread blocked in poll call.
 *
 * Back from sock_poll method the kernel change ret field with the response from poll return of the
 * corresponding kernel socket, and in opaque field there is a reference to the kernel poll table
 * we will use this reference to remove us from the file wait queue when ending the DCE poll call or
 * when ending the DCE process which is currently polling.
 *
 */
struct poll_table_ref
{
  int ret;
  void *opaque;
};

int
LibOS::Poll(struct SimSocket *socket, void/*PollTable*/* ptable)
{
  click_chatter("%s",__func__);
  return 0;
}

void
LibOS::PollFreeWait (void *ref)
{
  click_chatter("%s",__func__);
}

/*********************************************************************************/
/*********************** F U N C T I O N S   E N D *******************************/
/*********************************************************************************/

String
LibOS::status()
{
  StringAccum sa;

  sa << "<libos node=\"" << "BRN_NODENAME" << "\" >\n";

  sa << "</libos>\n";

  return sa.take_string();
}

enum {
  H_STATUS
};

static String
read_param(Element *e, void *vparam)
{
  LibOS *f = (LibOS *)e;
  switch((intptr_t)vparam) {
    case H_STATUS:
      return f->status();
      break;
  }

  return String();
}


void
LibOS::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("status", read_param, (void *) H_STATUS);
}

CLICK_ENDDECLS
//ELEMENT_REQUIRES(ns userlevel)
EXPORT_ELEMENT(LibOS)
