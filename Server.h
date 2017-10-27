#ifndef  _SERVER_H_

#define  _SERVER_H_

#include "io.h" 
#include "event.h"
#include "ws2tcpip.h"
#include "thread.h"
#include "string.h"
#include "iostream"
#include "Process.h"
#include <string>
#include <vector>
#include <algorithm>


using namespace std;


//服务器通讯
#define		PORT				 9090				//进程端口号
#define		BLOCK				 5					//连接数量]
#define     HEAD_LEN			 10					//数据包长度
#define		RECV_BUFF_SIZE		 2048				//缓存长度


//libevent事件结构体
typedef struct sock_ev
{
	struct event* read_ev;
	struct event* write_ev;
}*pev;



// 连接管理信息
struct ConnectInfo
{
	ConnectInfo()
	{
		isockfd = 0;
		iPort = 0;
		memset(ipaddr, 0, sizeof(ipaddr));
		bRecvMsg = false;
		bProcessMsg = false;
	}
	int				isockfd;
	int				iPort;
	char			ipaddr[32];
	bool			bRecvMsg;    // 是否已接收客户端发过来的数据
	bool			bProcessMsg; // 是否已处理客户端发过来的数据包
	unsigned long	start_clock;
	struct sock_ev* pSock_ev;
};

//libevent

void onRead(int iCliFd, short iEvent, void *arg);
void onWrite(int sock, short event, void* arg);
void onAccept(int iSvrFd, short iEvent, void *arg);
void release_sock_event(struct sock_ev* ev);
bool DelConInfo(vector<ConnectInfo> &KConInfo, int iSockfd);
bool CheckConnBeforeInsert(int sockfd);
bool SetConInfo(int sockfd, bool bState, int flag);
bool GetConnectState(int sockfd, int flag);

//do process
void do_process(int iSvrFd, void* bufferRead);

//host ip
void gethostip();

class CServer
{

public:
	int start();
};

#endif /*_SERVER_H_*/
