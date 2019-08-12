// server.cpp : 定义控制台应用程序的入口点。
//
// clang-format off
#include "stdafx.h"
#include "server.h"
// clang-format on
#include <assert.h>
#include <exception>
#include <iomanip>
#include <iostream>
#include <process.h>
#include <sstream>

void *io_context::param() { return parent()->param(); }
void io_context::param(void *param) { parent()->param(param); }
server::server(const std::string &local_address,
               const unsigned short local_port, cb_accept_func cb_a,
               cb_io_func cb_r, cb_io_func cb_w)
    : local_address_(local_address), local_port_(local_port), h_iocp_(NULL),
      accept_ex_(NULL), connect_ex_(NULL), disconnect_ex_(NULL),
      cb_accept(cb_a), cb_recv(cb_r), cb_send(cb_w) {

  SYSTEM_INFO system_info;
  GetSystemInfo(&system_info);
  work_thread_num_ = system_info.dwNumberOfProcessors;
  WSADATA wa;
  assert(0 == WSAStartup(MAKEWORD(2, 2), &wa));
}

server::~server() {
  stop();
  WSACleanup();
}

void server::init_net_func(SOCKET s) {
  GUID guidAcceptEx = WSAID_ACCEPTEX;
  DWORD bytes = 0;
  if (WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidAcceptEx,
               sizeof(GUID), &accept_ex_, sizeof(LPFN_ACCEPTEX), &bytes, NULL,
               NULL) == SOCKET_ERROR) {
    throw std::exception("get acceptex interface error", GetLastError());
  }

  GUID guidConnectEx = WSAID_CONNECTEX;
  if (::WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidConnectEx,
                 sizeof(GUID), &connect_ex_, sizeof(LPFN_CONNECTEX), &bytes,
                 NULL, NULL) == SOCKET_ERROR) {
    throw std::exception("get connectex interface error", GetLastError());
  }

  GUID guidDisconnectEx = WSAID_DISCONNECTEX;
  if (::WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidDisconnectEx,
                 sizeof(GUID), &disconnect_ex_, sizeof(LPFN_DISCONNECTEX),
                 &bytes, NULL, NULL) == SOCKET_ERROR) {
    throw std::exception("get disconnectex interface error", GetLastError());
  }
}

void server::post_accept() {
  std::tr1::shared_ptr<session> ptr =
      std::tr1::shared_ptr<session>(new session());
  assert(ptr);
  assert(ptr->recv_context() != NULL);
  assert(ptr->recv_context()->size() > (sizeof(sockaddr_in) + 16) * 2);
  DWORD len = 0;
  if (!accept_ex_(sk_listen_, ptr->socket(), ptr->recv_context()->data(), 0,
                  sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, &len,
                  ptr->recv_context())) {
    if (WSA_IO_PENDING != WSAGetLastError()) {
      throw std::exception("acceptex error", GetLastError());
    }
  }
  sessions_.push_back(ptr);
}

void server::post_recv(io_context *ctx) {
  assert(ctx != NULL);
  assert(ctx->parent() != NULL);
  DWORD recv_len = 0;
  DWORD flags = 0;
  if (WSARecv(ctx->parent()->socket(), &ctx->parent()->recv_context()->buffer(),
              1, &recv_len, &flags, ctx, NULL) == SOCKET_ERROR) {
    if (WSAGetLastError() != WSA_IO_PENDING) {
      throw std::exception("recv error", GetLastError());
    }
  }
}

void server::post_send(io_context *ctx, unsigned char *data, const int len) {
  assert(ctx != NULL);
  assert(ctx->parent() != NULL);
  assert(data != NULL);
  assert(len > 0);
  assert(ctx->parent()->send_context()->size() > len);

  ctx->parent()->send_context()->buffer().len = len;
  DWORD send_len = 0;
  DWORD flags = 0;
  memcpy(ctx->parent()->send_context()->data(), data, len);
  if (WSASend(ctx->parent()->socket(), &ctx->parent()->send_context()->buffer(),
              1, &send_len, flags, ctx->parent()->send_context(),
              NULL) == SOCKET_ERROR) {
    if (WSAGetLastError() != WSA_IO_PENDING) {
      throw std::exception("send error", GetLastError());
    }
  }
}

