#include "stdafx.h"
#include "ssl_server.h"
#include "hexdump.h"
#include <assert.h>
#include <exception>
#include <io.h>
#include <iostream>
#include <openssl/err.h>
#include <openssl/ssl.h>

using namespace std::tr1::placeholders;

ssl_server::ssl_server(const std::string &local_address,
                       const unsigned short local_port) {
  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();
  server_ptr_ = std::tr1::shared_ptr<server>(
      new server(local_address, local_port,
                 std::tr1::bind(&ssl_server::on_accept, this, _1, _2),
                 std::tr1::bind(&ssl_server::on_recv, this, _1, _2, _3),
                 std::tr1::bind(&ssl_server::on_send, this, _1, _2, _3)));
  ssl_ctx_ = SSL_CTX_new(SSLv23_server_method());
  SSL_CTX_set_cipher_list(ssl_ctx_, "-ALL:AES256-SHA");
}

ssl_server::~ssl_server() {}

ssl_server &ssl_server::ca_cert(const std::string &path) {
  assert(ssl_ctx_);
  if (0 != access(path.c_str(), 0)) {
    throw std::exception("load ca cert error!");
  }
  if (!SSL_CTX_load_verify_locations(ssl_ctx_, path.c_str(), NULL)) {
    throw std::exception("load ca cert error!", ERR_get_error());
  }
  return *this;
}

ssl_server &ssl_server::ca_path(const std::string &path) {
  assert(ssl_ctx_);
  if (!SSL_CTX_load_verify_locations(ssl_ctx_, NULL, path.c_str())) {
    throw std::exception("load ca path error!", ERR_get_error());
  }
  return *this;
}

ssl_server &ssl_server::cert(const std::string &path) {
  assert(ssl_ctx_);
  if (0 != access(path.c_str(), 0)) {
    throw std::exception("load server cert error!");
  }
  if (!SSL_CTX_use_certificate_file(ssl_ctx_, path.c_str(), SSL_FILETYPE_PEM)) {
    throw std::exception("set server use cert error!", ERR_get_error());
  }
  return *this;
}

ssl_server &ssl_server::key(const std::string &path) {
  assert(ssl_ctx_);
  if (0 != access(path.c_str(), 0)) {
    throw std::exception("load server key error!");
  }
  if (!SSL_CTX_use_PrivateKey_file(ssl_ctx_, path.c_str(), SSL_FILETYPE_PEM)) {
    throw std::exception("set server use key error!", ERR_get_error());
  }
  return *this;
}

ssl_server &ssl_server::dh_param(const std::string &path) {
  assert(ssl_ctx_);
  if (0 != access(path.c_str(), 0)) {
    throw std::exception("load dh file error!");
  }
  std::tr1::shared_ptr<BIO> guard_bio(BIO_new_file(path.c_str(), "r"),
                                      BIO_free);
  if (!guard_bio) {
    throw std::exception("load dh file error!");
  }

  std::tr1::shared_ptr<DH> guard_dh(
      PEM_read_bio_DHparams(guard_bio.get(), 0, 0, 0), DH_free);
  if (!SSL_CTX_set_tmp_dh(ssl_ctx_, guard_dh.get())) {
    throw std::exception("set server use dh error!", ERR_get_error());
  }
  return *this;
}

ssl_server &ssl_server::start() {
  server_ptr_->start();
  return *this;
}

ssl_server &ssl_server::stop() {
  server_ptr_->stop();
  return *this;
}

void ssl_server::on_recv(std::tr1::shared_ptr<server> s, io_context *ctx,
                         const int len) {
  assert(s);
  assert(ctx);  
  ssl_param *param = static_cast<ssl_param *>(ctx->param());
  assert(param);

  std::cout << Hexdump(ctx->parent()->recv_context()->data(), len);

  if (!SSL_is_init_finished(param->ssl())) {
    std::cout << len << std::endl;
    int w_len =
        BIO_write(param->rbio(), ctx->parent()->recv_context()->data(), len);
    int r = SSL_do_handshake(param->ssl());
    char buf[8192] = {0};
    int r_len = BIO_read(param->wbio(), buf, sizeof(buf));
    s->post_send(ctx, reinterpret_cast<unsigned char *>(buf), r_len);
    if (r != 1) {
      int err_SSL_get_error = SSL_get_error(param->ssl(), r);
      switch (err_SSL_get_error) {
      case SSL_ERROR_WANT_READ:
      case SSL_ERROR_WANT_WRITE:
        s->post_recv(ctx);
        break;
      default:
        closesocket(ctx->parent()->socket());
        return;
      }
    } else {
      s->post_recv(ctx);
    }
  } else {
    char buf[8192] = {0};
    int w_len =
        BIO_write(param->rbio(), ctx->parent()->recv_context()->data(), len);
    w_len = SSL_read(param->ssl(), buf, sizeof(buf));
    std::cout << Hexdump(buf, w_len) << std::endl;

    SSL_write(param->ssl(), "hello world!", sizeof("hello world!") - 1);
    int r_len = BIO_read(param->wbio(), buf, sizeof(buf));
    s->post_send(ctx, reinterpret_cast<unsigned char *>(buf), r_len);
  }
}
void ssl_server::on_send(std::tr1::shared_ptr<server> s, io_context *ctx,
                         const int len) {
  assert(s);
  assert(ctx);
  ssl_param *param = static_cast<ssl_param *>(ctx->param());
  assert(param);
  if (!SSL_is_init_finished(param->ssl())) {
    return;
  } else {
	//判断发送完hello world关闭连接
  }
}

void ssl_server::on_accept(std::tr1::shared_ptr<server> s, io_context *ctx) {
  assert(s);
  assert(ctx);

  ssl_param *param = new ssl_param(ssl_ctx_);
  assert(param);
  ctx->param(param);
  SSL_set_accept_state(param->ssl());

  s->post_recv(ctx);
}