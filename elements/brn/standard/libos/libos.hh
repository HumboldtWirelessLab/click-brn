#ifndef CLICK_LIBOS_HH
#define CLICK_LIBOS_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/timer.hh>
#include <sys/un.h>

#include "sim.h"
#include "sim-types.h"
#include "sim-init.h"

#include "elements/brn/brnelement.hh"

CLICK_DECLS


class LibOS : public BRNElement {
 public:

  LibOS();
  ~LibOS();

  const char *class_name() const	{ return "LibOS"; }
  const char *port_count() const	{ return "0/0"; }

  int configure(Vector<String> &conf, ErrorHandler *);
  int initialize(ErrorHandler *);

  void add_handlers();

  void run_timer(Timer* );

  Timer _timer;

  void init_libos();

  String status();

  struct SimExported *m_exported;
  struct SimImported imported;
  unsigned long next_pid;
  struct SimTask *start_task;


  static int Vprintf (struct SimKernel *kernel, const char *str, va_list args);
  static void *Malloc (struct SimKernel *kernel, unsigned long size);
  static void Free (struct SimKernel *kernel, void *ptr);
  static void *Memcpy (struct SimKernel *kernel, void *dst, const void *src, unsigned long size);
  static void *Memset (struct SimKernel *kernel, void *dst, char value, unsigned long size);
  static int AtExit (struct SimKernel *kernel, void (*function)(void));
  static int Access (struct SimKernel *kernel, const char *pathname, int mode);
  static char* Getenv (struct SimKernel *kernel, const char *name);
  static int Mkdir (struct SimKernel *kernel, const char *pathname, mode_t mode);
  static int Open (struct SimKernel *kernel, const char *pathname, int flags);
  static int __Fxstat (struct SimKernel *kernel, int ver, int fd, void *buf);
  static int Fseek (struct SimKernel *kernel, FILE *stream, long offset, int whence);
  static void Setbuf (struct SimKernel *kernel, FILE *stream, char *buf);
  static long Ftell (struct SimKernel *kernel, FILE *stream);
  static FILE* FdOpen (struct SimKernel *kernel, int fd, const char *mode);
  static size_t Fread (struct SimKernel *kernel, void *ptr, size_t size, size_t nmemb, FILE *stream);
  static size_t Fwrite (struct SimKernel *kernel, const void *ptr, size_t size, size_t nmemb, FILE *stream);
  static int Fclose (struct SimKernel *kernel, FILE *fp);
  static unsigned long Random (struct SimKernel *kernel);
  static void EventTrampoline (void (*fn)(void *context), void *context, void (*pre_fn)(void), Vector<void*> event);
  static void *EventScheduleNs (struct SimKernel *kernel, uint64_t ns, void (*fn)(void *context), void *context, void (*pre_fn)(void));
  static void EventCancel (struct SimKernel *kernel, void *ev);
  static uint64_t CurrentNs (struct SimKernel *kernel);
  static void TaskSwitch (int type, void *context);
  static struct SimTask *TaskStart (struct SimKernel *kernel, void (*callback)(void *), void *context);
  static struct SimTask *TaskCurrent (struct SimKernel *kernel);
  static void TaskWait (struct SimKernel *kernel);
  static int TaskWakeup (struct SimKernel *kernel, struct SimTask *task);
  static void TaskYield (struct SimKernel *kernel);
  static void SendMain (bool *r, void *dev, Vector<Packet*> p, const EtherAddress& d, uint16_t pro);
  static void DevXmit (struct SimKernel *kernel, struct SimDevice *dev, unsigned char *data, int len);
  static void SignalRaised (struct SimKernel *kernel, SimTask *task, int signalNumber);
  static struct SimDevice *DevToDev (Vector<void*> device);
  static void RxFromDevice (Vector<void*> device, Vector<Packet*> p,
                                    uint16_t protocol, const EtherAddress & from,
                                    const EtherAddress &to, int type);
  static void NotifyDeviceStateChange (Vector<void*> device);
  static void NotifyDeviceStateChangeTask (Vector<void*> device);
  static void ScheduleTaskTrampoline (void *context);
  static void ScheduleTask (void *event);
  static void NotifyAddDevice (Vector<void*> device);
  static void NotifyAddDeviceTask (Vector<void*> device);
  static void *CreateSocket (int domain, int type, int protocol);
  static int Close (struct SimSocket *socket);;
  static ssize_t Recvmsg (struct SimSocket *socket, struct msghdr *msg, int flags);
  static ssize_t Sendmsg (struct SimSocket *socket, const struct msghdr *msg, int flags);
  static int Getsockname (struct SimSocket *socket, struct sockaddr *name, socklen_t *namelen);
  static int Getpeername (struct SimSocket *socket, struct sockaddr *name, socklen_t *namelen);
  static int Bind (struct SimSocket *socket, const struct sockaddr *my_addr, socklen_t addrlen);
  static int Connect (struct SimSocket *socket, const struct sockaddr *my_addr, socklen_t addrlen, int flags);
  static int Listen (struct SimSocket *socket, int backlog);
  static int Shutdown (struct SimSocket *socket, int how);
  static int Accept (struct SimSocket *socket, struct sockaddr *my_addr, socklen_t *addrlen, int flags);
  static int Ioctl (struct SimSocket *socket, unsigned long request, char *argp);
  static int Setsockopt (struct SimSocket *socket, int level, int optname, const void *optval, socklen_t optlen);
  static int Getsockopt (struct SimSocket *socket, int level, int optname, void *optval, socklen_t *optlen);
  static void PollEvent (int flag, void *context);
  static int Poll(struct SimSocket *socket, void/*PollTable*/* ptable);
  static void PollFreeWait (void *ref);

};

CLICK_ENDDECLS
#endif
