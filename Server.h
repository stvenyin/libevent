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


//������ͨѶ
#define		PORT				 9090				//���̶˿ں�
#define		BLOCK				 5					//��������]
#define     HEAD_LEN			 10					//���ݰ�����
#define		RECV_BUFF_SIZE		 2048				//���泤��


//libevent�¼��ṹ��
typedef struct sock_ev
{
	struct event* read_ev;
	struct event* write_ev;
}*pev;



// ���ӹ�����Ϣ
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
	bool			bRecvMsg;    // �Ƿ��ѽ��տͻ��˷�����������
	bool			bProcessMsg; // �Ƿ��Ѵ���ͻ��˷����������ݰ�
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
