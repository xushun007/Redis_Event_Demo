## 简介

Redis没有选择流行的事件处理框架libevent(Memcached使用)和libevent，而是自己实现了一个精简，高性能的异步网络事件框架。

Libevent为了迎合通用性造成代码庞大而牺牲了在特定平台的不少性能。

Redis的异步网络事件模块与其他模块相对独立，可独立出来，将其复用，其代码相当精炼，可读性很好，值得学习和应用。


我们将抽取其事件处理模块代码`(ae.h, ae.c, ae_epoll.c, anet.h, anet.c)`,源码来源于Redis2.6，实现一个简单的回显服务器。
Redis的内存管理是应用zmalloc()、zfree()等系列函数，可以简单的将其替换成原始的malloc()和free()函数即可。