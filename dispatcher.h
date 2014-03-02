

#ifndef _DISPATCHER_H
#define _DISPATCHER_H

#include "ae.h"
#include "anet.h"
#include "zmalloc.h"
#include <unistd.h>
#include <memory>
#include <functional>

namespace v {

using std::function;
using std::weak_ptr;
using std::shared_ptr;
using std::enable_shared_from_this;

class Dispatcher : public enable_shared_from_this<Dispatcher> {
 public:
  // constructors
  static shared_ptr<Dispatcher> Create(shared_ptr<aeEventLoop> loop,
                                       int fd,
                                       int mask,
                                       long long timeout);

  static shared_ptr<Dispatcher> CreateAcceptor(shared_ptr<aeEventLoop> loop,
                                               const char *addr,
                                               int port,
                                               long long timeout);

  static shared_ptr<Dispatcher> CreateConnector(shared_ptr<aeEventLoop> loop,
                                                const char *addr,
                                                int port,
                                                long long timeout);

  // methods
  bool SetMask(int mask);
  bool SetTimeout(long long timeout);
  bool SetSocketOption(int option);

  // conditions
  function<bool()> readable = []() { return true; };
  function<bool()> writable = []() { return true; };

  // operates
  function<void(int fd)> read;
  function<void(int fd)> write;

  // events
  function<void(int fd)> on_accepted = [](int fd) { close(fd); };
  function<void(int fd)> on_connected;
  function<int(int fd, long long timeout)> on_timedout;
  function<void(int fd)> on_closed;

 private:
  enum SocketState {
    SS_CLOSED,
    SS_LISTEN,
    SS_CONNECTING,
    SS_CONNECTED,
  };

  static void FileProc(aeEventLoop *loop, int fd, aeData data, int mask);
  static int TimeProc(aeEventLoop *loop, long long id, aeData data);
  static void Destroy(Dispatcher *self);

  static void * operator new(size_t n) {
    return zmalloc(n);
  }

  static void operator delete(void *p) {
    zfree(p); 
  }

  Dispatcher() = default;
  Dispatcher(const Dispatcher &) = delete;
  Dispatcher & operator=(const Dispatcher &) = delete;

  weak_ptr<aeEventLoop> loop_;
  SocketState state_ = SS_CLOSED;
  int fd_ = ANET_ERR;
  int mask_ = AE_NONE;
  int id_ = AE_ERR;
  long long timeout_ = AE_NOMORE;
};

}   // namespace dis

#endif  // !_DISPATCHER_H
