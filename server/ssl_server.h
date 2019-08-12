#ifndef SSL_SERVER_H
#define SSL_SERVER_H
#include "server.h"
#include <assert.h>
#include <functional>
#include <memory>
#include <openssl/ssl.h>
class ssl_param {
public:
  ssl_param(SSL_CTX *ctx) : ssl_(NULL), rbio_(NULL), wbio_(NULL) {
    assert(ctx != NULL);
    ssl_ = SSL_new(ctx);
    rbio_ = BIO_new(BIO_s_mem());
    wbio_ = BIO_new(BIO_s_mem());
    SSL_set_bio(ssl_, rbio_, wbio_);
  }
  ~ssl_param() {}
  SSL *ssl() { return ssl_; }
  BIO *rbio() { return rbio_; }
  BIO *wbio() { return wbio_; }

private:
  SSL *ssl_;
  BIO *rbio_;
  BIO *wbio_;
};

class ssl_server {
public:
  ssl_server(const std::string &local_address, const unsigned short local_port);
  ~ssl_server();
  ssl_server &start();
  ssl_server &stop();
  ssl_server &ca_cert(const std::string &path);
  ssl_server &ca_path(const std::string &path);
  ssl_server &cert(const std::string &path);
  ssl_server &key(const std::string &path);
  ssl_server &dh_param(const std::string &path);

protected:
  void on_recv(std::tr1::shared_ptr<server> s, io_context *ctx, const int len);
  void on_send(std::tr1::shared_ptr<server> s, io_context *ctx, const int len);
  void on_accept(std::tr1::shared_ptr<server> s, io_context *ctx);

private:
  std::tr1::shared_ptr<server> server_ptr_;
  SSL_CTX *ssl_ctx_;
};
#endif // SSL_SERVER_H