void server::on_accept(io_context *ctx, const int len) {
  assert(ctx != NULL);
  assert(ctx->parent() != NULL);
  post_accept();
  if (len > 0) {
    on_recv(ctx, len);
  }
  if (!CreateIoCompletionPort((HANDLE)ctx->parent()->socket(), h_iocp_, NULL,
                              0)) {
    throw std::exception("bind listen to iocp", GetLastError());
  }
  if (cb_accept) {
    cb_accept(shared_from_this(), ctx);
  }
}

void server::on_recv(io_context *ctx, const int len) {
  assert(ctx != NULL);
  assert(ctx->parent() != NULL);
  ctx->parent()->up_recv_len(len);
  if (cb_recv) {
    cb_recv(shared_from_this(), ctx, len);
  }
}

void server::on_send(io_context *ctx, const int len) {
  assert(ctx != NULL);
  assert(ctx->parent() != NULL);
  ctx->parent()->up_send_len(len);
  if (cb_send) {
    cb_send(shared_from_this(), ctx, len);
  }
}

void server::start() {
  assert(work_thread_num_ > 0);
  assert(local_port_ > 0 && local_port_ < 65535);
  assert(!local_address_.empty());

  sk_listen_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  assert(sk_listen_ != INVALID_SOCKET);
  sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(local_address_.c_str());
  addr.sin_port = htons(local_port_);

  if (0 != bind(sk_listen_, (struct sockaddr *)&addr, sizeof(sockaddr))) {
    throw std::exception("bind error", GetLastError());
  }

  h_iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL,
                                   work_thread_num_);
  if (!h_iocp_) {
    throw std::exception("create iocp error", GetLastError());
  }
  init_net_func(sk_listen_);
  listen(sk_listen_, 0);

  handle_work_threads_.resize(work_thread_num_);
  for (int i = 0; i < work_thread_num_; i++) {
    post_accept();
    handle_work_threads_[i] = reinterpret_cast<HANDLE>(
        _beginthreadex(NULL, 0, work_thread, this, 0, NULL));
  }

  if (!CreateIoCompletionPort((HANDLE)sk_listen_, h_iocp_, CPK_ACCEPT, 0)) {
    throw std::exception("bind listen to iocp", GetLastError());
  }
}

void server::stop() {
  if (!h_iocp_) {
    return;
  }
  for (int i = 0; i < static_cast<int>(handle_work_threads_.size()); i++) {
    PostQueuedCompletionStatus(h_iocp_, 0, CPK_CLOSE, 0);
  }

  CloseHandle(h_iocp_);
  h_iocp_ = NULL;
}

unsigned int WINAPI server::work_thread(void *param) {
  server *s = static_cast<server *>(param);
  if (!s) {
    throw std::exception("server param error");
  }

  LPOVERLAPPED overlapped = NULL;
  ULONG_PTR cl_key = 0;
  DWORD recv_bytes = 0;
  while (true) {
    BOOL ret = GetQueuedCompletionStatus(s->h_iocp_, &recv_bytes, &cl_key,
                                         &overlapped, INFINITE);
    if (!ret && ERROR_NETNAME_DELETED == GetLastError()) {
      break;
    }
    io_context *ctx = static_cast<io_context *>(overlapped);
    if (CPK_ACCEPT == cl_key) {
      assert(ctx != NULL);
      s->on_accept(ctx, recv_bytes);
      continue;
    } else if (CPK_CLOSE == cl_key) {
      break;
    }

    assert(ctx != NULL);
    if (0 == recv_bytes) {
      closesocket(ctx->parent()->socket());
      s->sessions_.remove(ctx->parent()->shared_from_this());
    }
    assert(ctx != NULL);
    if (ctx == ctx->parent()->recv_context()) {
      s->on_recv(ctx, recv_bytes);
    } else if (ctx == ctx->parent()->send_context()) {
      s->on_send(ctx, recv_bytes);
    } else {
      assert(!"unknow context!");
    }
  }
  return 0;
}
