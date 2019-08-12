// clang-format off
#include "stdafx.h"
#include "ssl_server.h"
// clang-format on
#include <assert.h>
#include <sstream>
#include <iostream>
#include <memory>
#include "hexdump.h"

#pragma comment(lib, "ws2_32.lib")
static const char *default_address_ = "127.0.0.1";
static const int default_port_ = 12345;

static const std::string ca_cert_data =
    "-----BEGIN CERTIFICATE-----\n"
    "MIICcTCCAdoCCQDHhIe34KJlzjANBgkqhkiG9w0BAQsFADB9MQswCQYDVQQGEwJD\n"
    "TjELMAkGA1UECAwCU0gxCzAJBgNVBAcMAlNIMRYwFAYDVQQKDA1UZXN0IFNvZnR3\n"
    "YXJlMRMwEQYDVQQLDApUZXN0IEdyb3VwMQswCQYDVQQDDAJjYTEaMBgGCSqGSIb3\n"
    "DQEJARYLY2FAdGVzdC5jb20wHhcNMTkwODEyMTMwNjIyWhcNMjAwODExMTMwNjIy\n"
    "WjB9MQswCQYDVQQGEwJDTjELMAkGA1UECAwCU0gxCzAJBgNVBAcMAlNIMRYwFAYD\n"
    "VQQKDA1UZXN0IFNvZnR3YXJlMRMwEQYDVQQLDApUZXN0IEdyb3VwMQswCQYDVQQD\n"
    "DAJjYTEaMBgGCSqGSIb3DQEJARYLY2FAdGVzdC5jb20wgZ8wDQYJKoZIhvcNAQEB\n"
    "BQADgY0AMIGJAoGBALaB2d6SBJihDAGyyPvq15t28pO803QiPb8bCnSeCgAtSYVs\n"
    "j3RkGWY+3AoiPCRPUY9r5Rz0MEsdq6umkxqxK09Ky6nYVqqSMXW/hgf/j2FiJksT\n"
    "StuP2hcTinoO5xDPDLZNeQ9BbRH9ILwYMnX0OLC2tEVaAlk7CDfn/3awOPazAgMB\n"
    "AAEwDQYJKoZIhvcNAQELBQADgYEAFo9CrtFR5HqUsZ6IaC+fHYnIPYVyr8hKc02/\n"
    "F8FItr2+/rhZYKuPgeo1zPr+tQVkMTTCnu8F778VPw/6XnKgtQvR1aj4NrCPlcNI\n"
    "7LW0FRHeP3o/mtHR1AL5CAB2/KQ5mSpQEzeLZ6FM9/BpxSdnswfvhpLrCmex19y0\n"
    "KGrFiks=\n"
    "-----END CERTIFICATE-----\n";

static const std::string server_cert_data =
    "-----BEGIN CERTIFICATE-----\n"
    "MIICejCCAeMCCQCVT83IisPqCDANBgkqhkiG9w0BAQsFADB9MQswCQYDVQQGEwJD\n"
    "TjELMAkGA1UECAwCU0gxCzAJBgNVBAcMAlNIMRYwFAYDVQQKDA1UZXN0IFNvZnR3\n"
    "YXJlMRMwEQYDVQQLDApUZXN0IEdyb3VwMQswCQYDVQQDDAJjYTEaMBgGCSqGSIb3\n"
    "DQEJARYLY2FAdGVzdC5jb20wHhcNMTkwODEyMTMwNjIzWhcNMjAwODExMTMwNjIz\n"
    "WjCBhTELMAkGA1UEBhMCQ04xCzAJBgNVBAgMAlNIMQswCQYDVQQHDAJTSDEWMBQG\n"
    "A1UECgwNVGVzdCBTb2Z0d2FyZTETMBEGA1UECwwKVGVzdCBHcm91cDEPMA0GA1UE\n"
    "AwwGc2VydmVyMR4wHAYJKoZIhvcNAQkBFg9zZXJ2ZXJAdGVzdC5jb20wgZ8wDQYJ\n"
    "KoZIhvcNAQEBBQADgY0AMIGJAoGBALczzt8w8en9VD9837OYE7GtUpW20cUl/hfo\n"
    "DZXuTYv6PHioH1Iw7/tjIKFIvFsNcXp10P3dQ97hVBHsh1f33KWY+Q2Rua8z2UIP\n"
    "YgJ48kKEZPmi7yN3QZyIOXZAX7qut1ZkSAqX0LDlviDuuQVHvVvppD+9Fig7huSc\n"
    "3R8smO6/AgMBAAEwDQYJKoZIhvcNAQELBQADgYEAV8pY6/Vv+uTY7ZGN2fWlJc0Z\n"
    "UexL0gXgoeXbDEOYF0V150EC/rnsxPKjBUVLKj2vLs9Zl+S3kprLZTM6V4uZj/2c\n"
    "Lk4B+0mp5zzsgGpwQFhuHUDOa2Zo4y1+e5VIP6/EcloOw9/eAGHYbyqWX94oRrEk\n"
    "A1/edbtiX/pv8k4jiJk=\n"
    "-----END CERTIFICATE-----\n";

