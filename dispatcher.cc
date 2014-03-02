
#include "dispatcher.h"
#include <assert.h>
#include <errno.h>

namespace v {

static char err[ANET_ERR_LEN];

shared_ptr<Dispatcher> Dispatcher::Create(
    shared_ptr<aeEventLoop> loop,
    int fd,
    int mask,
    long long timeout) {
  shared_ptr<Dispatcher> dispatcher(new Dispatcher, Destroy);
  if (!dispatcher)
    return nullptr;

  dispatcher->loop_ = loop;
  dispatcher->state_ = SS_CLOSED;
  dispatcher->fd_ = fd;
  dispatcher->mask_ = mask;
  dispatcher->timeout_ = timeout;

  if (dispatcher->timeout_ != AE_NOMORE) {
    dispatcher->id_ = aeCreateTimeEvent(loop.get(),
                                        dispatcher->timeout_,
                                        TimeProc,
                                        dispatcher,
                                        nullptr);

    if (dispatcher->id_ == AE_ERR)
      return nullptr;
  }

  if (dispatcher->fd_ >= aeGetSetSize(loop.get()) &&
      aeResizeSetSize(loop.get(), dispatcher->fd_ + 1) == AE_ERR)
    return nullptr;

  if (dispatcher->fd_ != AE_ERR &&
      aeCreateFileEvent(
          loop.get(),
          dispatcher->fd_,
          dispatcher->mask_,
          FileProc,
          dispatcher) == AE_ERR)
    return nullptr;

  return dispatcher;
}

shared_ptr<Dispatcher> Dispatcher::CreateAcceptor(
    shared_ptr<aeEventLoop> loop,
    const char *addr,
    int port,
    long long timeout) {
  shared_ptr<Dispatcher> dispatcher(new Dispatcher, Destroy);
  if (!dispatcher)
    return nullptr;

  dispatcher->loop_ = loop;
  dispatcher->state_ = SS_LISTEN;
  dispatcher->fd_ = anetTcpServer(err, port, (char *)addr);
  dispatcher->mask_ = AE_READABLE;
  dispatcher->timeout_ = timeout;

  if (dispatcher->fd_ == ANET_ERR)
    return nullptr;

  if (dispatcher->fd_ >= aeGetSetSize(loop.get()) &&
      aeResizeSetSize(loop.get(), dispatcher->fd_ + 1) == AE_ERR)
    return nullptr;

  if (dispatcher->timeout_ != AE_NOMORE) {
    dispatcher->id_ = aeCreateTimeEvent(loop.get(),
                                        dispatcher->timeout_,
                                        TimeProc,
                                        dispatcher,
                                        nullptr);

    if (dispatcher->id_ == AE_ERR)
      return nullptr;
  }

  if (aeCreateFileEvent(
          loop.get(),
          dispatcher->fd_,
          dispatcher->mask_,
          FileProc,
          dispatcher) == AE_ERR)
    return nullptr;

  return dispatcher;
}

std::shared_ptr<Dispatcher> Dispatcher::CreateConnector(
    shared_ptr<aeEventLoop> loop,
    const char *addr,
    int port,
    long long timeout) {
  shared_ptr<Dispatcher> dispatcher(new Dispatcher, Destroy);
  if (!dispatcher)
    return nullptr;

  dispatcher->loop_ = loop;
  dispatcher->state_ = SS_CLOSED;
  dispatcher->fd_ = anetTcpNonBlockConnect(err, (char *)addr, port);
  dispatcher->mask_ = AE_READABLE | AE_WRITABLE;
  dispatcher->timeout_ = timeout;

  if (dispatcher->fd_ == ANET_ERR)
    return nullptr;

  if (dispatcher->fd_ >= aeGetSetSize(loop.get()) &&
      aeResizeSetSize(loop.get(), dispatcher->fd_ + 1) == AE_ERR)
    return nullptr;

  if (errno == EINPROGRESS)
    dispatcher->state_ = SS_CONNECTING;

  if (dispatcher->timeout_ != AE_NOMORE) {
    dispatcher->id_ = aeCreateTimeEvent(loop.get(),
                                        dispatcher->timeout_,
                                        TimeProc,
                                        dispatcher,
                                        nullptr);

    if (dispatcher->id_ == AE_ERR)
      return nullptr;
  }

  if (aeCreateFileEvent(
          loop.get(),
          dispatcher->fd_,
          dispatcher->mask_,
          FileProc,
          dispatcher) == AE_ERR)
    return nullptr;

  return dispatcher;
}

bool Dispatcher::SetMask(int mask) {
  if (mask == mask_)
    return true;

  auto loop = loop_.lock();
  if (loop && fd_ != AE_ERR &&
      aeCreateFileEvent(
          loop.get(),
          fd_,
          mask,
          FileProc,
          shared_from_this()) == AE_OK) {
    aeDeleteFileEvent(loop.get(), fd_, mask_);
    mask_ = mask;
    return true;; 
  }

  return false;;
}

bool Dispatcher::SetTimeout(long long timeout) {
  auto loop = loop_.lock();
  if (!loop)
    return false;

  if (timeout != AE_NOMORE) {
    auto id = aeCreateTimeEvent(loop.get(),
                                timeout,
                                TimeProc,
                                shared_from_this(),
                                nullptr);
    if (id == AE_ERR)
      return false;

    if (id_ != AE_ERR)
      aeDeleteTimeEvent(loop.get(), id_);  

    id_ = id;
    timeout_ = timeout;
  }

  return true;
}

void Dispatcher::FileProc(aeEventLoop *loop, int fd, aeData data, int mask) {
  std::shared_ptr<Dispatcher> dispatcher = 
    std::static_pointer_cast<Dispatcher>(data.lock());

  if (!dispatcher)
    return;

  SocketState &state = dispatcher->state_;

  if (state == SS_CLOSED && anetPeerToString(fd, nullptr, 0, nullptr) == 0) {
    state = SS_CONNECTED;
    if (dispatcher->on_connected)
      dispatcher->on_connected(fd); 
  }

  switch (mask) {
    case AE_READABLE: {
      if (dispatcher->readable && dispatcher->readable()) {
        int s;
        switch (state) {
          case SS_LISTEN:
            s = anetTcpAccept(err, fd, nullptr, 0, nullptr);
            if (s != ANET_ERR && dispatcher->on_accepted)
              dispatcher->on_accepted(s);
            break;
          case SS_CONNECTED:
            if (dispatcher->read)
              dispatcher->read(fd);
            break;
          default:
            assert(false);
        }
      }

      break;
    }
    case AE_WRITABLE: {
      if (dispatcher->writable && dispatcher->writable()) {
        switch (state) {
          case SS_CONNECTING:
            state = SS_CONNECTED;
            if (dispatcher->on_connected)
              dispatcher->on_connected(fd);
            // go on
          case SS_CONNECTED:
            if (dispatcher->write)
              dispatcher->write(fd);
            break;
          default:
            assert(false);
        }
      }

      break; 
    }
  }
}

int Dispatcher::TimeProc(aeEventLoop *loop, long long id, aeData data) {
  std::shared_ptr<Dispatcher> dispatcher = 
    std::static_pointer_cast<Dispatcher>(data.lock());

  if (!dispatcher)
    return AE_NOMORE;

  if (dispatcher->on_timedout)
    dispatcher->timeout_ = dispatcher->on_timedout(
      dispatcher->fd_, dispatcher->timeout_);
  else
    dispatcher->timeout_ = AE_NOMORE;

  if (dispatcher->timeout_ == AE_NOMORE)
    dispatcher->id_ = AE_ERR;

  return dispatcher->timeout_;
}

void Dispatcher::Destroy(Dispatcher *self) {
  auto loop = self->loop_.lock();
  if (loop) {
    if (self->id_ != AE_ERR)
      aeDeleteTimeEvent(loop.get(), self->id_);
    if (self->fd_ != ANET_ERR) {
      aeDeleteFileEvent(loop.get(), self->fd_, self->mask_);
      close(self->fd_);
    }
  }
  if (self->on_closed)
    self->on_closed(self->fd_);

  delete self;
}

}   // namespace dis
