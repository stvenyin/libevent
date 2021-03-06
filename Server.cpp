
#include "Server.h"

#pragma comment(lib,"Ws2_32.lib")


//base是基本事件管理员,参考memcached
struct event_base *main_base;

vector<ConnectInfo>		 KConInfo;

char	buffer[RECV_BUFF_SIZE];

char	ip[MAX_PATH];


int CServer::start()
{

	//////////////////////////////////////////////////////////////////////////
	struct sockaddr_in sSvrAddr;
	WSADATA wsaData;

	memset(&wsaData, 0, sizeof(wsaData));
	memset(&sSvrAddr, 0, sizeof(sSvrAddr));


	//
	int iResult = 0;
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		/*printf("Startup failed!");*/
		return -1;
	}

	gethostip();

	sSvrAddr.sin_family = AF_INET;
	sSvrAddr.sin_addr.s_addr = inet_addr(ip);
	/*sSvrAddr.sin_addr.s_addr = inet_addr("58.96.180.109"); */
	sSvrAddr.sin_port = htons(PORT);


	int iSvfFd = 0;

	iSvfFd = socket(AF_INET, SOCK_STREAM, 0);
	if (SOCKET_ERROR == iSvfFd)
	{
		/*printf("tcpSocket fail!\n");*/
		return -1;
	}

	////支持端口复用
	////设置该选项后,在父子进程模型中,当子进程为客户服务的时候如果父进程退出，可以
	////重新启动程序完成服务的无缝升级
	////否则在所有父子进程完全退出前再启动该程序会在端口上绑定失败,也即不能完成无缝升级
	////SO_REUSEADDR,地址可以重用,不必time_wait 2个MSL时间(maximum segment lifttime)
	//int reuseaddr_on = 1;
	//setsockopt(iSvfFd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuseaddr_on, sizeof(reuseaddr_on));

	//设置为非阻塞模式
	if (SOCKET_ERROR == evutil_make_socket_nonblocking(iSvfFd))
	{
		/*printf("Server set blocking fail!\n");*/
		return -1;
	}

	if (SOCKET_ERROR == bind(iSvfFd, (struct sockaddr*)&sSvrAddr, sizeof(sSvrAddr)))
	{
		/*printf("bind fail!\n");*/
		return -1;
	}

	if (SOCKET_ERROR == listen(iSvfFd, BLOCK))
	{
		/*printf("listen fail!\n");*/
		return -1;
	}

	else
	{
		/*printf("Server is start!!\n");*/
	}

	/*cout << "-----------------------------main:" << GetCurrentThreadId() << endl;*/

	struct event evListen;

	//事件初始化
	main_base = event_init();
	//事件设置
	//event_set(&evListen, iSvfFd, EV_READ | EV_PERSIST, onAccept, NULL);
	//将事件和base 关联
	//event_base_set(main_base, &evListen);
	//添加事件
	//event_add(&evListen, NULL);
	//进入事件循环,当事件队列里面的任何一个文件描述符发生事件的时候就会进入回调函数执行
	//event_base_dispatch(main_base);

	return 0;

}



// 连接请求事件回调函数 
void onAccept(int iSvrFd, short iEvent, void *arg)
{

	int fd = 0;

	struct sockaddr_in sCliAddr;
	//
	socklen_t iSinSize = sizeof(sCliAddr);
	fd = accept(iSvrFd, (struct sockaddr*)&sCliAddr, &iSinSize);

	/*cout << "-----------------------------onAccept:" << fd << endl;*/

	//设备连入
	/*cout << "-----------------------------device connect:" << fd << endl;*/

	char remote[INET_ADDRSTRLEN];
	memset(remote, 0, sizeof(remote));

	//Connect Manager
	ConnectInfo info;
	memset(&info, 0, sizeof(info));

	info.isockfd = fd;
	info.iPort = ntohs(sCliAddr.sin_port);
	const char* pTmp = inet_ntop(AF_INET, &sCliAddr.sin_addr, remote, INET_ADDRSTRLEN);
	if (pTmp != NULL)
	{
		memcpy(info.ipaddr, pTmp, strlen(pTmp));
	}


	// 注意:read_ev需要从堆里malloc出来,如果在栈上分配，那么当函数返回时变量占用的内存会被释放
	// 此时事件主循环event_base_dispatch会访问无效的内存而导致进程崩溃(即crash)

	struct sock_ev* ev = new sock_ev;
	ev->read_ev = new event;
	ev->write_ev = new event;


	info.start_clock = GetTickCount();
	info.pSock_ev = ev;
	info.bRecvMsg = false;
	info.bProcessMsg = false;
	KConInfo.push_back(info);

	// 插入之前，检查连接管理器里面是否还有相同的socket,主要是防止客户端连接超时，关闭了连接，而在onread里面没有清除掉
	CheckConnBeforeInsert(fd);

	event_set(ev->read_ev, fd, EV_READ | EV_PERSIST, onRead, ev);
	event_base_set(main_base, ev->read_ev);
	event_add(ev->read_ev, 0);
}