static const std::string server_key_data =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "MIICXAIBAAKBgQC3M87fMPHp/VQ/fN+zmBOxrVKVttHFJf4X6A2V7k2L+jx4qB9S\n"
    "MO/7YyChSLxbDXF6ddD93UPe4VQR7IdX99ylmPkNkbmvM9lCD2ICePJChGT5ou8j\n"
    "d0GciDl2QF+6rrdWZEgKl9Cw5b4g7rkFR71b6aQ/vRYoO4bknN0fLJjuvwIDAQAB\n"
    "AoGAdi/XCoeB2SkTu61sh2jZc6tT9r+tTlk3Neb/NLU6k84ISvJy2kw11WBawZGx\n"
    "6a+fgJgXDl87FMMawEFuAuMlFRuqBrRsFwk4+6VzJzV3z/BNdA7YZPleBDf0JO5N\n"
    "kJSRSYoZzaao0bYtqNv+2RMJP1qGdWwHPC8KDWSSLEVqKOECQQDn6N1F7AAaL2Ri\n"
    "HXKEQvaETwM5Ah4L16y6YUueQlHtBhuwCarlv9rob2gYdpIFu7E6+bVOmtzueI0f\n"
    "l+P1RoxxAkEAyjut+jOFQIx5xcL2fNgAlZytrE3j8V51fKT6ZwC4o764IBTWgRiT\n"
    "+L7u7z6M5T3hqcfp+IBwizJ80h87IkKGLwJAafTb/FFqboxOqgFYTBOoPCU5jLdp\n"
    "8PE2auV/PiyA7/GFfvW7zkLNCruz7NFnwBTUUeS7MNHStWYA3HlyXqNAEQJBAMQ7\n"
    "NI3bACmqJV7n1xU84xRJe5v92HiVF5ti2jaoVIFOxosarSmHF83+Nwqev0iRyy5b\n"
    "dYRT3OC0lLmu5EpSErECQCIOYUzye/denkCS5NTEuelZ1bcCxYomerjEerIC9yC9\n"
    "IiUmiSgC8yOvSvaIYbtduCURslRVcNfLfbwT1KDIQgk=\n"
    "-----END RSA PRIVATE KEY-----\n";

static const std::string dh_data =
    "-----BEGIN DH PARAMETERS-----\n"
    "MIGHAoGBAJnd2cpNAaQv+jQXaRhBz0bw8QTgklVZAAd4zDCTRtD7qrhIeEuUFMwm\n"
    "Z/kqek7pqsbqBpHsgTxCMuxrpx3feeFlZqjPA1dQnQg86aaclVBvl1jR8xu0A6Lc\n"
    "uuGrlA/QYGejzxSdmE6RKcXioAtMe5PntsOxAZCAg6+QGSTMXFVbAgEC\n"
    "-----END DH PARAMETERS-----\n";

void save_file(const std::string &path, const std::string &data) {
  std::tr1::shared_ptr<FILE> file(fopen(path.c_str(), "wb"), fclose);
  if (!file) {
    throw std::exception("error open file!");
  }

  int len = fwrite(data.data(), 1, data.length(), file.get());
  while (len < data.length()) {
    len += fwrite(data.data() + len, 1, data.length() - len, file.get());
  }
}

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
  char *pos = strrchr(argv[0], '\\');
  std::string path(argv[0], pos - argv[0]);

  std::string ca_cert_path = path + "\\ca_cert.pem";
  std::string cert_path = path + "\\cert.pem";
  std::string key_path = path + "\\key.pem";
  std::string dh_param_path = path + "\\dh.pem";
  save_file(ca_cert_path, ca_cert_data);
  save_file(cert_path, server_cert_data);
  save_file(key_path, server_key_data);
  save_file(dh_param_path, dh_data);
  assert(port_ > 0 && port_ < 65535);
  assert(address_ != NULL);
  std::tr1::shared_ptr<ssl_server> ssl_server_ptr =
      std::tr1::shared_ptr<ssl_server>(new ssl_server(address_, port_));
  ssl_server_ptr->ca_cert(ca_cert_path)
      .cert(cert_path)
      .key(key_path)
      .dh_param(dh_param_path);
  ssl_server_ptr->start();
  getchar();
  ssl_server_ptr->stop();
  return 0;
}
