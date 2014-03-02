// echo.cc

#include <err.h>
#include <stdlib.h>
#include <signal.h>
#include <unordered_map>
#include "dispatcher.h"

using namespace std;

namespace {

inline shared_ptr<aeEventLoop> GetLoop() {
  static auto loop = shared_ptr<aeEventLoop>(
      aeCreateEventLoop(INIT_SETSIZE), aeDeleteEventLoop);
  return loop;
}

void sighandler(int signo) {
  switch (signo) {
    case SIGINT:
      aeStop(GetLoop().get());
      break;
  }
}

}

int main(int argc, char *argv[]) {
  if (argc != 3 && argc != 4)
    errx(1, "usage: %s <ip> <port> [timeout]", argv[0]);

  signal(SIGINT, sighandler);

  auto ip = argv[1];
  auto port = atoi(argv[2]);
  auto timeout = argc == 4 ? atoi(argv[3]) : AE_NOMORE;
  auto service = v::Dispatcher::CreateAcceptor(GetLoop(), ip, port, timeout);

  unordered_map<int, decltype(service)> pool;

  service->on_accepted = [&](int fd) {
    char peer_ip[129];
    int peer_port;

    auto &clients = pool;
    auto s = anetPeerToString(fd, peer_ip, sizeof(peer_ip), &peer_port);
    if (s != -1) {
      warnx("-- <%d>-[%s:%d] incoming", fd, peer_ip, peer_port);

      auto client = v::Dispatcher::Create(GetLoop(), fd, AE_READABLE, timeout);
      if (!client) {
        warnx("-- <%d>-[%s:%d] error", fd, ip, port);
        return;
      }

      clients[fd] = client;

      // blocking I/O
      client->read = [&](int fd) {
        char buf[BUFSIZ];
        auto n = read(fd, buf, sizeof(buf));
        if (n <= 0) {
          if (n < 0)
            warn("-- <%d> read error", fd);
          else
            warnx("-- <%d> EOF", fd);

          clients[fd] = nullptr;
        } else if (n > 0) {
          if (write(fd, buf, n) < 0) {
            warn("-- <%d> write error", fd);
            clients[fd] = nullptr;
          }
        }
      };

      client->on_timedout = [&](int fd, long long timeout) {
        warnx("-- <%d> i/o timedout", fd);
        clients[fd] = nullptr;
        return AE_NOMORE;
      };

      client->on_closed = [&](int fd) {
        warnx("-- <%d> disconnected", fd);
      };
    }
  };

  aeMain(GetLoop().get());
}