// 读事件回调函数 
void onRead(int iCliFd, short iEvent, void *arg)
{

	/*cout << "-----------------------------onRead:" << iCliFd << endl;
*/
	int iLen = 0;

	char buf[RECV_BUFF_SIZE] = { 0 };

	struct sock_ev* ev = (struct sock_ev*)arg;

	iLen = recv(iCliFd, buf, RECV_BUFF_SIZE, 0);

	/*cout << "-----------------------------onRead:" << buf << endl;*/

	if (0 == iLen)
	{
		/*cout << "Client Close sockfd = " << iCliFd << " iLen = " << iLen << endl;*/

		// 检查之前是否接收过客户端发过来的消息 
		bool bHasRecvMsg = GetConnectState(iCliFd, 0);
		// 如果客户端没有发内容过来，客户端是类似这样的connect -> close 没有send 与recv，通常客户端在连接超时时会出现这种现象
		if (!bHasRecvMsg)
		{
			release_sock_event(ev);
			DelConInfo(KConInfo, iCliFd);
		}
		else // 表示已经接收过消息 客户端connect -> send 
		{
			bool bHasProcessMsg = GetConnectState(iCliFd, 1);
			// 表示客户端有发内容过来，但是没有处理, 客户端 connect -> send ->close 没有recv,通常客户端在发送超时时会出现这种现象
			if (!bHasProcessMsg)
			{
				// 如果并处理客户端发过来的消息，则可以表示客户端发送超时，关闭了连接，关闭消息与命令消息同时到达
				// 在libevent的事件堆里面 命令消息在关闭消息之前，所以会先触发命令消息对应的事件，接收数据，并添加写事件onwrite 来做逻辑处理，但是根据顺序接下来会触发关闭消息对应的事件，
				// 再就接下来才处理写事件，做逻辑处理，这样之前的就会出问题，在关闭消息的回调函数里面会把event给删掉，写事件再触发时，会导致崩溃，所以加了事件的控制策略
				// 这时则不删除连接对应的事件
				/*cout << "the client send msg timeout ------------------> iCliFd = " << iCliFd << endl;*/
				SetConInfo(iCliFd, true, 1); // 设置该连接的消息处理状态为true,以便于在onWrite里面不做任何逻辑处理，只清除连接
			}
			else
			{
				// 如果已经接收并处理过客户端发过来的消息，则删除该连接对应的事件
				/*cout << "close socket normal iCliFd = " << iCliFd << endl;*/
				release_sock_event(ev);
				DelConInfo(KConInfo, iCliFd);
			}
		}

		// 最后都得把该连接给关闭掉
		closesocket(iCliFd);
		return;
	}


	else if (iLen < 0)
	{
		/*cout << "errno" << endl;*/
		return;
	}

	SetConInfo(iCliFd, true, 0);  // 表示该连接已经接收了命令消息 

	memset(buffer, 0, sizeof(buffer));
	memcpy(buffer, (char*)&iLen, sizeof(iLen));
	char* pHead = buffer;
	pHead += sizeof(iLen);
	memcpy(pHead, buf, iLen);

	event_set(ev->write_ev, iCliFd, EV_WRITE, onWrite, pHead);
	event_base_set(main_base, ev->write_ev);
	event_add(ev->write_ev, NULL);

	return;
}



// 得到该连接是否已接收并处理过消息 flag 0 表示是否已接收过命令消息 1表示是否已处理过命令消息
bool GetConnectState(int sockfd, int flag)
{
	if (0 == KConInfo.size())
	{
		return false;
	}

	vector<ConnectInfo>::iterator it = std::find_if(KConInfo.begin(), KConInfo.end(), [sockfd](const ConnectInfo& kConInfo){
		return sockfd == kConInfo.isockfd;
	});

	if (it != KConInfo.end())
	{
		if (0 == flag)
		{
			return it->bRecvMsg;
		}
		else if (1 == flag)
		{
			return it->bProcessMsg;
		}
	}
	return false;
}



