/* Copyright (C) 2005 BerlinRoofNet Lab
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA. 
 *
 * For additional licensing options, consult http://www.BerlinRoofNet.de 
 * or contact brn@informatik.hu-berlin.de. 
 */

#include <click/config.h>
#include <click/pathvars.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif //HAVE_UNISTD_H

//#include <clicknet/wifi.h>

#include <click/router.hh>
#include <click/error.hh>
#include <click/string.hh>
#include <click/glue.hh>
#include <click/driver.hh>
#include <click/master.hh>
#include <click/simclick.h>
#include <click/handlercall.hh>

#include <clicknet/wifi.h>
#include "jni_click_runtime_ClickAdapter.hh"
#include "clickadapter.hh"
#include "java_array.hh"

CLICK_USING_DECLS

#define SIMCLICK_PTYPE_WIFI 3
#define SIMCLICK_PTYPE_WIFI_EXTRA 4

#define IFID_LASTIF 32

////////////////////////////////////////////////////////////////////////////////

//
// State for each simulated machine
//

class SimState {
public:
  Router *router;
  bool* _nics_ready;
  int64_t last_scheduled;

  SimState() {
    router = NULL;
    _nics_ready = new bool[IFID_LASTIF];
    last_scheduled = -1L;
  }

  ~SimState() {
    delete[] _nics_ready;
  }

  static SimState* simmain(simclick_node_t *simnode,const char* router_file);
  static bool didinit_;
  
  int unsupported_command_counter;
};

////////////////////////////////////////////////////////////////////////////////

bool SimState::didinit_ = false;

static simclick_node_t *cursimnode = NULL;
static SimState* router_state = NULL;

JNIEnv     *javaEnv = NULL;
jobject    javaException;

// functions for packages

////////////////////////////////////////////////////////////////////////////////

// main

SimState*
SimState::simmain(simclick_node_t *simnode, const char *router_file)
{
  if (!didinit_) {
    click_static_initialize();
    didinit_ = true;
  }

  bool warnings = true;

  SimState* newstate = new SimState();

  // lex
  ErrorHandler *errh = ErrorHandler::default_handler();
  newstate->router = click_read_router(router_file, false, errh, false);
  if (!newstate->router)
    exit(1);

  newstate->router->master()->initialize_ns(simnode);
  if (newstate->router->nelements() == 0 && warnings)
    errh->warning("%s: configuration has no elements", router_file);

  if (errh->nerrors() > 0 || newstate->router->initialize(errh) < 0)
    exit(1);

  newstate->router->activate(errh);
  
  newstate->unsupported_command_counter = 0;
  
  return newstate;
}
////////////////////////////////////////////////////////////////////////////////

//
// XXX
// OK, this bit of code here should work fine as long as your simulator
// isn't multithreaded. If it is, there could be multiple threads stomping
// on each other and potentially causing subtle or unsubtle problems.
//
static void setsimstate(JNIEnv * env, int64_t simtime_ns) {
  cursimnode->curtime.tv_sec  = simtime_ns / 1000000000L;
  cursimnode->curtime.tv_usec = (simtime_ns / 1000L) % 1000000L;

  javaEnv = env;
}

static void setsimstate(simclick_node_t *newstate) {
  cursimnode = newstate;
}

/*static simclick_simstate* getsimstate() {
  return &cursimclickstate;
}*/

