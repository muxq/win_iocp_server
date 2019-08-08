// clang-format off
#include "stdafx.h"
#include "server.h"
// clang-format on
#include <assert.h>
#include <sstream>
#include <iostream>
#include "hexdump.h"

#pragma comment(lib, "ws2_32.lib")
static const char *default_address_ = "127.0.0.1";
static const int default_port_ = 12345;

void on_recv(std::tr1::shared_ptr<server> s, io_context *ctx, const int len) {
  assert(s);
  assert(ctx);
  Hexdump(ctx->parent()->recv_context()->data(), len);
  s->post_send(ctx, reinterpret_cast<unsigned char *>("hello world!"),
               sizeof("hello world!") - 1);
}
void on_send(std::tr1::shared_ptr<server> s, io_context *ctx, const int len) {
  assert(s);
  assert(ctx);
  if (sizeof("hello world!") - 1 == ctx->parent()->send_len()) {
    closesocket(ctx->parent()->socket());
  } else {
    // send left data
    s->post_send(ctx, reinterpret_cast<unsigned char *>("hello world!") + len,
                 sizeof("hello world!") - 1 - len);
  }
}

void on_accept(std::tr1::shared_ptr<server> s, io_context *ctx) {
  assert(s);
  assert(ctx);
  s->post_recv(ctx);
}

void usage() {
  std::ostringstream os;
  os << "usage:\n";
  os << " -a local listen address default is 127.0.0.1\n";
  os << " -p local listen port default is 12345\n";
  os << "example: \n";
  os << " server.exe -a 0.0.0.0 -p 8080";
  std::cout << os.str();
}
int _tmain(int argc, _TCHAR *argv[]) {
  const char *address_ = default_address_;
  int port_ = default_port_;

  for (int n = 1; n < argc; n++) {
    if (0 == stricmp(argv[n], "-p")) {
      (n == (argc + 1)) ? port_ : (port_ = atoi(argv[n + 1]));
      n++;
    } else if (0 == stricmp(argv[n], "-a")) {
      (n == (argc + 1)) ? address_ : (address_ = argv[n + 1]);
      n++;
    } else {
      usage();
      exit(-1);
    }
  }
  assert(port_ > 0 && port_ < 65535);
  assert(address_ != NULL);
  using namespace std::tr1::placeholders;
  std::tr1::shared_ptr<server> server_ptr = std::tr1::shared_ptr<server>(
      new server(address_, port_, std::tr1::bind(on_accept, _1, _2),
                 std::tr1::bind(on_recv, _1, _2, _3),
                 std::tr1::bind(on_send, _1, _2, _3)));
  server_ptr->start();
  getchar();
  server_ptr->stop();
  return 0;
}
