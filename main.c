/**
 * 
 * 基于Redis网络事件框架Demo
 * 
 * @author xushun
 * 
 *  email : xushun007@gmail.com
 */


#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include "ae.h"
#include "anet.h"


#define MAX_LEN 10240
#define MAX_REQUEST_SIZE 5120

#define PORT 23456
#define IP_ADDR_LEN 40

#define TIMER_LOOP_CYCLE 10000

// 读数据状态
#define OK 0
#define ERR -1
#define NO_READY 1

// 存放错误信息的字符串
char g_err_string[MAX_LEN];

// 事件循环体
aeEventLoop *g_event_loop = NULL;

// 客户端结构体
typedef struct client {
	int fd;                    // 文件描述符
	char ipaddr[IP_ADDR_LEN];  // client IP 地址
	int port;                  // 端口
	char request[MAX_LEN];     // 客户端请求字符串
	char response[MAX_LEN];    // 响应字符串
	int len;                   // 缓冲长度
	int rsplen;
}client_t;

client_t* createClient() {
	client_t* c = malloc(sizeof(client_t));
	if(c == NULL) {
		fprintf(stderr, "alloc mem failure.");
		exit(1);
	}
	c->fd = -1;
	c->len = 0;
	c->rsplen = 0;
	return c;
}

// 定时器
int PrintTimer(struct aeEventLoop *eventLoop, long long id, void *clientData)
{
	static int i = 0;
	printf("Timer: %d\n", i++);
	// TIMER_LOOP_CYCLE/1000 秒后再次执行该函数
	return TIMER_LOOP_CYCLE;
}

//停止事件循环
void StopServer()
{
	aeStop(g_event_loop);
}

// 客户退出处理函数
void ClientClose(aeEventLoop *el, client_t* c, int err)
{
	//如果err为0，则说明是正常退出，否则就是异常退出
	if( 0 == err )
		printf("Client quit: %d\n", c->fd);
	else if( -1 == err )
		fprintf(stderr, "Client Error: %s\n", strerror(errno));

	//删除结点，关闭文件
	aeDeleteFileEvent(el, c->fd, AE_READABLE);
	close(c->fd);
	free(c);
}

// 响应客户
void sendReplyToClient(aeEventLoop *el, int fd, void *privdata, int mask) {	
	client_t* c = privdata;
	int nwritten = c->rsplen;
	int res, sentlen = 0;
	
	printf("Request From %s:%d : %s\n", c->ipaddr, c->port, c->response);
	while(nwritten) {
	    res = write(fd, c->response + sentlen, c->rsplen - sentlen);

	    // 写入出错
	    if (res == -1) {
		if (errno == EAGAIN) {
		    continue;
		} else {
		    fprintf(stderr, "send response to client failure.\n");
		    ClientClose(el, c, res);
		}
	    }
            
            nwritten -= res;
            sentlen += res;

            if (sentlen == c->rsplen) {
                c->rsplen = 0;
            }
	}

	aeDeleteFileEvent(el,c->fd,AE_WRITABLE);
}

int processBuffer(aeEventLoop *el, client_t* c, int res) {
	char *newline = strstr(c->request,"\r\n");
	int reqlen;	

	if(newline == NULL) {
		if(c->len > MAX_REQUEST_SIZE) {
			fprintf(stderr,"Protocol error: too big request");
			ClientClose(el, c, res);
			return ERR;
		}
		return NO_READY;
	}

	reqlen = newline - c->request + 2;
	memcpy(c->response, c->request, reqlen);
	c->rsplen = reqlen;
	c->len -= reqlen;
	if(c->len)
		memmove(c->request, c->request+reqlen, c->len);

	return OK;
}

// 读取客户端数据
void ReadFromClient(aeEventLoop *el, int fd, void *privdata, int mask)
{
	int res;
	client_t* c = privdata;
	res = read(fd, c->request + c->len, MAX_LEN - c->len);
	if( res <= 0 )
	{
		ClientClose(el, c, res);
		return;
	}
	c->len += res;	
	
	if(processBuffer(el, c, res) == OK) {
		if(aeCreateFileEvent(el, fd, AE_WRITABLE, sendReplyToClient, c) == AE_ERR)  {
			fprintf(stderr, "Can't Register File Writeable Event.\n");
			ClientClose(el, c, res);
		}			
	}
	
}



//接受新连接
void AcceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask)
{
	client_t* c = createClient();

	c->fd = anetTcpAccept(g_err_string, fd, c->ipaddr, &c->port);
	if(c->fd == ANET_ERR) {
		fprintf(stderr, "Accepting client connection: %s", g_err_string);
		free(c);
		return;	
	}
	printf("Connected from %s:%d\n", c->ipaddr, c->port);
	
	if( aeCreateFileEvent(el, c->fd, AE_READABLE, ReadFromClient, c) == AE_ERR )
	{
		fprintf(stderr, "Create File Event fail: fd(%d)\n", c->fd);
		close(c->fd);
                free(c);
	}
}

int main()
{

	printf("Start\n");

	signal(SIGINT, StopServer);

	//初始化网络事件循环
	g_event_loop = aeCreateEventLoop(1024*10);

	//设置监听事件
	int fd = anetTcpServer(g_err_string, PORT, NULL);
	if( ANET_ERR == fd )
		fprintf(stderr, "Open port %d error: %s\n", PORT, g_err_string);
	if( aeCreateFileEvent(g_event_loop, fd, AE_READABLE, AcceptTcpHandler, NULL) == AE_ERR )
		fprintf(stderr, "Unrecoverable error creating server.ipfd file event.");

	//设置定时事件
	aeCreateTimeEvent(g_event_loop, 1, PrintTimer, NULL, NULL);

	//开启事件循环
	aeMain(g_event_loop);

	//删除事件循环
	aeDeleteEventLoop(g_event_loop);

	printf("End\n");
	return 0;
}