extern "C" {

////////////////////////////////////////////////////////////////////////////////

int simclick_gettimeofday(struct timeval* tv) {
  if (cursimnode) {
    *tv = cursimnode->curtime;
    return 0;
  } else {
    tv->tv_sec = 0;
    tv->tv_usec = 0;
    fprintf(stderr,"Hey! Called simclick_gettimeofday without simstate set!\n");
    return -1;
  }
}

  int simclick_click_create(simclick_node_t *simnode, const char* router_file) {
    static bool didinit = false;

    setsimstate(simnode);

    if (!didinit) {
      click_static_initialize();
      didinit = true;
    }

    bool warnings = true;

    // lex
    ErrorHandler *errh = ErrorHandler::default_handler();
    int before = errh->nerrors();

    Router *r = click_read_router(router_file, false, errh, false);
    simnode->clickinfo = r;
    if (!r)
      return errh->fatal("%s: not a valid router", router_file);
    r->master()->initialize_ns(simnode);
    if (r->nelements() == 0 && warnings)
      errh->warning("%s: configuration has no elements", router_file);
    if (errh->nerrors() != before || r->initialize(errh) < 0)
      return errh->fatal("%s: errors prevent router from initializing", router_file);

    r->activate(errh);
    return 0;
  }

/*
 * XXX Need to actually implement this a little more intelligenetly...
 */
 void simclick_click_run(simclick_node_t *simnode) {
   setsimstate(simnode);
  //fprintf(stderr,"Hey! Need to implement simclick_click_run!\n");
  // not right - mostly smoke testing for now...
   Router* r = (Router *) simnode->clickinfo;
   if (r) {
     r->master()->thread(0)->driver();
   } else {
     click_chatter("simclick_click_run: call with null router");
   }
 }

 void simclick_click_kill(simclick_node_t *simnode) {
  //fprintf(stderr,"Hey! Need to implement simclick_click_kill!\n");
   setsimstate(simnode);
   Router *r = (Router *) simnode->clickinfo;
   if (r) {
     delete r;
     simnode->clickinfo = 0;
   } else {
     click_chatter("simclick_click_kill: call with null router");
   }
 }

 int simclick_click_send(simclick_node_t *simnode,
                         int ifid,int type,const unsigned char* data,int len,
                         simclick_simpacketinfo* pinfo)
 {
  setsimstate(simnode);
  int result = 0;
  Router* r = (Router *) simnode->clickinfo;
  if (r) {
    r->sim_incoming_packet(ifid,type,data,len,pinfo);
    r->master()->thread(0)->driver();
  }
  else {
    click_chatter("simclick_click_send: called with null router");
    result = -1;
  }
  return result;
}

char* simclick_click_read_handler(simclick_node_t *simnode,
    const char* elementname,
    const char* handlername,
    SIMCLICK_MEM_ALLOC memalloc,
    void* memparam)
{
  Router *r = (Router *) simnode->clickinfo;
  if (!r) {
    click_chatter("simclick_click_read_handler: call with null router");
    return 0;
  }
  setsimstate(simnode);
  String hdesc = String(elementname) + "." + String(handlername);
  ErrorHandler *errh = ErrorHandler::default_handler();
  int before = errh->nerrors();
  String result = HandlerCall::call_read(hdesc, r->root_element(), errh);
  if (!result && errh->nerrors() != before)
    return 0;
  char *rstr;
  if (memalloc)
    rstr = (char *) memalloc(result.length() + 1, memparam);
  else
    rstr = (char *) malloc(result.length() + 1);
  if (rstr) {
    memcpy(rstr, result.data(), result.length());
    rstr[result.length()] = 0;
  }
  return rstr;
}

int simclick_click_write_handler(simclick_node_t *simnode,
    const char* elementname,
    const char* handlername,
    const char* writestring)
{
  Router *r = (Router *) simnode->clickinfo;
  if (!r) {
    click_chatter("simclick_click_write_handler: call with null router");
    return -3;
  }
  setsimstate(simnode);
  String hdesc = String(elementname) + "." + String(handlername);
  return HandlerCall::call_write(hdesc, String(writestring), r->root_element(), ErrorHandler::default_handler());
}

////////////////////////////////////////////////////////////////////////////////
// Java natives
////////////////////////////////////////////////////////////////////////////////

inline bool is_null(JNIEnv* env, void* ptr)
{
  if (ptr == NULL)
  {
    jclass exc = env->FindClass("java/lang/NullPointerException");
    if (exc != NULL)
      env->ThrowNew(exc, "(in C++ code)");
    return true;
  }
  return false;
}

inline void throw_click_exception(JNIEnv* env, const char* text)
{
  jclass exc = env->FindClass("brn/click/ClickException");
  if (exc != NULL)
    env->ThrowNew(exc, text);
}

inline bool check_handle(JNIEnv* env, jint handle)
{
  if (handle == 0)
  {
    throw_click_exception(env, "Invalid click handle");
    return false;
  }
  return true;
}

class smart_jstring
{
  JNIEnv * _env;
  jstring _js;
  const char* _s;
public:
  smart_jstring(JNIEnv* env, const char* s) : _env(env), _s(NULL) {
    _js = _env->NewStringUTF(s);
  }
  smart_jstring(JNIEnv* env, jstring& js) : _env(env), _js(js), _s(NULL) {}
  ~smart_jstring() {release();}
  const char* c_str() {
    if (NULL == _s && NULL == _js)
      return (NULL);
    if (NULL == _s)
      _s = _env->GetStringUTFChars(_js, NULL);
    return (_s);
  }
  jstring j_str() {
    return (_js);
  }
protected:
  void release() {
    if (_s)
      _env->ReleaseStringUTFChars(_js, _s);
    _s = NULL;
  }
};

////////////////////////////////////////////////////////////////////////////////

JNIEXPORT jlong JNICALL Java_click_runtime_ClickAdapter_click_1create
  (JNIEnv * env, jobject obj, jstring routerFile, jlong startSimulationTime)
{
  cursimnode = new simclick_node_t;

  try {
    if (is_null(env, routerFile))
      return 0;

    setsimstate(env, startSimulationTime);
    smart_jstring router = smart_jstring(env, routerFile);

    // Since we store obj across native function calls, we need a global reference
    // we use weak reference to not confuse the jvmgc
    jweak weak = env->NewWeakGlobalRef(obj);
    if (NULL == weak)
	    return 0;

    SimState* state = SimState::simmain((simclick_node_t*)weak,router.c_str());
    return (jlong) state;
  } 
  catch (jthrowable& exc) {
    env->Throw(exc);
  }
  catch(...) {
    throw_click_exception(env, "internal error (click crashed)");
  }


  return 0;
}

////////////////////////////////////////////////////////////////////////////////

JNIEXPORT void JNICALL Java_click_runtime_ClickAdapter_click_1run
  (JNIEnv *env, jobject, jlong clickHandle, jlong currSimulationTime,
  jbooleanArray jnics_ready)
{
  try {
    if (!check_handle(env, clickHandle))
      return;

    setsimstate(env, currSimulationTime);

    if (NULL != router_state) {
      throw_click_exception(env, "invalid router state");
      return;
    }

    SimState* state = (SimState*)clickHandle;
    router_state = state;
    Router* r = state->router;

    if (NULL == r) {
      throw_click_exception(env, "call with null router");
      return;
    }
    if (NULL == router_state) {
      throw_click_exception(env, "invalid router state");
      return;
    }

    jboolean* nics_ready = env->GetBooleanArrayElements(jnics_ready, NULL);
    for (int i = 0; i < env->GetArrayLength(jnics_ready); i++) {
      state->_nics_ready[i] = nics_ready[i];
    }

    r->master()->thread(0)->driver();
    router_state = NULL;
  } 
  catch (jthrowable& exc) {
    env->Throw(exc);
  }
  catch(...) {
    throw_click_exception(env, "internal error (click crashed)");
  }
}

////////////////////////////////////////////////////////////////////////////////

JNIEXPORT void JNICALL Java_click_runtime_ClickAdapter_click_1kill
  (JNIEnv *env, jobject, jlong clickHandle, jlong currSimulationTime)
{
  try {
    if (!check_handle(env, clickHandle))
      return;

    setsimstate(env, currSimulationTime);

    SimState* state = (SimState*)clickHandle;
    Router* r = state->router;
    if (NULL == r) {
      throw_click_exception(env, "call with null router");
      return;
    }

    // Release weak reference
    if (NULL != r->master()) {
    jweak wobj = (jweak)r->master()->simnode();

      env->DeleteWeakGlobalRef(wobj);
    }

    state->router = NULL;
    delete r;
  } 
  catch (jthrowable& exc) {
    env->Throw(exc);
  }
  catch(...) {
    throw_click_exception(env, "internal error (click crashed)");
  }
}

////////////////////////////////////////////////////////////////////////////////

JNIEXPORT jint JNICALL Java_click_runtime_ClickAdapter_click_1send
  (JNIEnv *env, jobject, jlong clickHandle, jlong currSimulationTime,
  jint jifid, jint jtype, jbyteArray jdata, jint id, jint fid, jint simtype,
  jboolean txfeedback, jbooleanArray jnics_ready)
{
  int result = 0;
  try {
    if (!check_handle(env, clickHandle))
      return -1;

    setsimstate(env, currSimulationTime);

    if (NULL != router_state) {
      throw_click_exception(env, "invalid router state");
      return -1;
    }

    SimState* state = (SimState*)clickHandle;
    router_state = state;
    Router* r = state->router;

    if (NULL == r) {
      throw_click_exception(env, "call with null router");
      return -1;
    }
    if (NULL == router_state) {
      throw_click_exception(env, "invalid router state");
      return -1;
    }
    jboolean* nics_ready = env->GetBooleanArrayElements(jnics_ready, NULL);
    for (int i = 0; i < env->GetArrayLength(jnics_ready); i++) {
      state->_nics_ready[i] = nics_ready[i];
    }

    // TODO we do not use sim packet info
    simclick_simpacketinfo info;
    info.id = id;
    info.fid = fid;
    info.simtype = simtype;

    int len = env->GetArrayLength(jdata);
    jbyte* data = env->GetByteArrayElements(jdata, NULL);

    if (SIMCLICK_PTYPE_WIFI_EXTRA == jtype) {
      click_wifi_extra* eh = (click_wifi_extra*)data;
      if (txfeedback) {
        assert(WIFI_EXTRA_TX == (eh->flags & WIFI_EXTRA_TX));
	info.txfeedback = 1;
      } else {
        assert(0 == (eh->flags & WIFI_EXTRA_TX));
	info.txfeedback = 0;
      }
    }

    // Click will copy the data
    r->sim_incoming_packet(jifid, jtype, (unsigned char*)data, len, &info);

    env->ReleaseByteArrayElements(jdata, data, JNI_ABORT);

    // sim_incoming_packet is not atomic, nested invocations are possible 
    // so we MUST set the simulation time again since it is stored in a global var
    setsimstate(env, currSimulationTime);
    r->master()->thread(0)->driver();
    router_state = NULL;
} 
  catch (jthrowable& exc) {
    env->Throw(exc);
  }
  catch(...) {
    throw_click_exception(env, "internal error (click crashed)");
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////

JNIEXPORT jstring JNICALL Java_click_runtime_ClickAdapter_click_1read_1handler
  (JNIEnv *env, jobject, jlong clickHandle, jlong currSimulationTime,
  jstring elemName, jstring handlerName)
{
  try {
    if (!check_handle(env, clickHandle) || is_null(env, handlerName))
      return 0;

    setsimstate(env, currSimulationTime);

    SimState* state = (SimState*)clickHandle;
    Router* r = state->router;
    if (NULL == r) {
      throw_click_exception(env, "call with null router");
      return 0;
    }

    smart_jstring element = smart_jstring(env, elemName);
    smart_jstring handler = smart_jstring(env, handlerName);

    String hdesc = String(element.c_str()) + "." + String(handler.c_str());
    ErrorHandler *errh = ErrorHandler::default_handler();

    int before = errh->nerrors();
    String result = HandlerCall::call_read(hdesc, r->root_element(), errh);

    if (!result && errh->nerrors() != before)
      return 0;

    return smart_jstring(env, result.c_str()).j_str();
  }
  catch (jthrowable& exc) {
    env->Throw(exc);
  }
  catch(...) {
    throw_click_exception(env, "internal error (click crashed)");
  }

  return (0);
}

////////////////////////////////////////////////////////////////////////////////

JNIEXPORT jint JNICALL Java_click_runtime_ClickAdapter_click_1write_1handler
  (JNIEnv *env, jobject, jlong clickHandle, jlong currSimulationTime,
  jstring elemName, jstring handlerName, jstring jvalue)
{
  int ret = 0;

  try {
    if (!check_handle(env, clickHandle) 
      || is_null(env, elemName) || is_null(env, handlerName) || is_null(env, jvalue))
      return -3;

    setsimstate(env, currSimulationTime);

    SimState* state = (SimState*)clickHandle;
    Router* r = state->router;
    if (NULL == r) {
      throw_click_exception(env, "call with null router");
      return -3;
    }

    smart_jstring element = smart_jstring(env, elemName);
    smart_jstring handler = smart_jstring(env, handlerName);
    smart_jstring value = smart_jstring(env, jvalue);

    String hdesc = String(element.c_str()) + "." + String(handler.c_str());
    ret = HandlerCall::call_write( hdesc, 
                                  String(value.c_str()), 
                                  r->root_element(), 
                                  ErrorHandler::default_handler());
  }
  catch (jthrowable& exc) {
    env->Throw(exc);
  }
  catch(...) {
    throw_click_exception(env, "internal error (click crashed)");
  }

  return (ret);
}

////////////////////////////////////////////////////////////////////////////////
// Call stubs into the simulator
////////////////////////////////////////////////////////////////////////////////

int simclick_sim_ifid_from_name(simclick_node_t *simnode,const char* ifname)
{
  // TODO check weak reference? or produces the jvm a meaningfull exception?
  brn::click::ClickAdapter click_adapter((jobject) simnode);

  return click_adapter.sim_ifid_from_name(ifname);
}

////////////////////////////////////////////////////////////////////////////////

void simclick_sim_ipaddr_from_name(simclick_node_t *simnode,const char* ifname,
           char* buf,int len)
{
  brn::click::ClickAdapter click_adapter((jobject) simnode);

  const char* result = click_adapter.sim_ipaddr_from_name(ifname);
  if (result) {
    strncpy(buf, result, len);
    delete[] result;
  }
  else {
    buf[0] = '\0';
  }
}

////////////////////////////////////////////////////////////////////////////////

void simclick_sim_macaddr_from_name(simclick_node_t *simnode,const char* ifname,
            char* buf,int len)
{
  brn::click::ClickAdapter click_adapter((jobject) simnode);

  const char* result = click_adapter.sim_macaddr_from_name(ifname);
  if (result) {
    strncpy(buf, result, len);
    delete[] result;
  }
  else {
    buf = '\0';
  }
}

////////////////////////////////////////////////////////////////////////////////

int simclick_sim_send(simclick_node_t *simnode,
          int ifid,int type, const unsigned char* data,
          int len,simclick_simpacketinfo* info)
{
  brn::click::ClickAdapter click_adapter((jobject) simnode);

  if (NULL == router_state || NULL == router_state->_nics_ready)
    throw_click_exception(javaEnv, "route state null");

  router_state->_nics_ready[ifid] = false;

  JavaByteArray jdata((const char*)data, len);
  return click_adapter.sim_send_to_if((long)simnode, ifid, type, &jdata, info->id, info->fid, info->simtype);
}

////////////////////////////////////////////////////////////////////////////////

int simclick_sim_schedule(simclick_node_t *simnode,
        struct timeval* when)
{
  brn::click::ClickAdapter click_adapter((jobject) simnode);

  int64_t lwhen = (int64_t)(when->tv_sec) * 1000000000L
    + (int64_t)(when->tv_usec) * 1000L;

  // TODO prevent driver from scheduling the same timer twice
  if (router_state->last_scheduled == lwhen)
    return 0; // already sheduled

  router_state->last_scheduled = lwhen;
  return click_adapter.sim_schedule((long)simnode, lwhen);
}

////////////////////////////////////////////////////////////////////////////////

void simclick_sim_get_node_name(simclick_node_t *simnode,char* buf,int len)
{
  brn::click::ClickAdapter click_adapter((jobject) simnode);

  const char* result = click_adapter.sim_get_node_name();
  if (result) {
    strncpy(buf, result, len);
    delete[] result;
  }
  else {
    buf = '\0';
  }
}

////////////////////////////////////////////////////////////////////////////////

int simclick_sim_if_ready(simclick_node_t */*simnode*/, int ifid)
{
  //brn::click::ClickAdapter click_adapter((jobject) siminst);
  //return click_adapter.sim_if_ready((int)clickinst, ifid);

  return router_state->_nics_ready[ifid];
}

////////////////////////////////////////////////////////////////////////////////

int simclick_sim_trace(simclick_node_t *simnode, const char* event)
{
  brn::click::ClickAdapter click_adapter((jobject) simnode);

  return click_adapter.sim_trace((long)simnode, event);
}

////////////////////////////////////////////////////////////////////////////////

int simclick_sim_get_node_id(simclick_node_t *simnode)
{
  brn::click::ClickAdapter click_adapter((jobject) simnode);

  return click_adapter.sim_get_node_id((long)simnode);
}

////////////////////////////////////////////////////////////////////////////////

int simclick_sim_get_next_pkt_id(simclick_node_t *simnode)
{
  brn::click::ClickAdapter click_adapter((jobject) simnode);

  return click_adapter.sim_get_next_pkt_id((long)simnode);
}

////////////////////////////////////////////////////////////////////////////////
// functions for packages

int simclick_sim_command(simclick_node_t *simnode, int cmd, ...)
{
  va_list val;
  va_start(val, cmd);

  int r = -1;

  if ( simnode == NULL ) printf("Node is NULL\n");
/*
#define SIMCLICK_VERSION    0  // none
#define SIMCLICK_SUPPORTS   1  // int call
#define SIMCLICK_IFID_FROM_NAME   2  // const char *ifname
#define SIMCLICK_IPADDR_FROM_NAME 3  // const char *ifname, char *buf, int len
#define SIMCLICK_MACADDR_FROM_NAME  4  // const char *ifname, char *buf, int len
#define SIMCLICK_SCHEDULE   5  // struct timeval *when
#define SIMCLICK_GET_NODE_NAME    6  // char *buf, int len
#define SIMCLICK_IF_READY   7  // int ifid
#define SIMCLICK_TRACE      8  // const char *event
#define SIMCLICK_GET_NODE_ID    9  // none
#define SIMCLICK_GET_NEXT_PKT_ID  10 // none
#define SIMCLICK_CHANGE_CHANNEL   11 // int ifid, int channelid
*/
//  printf("command: %d\n",cmd);
  switch (cmd) {
    case SIMCLICK_MACADDR_FROM_NAME:  //4
    {
      const char *ifname = va_arg(val, const char *);
      char *buf = va_arg(val, char *);
      int len = va_arg(val, int);
      brn::click::ClickAdapter click_adapter((jobject) simnode);

//      printf("%s\n",ifname);

      const char* result = click_adapter.sim_macaddr_from_name(ifname);
//      printf("cl: %s\n",result);
      if (result) {
        strncpy(buf, result, len);
        delete[] result;
        result = NULL;
      }
      else {
        buf = '\0';
      }

      r = 0;

      break;
    }
    case SIMCLICK_IPADDR_FROM_NAME:   //3
    {
      const char *ifname = va_arg(val, const char *);
      char *buf = va_arg(val, char *);
      int len = va_arg(val, int);
      brn::click::ClickAdapter click_adapter((jobject) simnode);
//      printf("%s\n",ifname);
      const char* result = click_adapter.sim_ipaddr_from_name(ifname);
      if (result) {
        strncpy(buf, result, len);
        delete[] result;
      }
      else {
        buf[0] = '\0';
      }
      break;
    }
    case SIMCLICK_IFID_FROM_NAME:    //2
    {
      const char *ifname = va_arg(val, const char *);
      //char *buf = va_arg(val, char *);  //not used
      //int len = va_arg(val, int);  //not used
      brn::click::ClickAdapter click_adapter((jobject) simnode);
      r = click_adapter.sim_ifid_from_name(ifname);
      break;
    }
    case SIMCLICK_SCHEDULE:    //5
    {
      const struct timeval *when = va_arg(val, const struct timeval *);
      brn::click::ClickAdapter click_adapter((jobject) simnode);

      int64_t lwhen = (int64_t)(when->tv_sec) * 1000000000L
                    + (int64_t)(when->tv_usec) * 1000L;

  // TODO prevent driver from scheduling the same timer twice
      if (router_state->last_scheduled == lwhen)
          return 0; // already sheduled

      router_state->last_scheduled = lwhen;
      return click_adapter.sim_schedule((long)simnode, lwhen);
    }
    case SIMCLICK_IF_READY:    //7
    {
      int ifid = va_arg(val, int);
      return router_state->_nics_ready[ifid];
    }
    default:
    {
      if ( router_state->unsupported_command_counter < 5 ) {
        printf("unsupported command: %d\n",cmd);
	router_state->unsupported_command_counter++;
      }
      r = -1;
    }
  }
  va_end(val);
  return r;
}
////////////////////////////////////////////////////////////////////////////////
}