// 设置连接的一些状态, flag 0表示是否已经接收过消息  1 表示是否已经处理过该消息 
bool SetConInfo(int sockfd, bool bState, int flag)
{
	if (0 == KConInfo.size())
	{
		return false;
	}

	vector<ConnectInfo>::iterator it = std::find_if(KConInfo.begin(), KConInfo.end(), [sockfd](const ConnectInfo& kConInfo){
		return sockfd == kConInfo.isockfd;
	});

	if (it != KConInfo.end())
	{
		if (0 == flag)
		{
			it->bRecvMsg = bState;
		}
		else if (1 == flag)
		{
			it->bProcessMsg = bState;
		}

		return true;
	}
	return false;
}


// 连接检查，如果有新连接要加入进来，则要检查该容器里面是否有该socket,有的话则需要清除掉
bool CheckConnBeforeInsert(int sockfd)
{
	bool bRst = DelConInfo(KConInfo, sockfd);
	return bRst;
}


//发送事件回调函数 
void onWrite(int iCliFd, short event, void* arg)
{

	cout << "-----------------------------onWrite:" << iCliFd << endl;

	bool bProcessState = GetConnectState(iCliFd, 1);

	// 表示该连接还没有处理过消息
	if (!bProcessState)
	{
		char* bufferRead = (char*)arg;
		// 对客户端发过的数据进行逻辑处理,总入口
		do_process(iCliFd, bufferRead);
		SetConInfo(iCliFd, true, 1); // 表示已经处理过该命令消息 bProcessMsg 为true
	}
	else // 表示该连接已经处理过消息或者客户端发送超时，将连接关闭了，这时则需要将其从连接容器中删除掉，并清除掉event 事件
	{
		if (KConInfo.size() > 0)
		{
			vector<ConnectInfo>::iterator it = std::find_if(KConInfo.begin(), KConInfo.end(), [iCliFd](const ConnectInfo& kConInfo){
				return iCliFd == kConInfo.isockfd;
			});

			if (it != KConInfo.end())
			{
				release_sock_event(it->pSock_ev);
			}

			DelConInfo(KConInfo, iCliFd);
		}
	}
	return;
}


//Clear Connect
bool DelConInfo(vector<ConnectInfo> &KConInfo, int iSockfd)
{

	if (0 == KConInfo.size())
	{
		return false;
	}

	vector<ConnectInfo>::iterator it;
	for (it = KConInfo.begin(); it != KConInfo.end();)
	{
		if (it->isockfd == iSockfd)
		{
			it = KConInfo.erase(it);
			continue;
		}
		it++;
	}

	return true;
}


//获取本机IP
void gethostip()
{
	struct in_addr addr;
	char host_name[MAX_PATH];
	memset(host_name, 0, MAX_PATH);

	if (SOCKET_ERROR == gethostname(host_name, MAX_PATH))
	{
		return;
	}
	struct hostent *phe = gethostbyname(host_name);
	if (NULL == phe)
	{
		return;
	}
	for (int i = 0; phe->h_addr_list[i] != 0; ++i)
	{
		memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
		strcpy(ip, inet_ntoa(addr));
		if (ip != NULL) break;
	}

	return;
}



//释放内存
void release_sock_event(struct sock_ev* ev)
{

	/*cout << "-----------------------------release:" << endl << endl;*/

	if (ev == NULL)
	{
		return;
	}

	event_del(ev->read_ev);
	delete ev->read_ev;
	delete ev->write_ev;
	delete ev;

}


//业务
void do_process(int iSvrFd, void *bufferRead)
{
	/*cout << "-----------------------------do_process:" << iSvrFd << endl;*/

	int iLen = *((int*)bufferRead);
	int iPACKLen = *((int*)buffer);

	if (iLen < HEAD_LEN)
	{
		send(iSvrFd, "Page is too short!", 30, 0);
		return;
	}

	if (iLen != iPACKLen)
	{
		send(iSvrFd, "Page is not a package!", 30, 0);
		return;
	}

	char *iType = &buffer[sizeof(int)* 2];

	if (0 == strcmp(iType, "worker"))
	{
		send(iSvrFd, "ok!", 30, 0);
		CProcess process;
		process.PushMessage();
	}

}



