## win_iocp_server
windows iocp实现的tcp server，基于C++ 03标准，使用vs2008开发，使用了tr1库中的shared_ptr、bind和function。

本工程，主要演示了TCP server创建服务，接受请求，出发回调，及收发数据的处理。
主要解决了，socket全双工时，能够根据OVERLAPPED指针，判断出是是收数据事件还是发数据事件，并且能够找到对应的session对象。

### 核心技术
- OVERLAPPED的继承，GetQueuedCompletionStatus有事件响应时，获取对应session对象
```
class io_context : public OVERLAPPED {
...
private:
  session *parent_;
}
```

- session用于封装套接字和对应相关联的OVERLAPPED上下文，这样套接字可以同时post 收发事件

```
class session : public std::tr1::enable_shared_from_this<session> {
...
private:
  SOCKET st_;
  long send_len_; //总共发送
  long recv_len_; //总共接收
  io_context *array_ctx_[2]; //接收和发送的OVERLAPPED对象
};
```

以上使用了两个不同的OVERLAPPED对象，创建两个的目的主要解决，同时提交收发事件时无法通过session对象的 enum dwIOType{ IO_RECV, IO_SEND}类型，判断是否为接收或发送事件。

### 后续
基于此server 实现https server
