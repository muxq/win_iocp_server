#ifndef IOCP_SERVER_H
#define IOCP_SERVER_H
#include "safe_stack.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

//借用指针地址
enum CPK_TYPE {
  CPK_ACCEPT = 1000, //接收请求的完成标识
  CPK_CLOSE,
};

//上下文类型
enum CTX_TYPE {
  RECV_CONTEXT = 0, //用于接收的上下文
  SEND_CONTEXT,     //用于发送的上下文
};

class server;
class session;
class io_context;

typedef std::tr1::function<void(std::tr1::shared_ptr<server>, io_context *,
                                const int)>
    cb_io_func;
typedef std::tr1::function<void(std::tr1::shared_ptr<server>, io_context *)>
    cb_accept_func;

class io_context : public OVERLAPPED {
public:
  explicit io_context(session *parent, const int size = 4096)
      : parent_(parent), buf_(size) {
    memset(static_cast<OVERLAPPED *>(this), 0, sizeof(OVERLAPPED));
    w_buf_.buf = reinterpret_cast<char *>(&buf_[0]);
    w_buf_.len = size;
  }
  ~io_context() {}
  session *parent() { return parent_; }
  WSABUF &buffer() { return w_buf_; }
  unsigned char *data() { return &buf_[0]; }
  int size() { return buf_.size(); }

private:
  WSABUF w_buf_;
  session *parent_;
  std::vector<unsigned char> buf_;
};

class session : public std::tr1::enable_shared_from_this<session> {
public:
  session() : st_(INVALID_SOCKET), send_len_(0), recv_len_(0) {
    for (int n = RECV_CONTEXT; n <= SEND_CONTEXT; n++) {
      array_ctx_[n] = new io_context(this);
    }
    st_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  }
  ~session() {
    for (int n = RECV_CONTEXT; n <= SEND_CONTEXT; n++) {
      if (array_ctx_[n]) {
        delete array_ctx_[n];
      }
    }
  }
  SOCKET socket() { return st_; }
  const int send_len() const { return send_len_; }
  const int recv_len() const { return recv_len_; }

  void up_recv_len(const int len) {
    if (len > 0)
      recv_len_ += len;
  }
  void up_send_len(const int len) {
    if (len > 0)
      send_len_ += len;
  }
  io_context *recv_context() { return array_ctx_[RECV_CONTEXT]; }
  io_context *send_context() { return array_ctx_[SEND_CONTEXT]; }
  const io_context *recv_context() const { return array_ctx_[RECV_CONTEXT]; }
  const io_context *send_context() const { return array_ctx_[SEND_CONTEXT]; }

private:
  SOCKET st_;
  long send_len_;
  long recv_len_;
  io_context *array_ctx_[2];
};

class server : public std::tr1::enable_shared_from_this<server> {
public:
  server(const std::string &local_address, const unsigned short local_port,
         cb_accept_func cb_a, cb_io_func cb_r, cb_io_func cb_w);
  ~server();

  int &thread_num() { return work_thread_num_; }
  const int thread_num() const { return work_thread_num_; }
  void start();
  void stop();

  void post_accept();
  void post_recv(io_context *ctx);
  void post_send(io_context *ctx, unsigned char *data, const int len);
private:
  void init_net_func(SOCKET s);
  void on_recv(io_context *ctx, const int len);
  void on_send(io_context *ctx, const int len);
  void on_accept(io_context *ctx, const int len);
private:
  server(const server &);
  server &operator=(const server &);

protected:
  static unsigned int WINAPI work_thread(void *param);

private:
  std::string local_address_;
  unsigned short local_port_;
  HANDLE h_iocp_;
  int work_thread_num_;
  std::vector<HANDLE> handle_work_threads_;
  LPFN_ACCEPTEX accept_ex_;
  LPFN_CONNECTEX connect_ex_;
  LPFN_DISCONNECTEX disconnect_ex_;
  SOCKET sk_listen_;
  cb_io_func cb_recv;
  cb_io_func cb_send;
  cb_accept_func cb_accept;
  safe_stack<std::tr1::shared_ptr<session> > sessions_;
};
#endif // IOCP_SERVER_H