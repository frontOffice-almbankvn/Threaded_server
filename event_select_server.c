#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <ws2tcpip.h>
#include <WinSock2.h>
#include <process.h>
#include <WS2tcpip.h>
#include <ws2def.h>
// 
//#include <WinSock2.h>
//#include <WS2tcpip.h>
//#include <ws2def.h>
//#include <stdio.h>
//#include <limits.h>
//#include <stdbool.h>

#define SERVER_PORT 5500
#define SERVER_ADDR "127.0.0.1"
#define BUFF_SIZE 2048

#pragma comment(lib,"Ws2_32.lib")

int Receive(SOCKET, char*, int, int);
int Send(SOCKET, char*, int, int);
//#testing
void printSock(SOCKET* s, WSAEVENT* event);
int main(int argc, char* argv[]) {
	DWORD		nEvents = 0;
	DWORD		index;
	/*SOCKET		socks[WSA_MAXIMUM_WAIT_EVENTS];*/
	/*= (SOCKET*)malloc(WSA_MAXIMUM_WAIT_EVENTS * sizeof(SOCKET));*/
	SOCKET* socks = (SOCKET*)malloc(WSA_MAXIMUM_WAIT_EVENTS * sizeof(SOCKET));

	/*WSAEVENT	events[WSA_MAXIMUM_WAIT_EVENTS];*/
	WSAEVENT* events = (WSAEVENT*) malloc(WSA_MAXIMUM_WAIT_EVENTS * sizeof(WSAEVENT));

	WSANETWORKEVENTS sockEvent;

	//Step 1: Initiate WinSock
	WSADATA wsaData;
	WORD wVersion = MAKEWORD(2, 2);
	if (WSAStartup(wVersion, &wsaData)) {
		printf("Winsock 2.2 is not supported\n");
		return 0;
	}

	//Step 2: Construct LISTEN socket	
	SOCKET listenSock;
	listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	//Step 3: Bind address to socket
	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_ADDR, &serverAddr.sin_addr);

	socks[0] = listenSock;
	events[0] = WSACreateEvent(); //create new events
	nEvents++;

	// Associate event types FD_ACCEPT and FD_CLOSE
	// with the listening socket and newEvent   
	WSAEventSelect(socks[0], events[0], FD_ACCEPT | FD_CLOSE);


	if (bind(listenSock, (SOCKADDR  *)&serverAddr, sizeof(serverAddr)))
	{
		printf("Error %d: Cannot associate a local address with server socket.", WSAGetLastError());
		return 0;
	}

	//Step 4: Listen request from client
	if (listen(listenSock, 10)) {
		printf("Error %d: Cannot place server socket in state LISTEN.", WSAGetLastError());
		return 0;
	}

	printf("Server started!\n");

	char sendBuff[BUFF_SIZE], recvBuff[BUFF_SIZE];
	SOCKET connSock;
	SOCKADDR_IN clientAddr;
	int clientAddrLen = sizeof(clientAddr);
	int ret, i;

	for (i = 1; i < WSA_MAXIMUM_WAIT_EVENTS; i++) {
		socks[i] = 0;
	}

	while (1) {
		//wait for network events on all socket
		index = WSAWaitForMultipleEvents(nEvents, events, FALSE, WSA_INFINITE, FALSE);
		if (index == WSA_WAIT_FAILED) {
			printf("Error %d: WSAWaitForMultipleEvents() failed\n", WSAGetLastError());
			break;
		}

		index = index - WSA_WAIT_EVENT_0;
		//Gan bo bat su kien vao
		WSAEnumNetworkEvents(socks[index], events[index], &sockEvent);

		if (sockEvent.lNetworkEvents & FD_ACCEPT) {
			if (sockEvent.iErrorCode[FD_ACCEPT_BIT] != 0) {
				printf("FD_ACCEPT failed with error %d\n", sockEvent.iErrorCode[FD_READ_BIT]);
				break;
			}

			if ((connSock = accept(socks[index], (SOCKADDR*)&clientAddr, &clientAddrLen)) == SOCKET_ERROR) {
				printf("Error %d: Cannot permit incoming connection.\n", WSAGetLastError());
				break;
			}

			//Add new socket into socks array
			int i;
			if (nEvents == WSA_MAXIMUM_WAIT_EVENTS) {
				printf("\nToo many clients.");
				closesocket(connSock);
			}
			else
				for (i = 1; i < WSA_MAXIMUM_WAIT_EVENTS; i++)
					if (socks[i] == 0) {
						socks[i] = connSock;
						events[i] = WSACreateEvent();
						WSAEventSelect(socks[i], events[i], FD_READ | FD_CLOSE);
						nEvents++;
						break;
					}

			//reset event
			WSAResetEvent(events[index]);
		}

		if (sockEvent.lNetworkEvents & FD_READ) {
			//Receive message from client
			if (sockEvent.iErrorCode[FD_READ_BIT] != 0) {
				printf("FD_READ failed with error %d\n", sockEvent.iErrorCode[FD_READ_BIT]);
				break;
			}

			ret = Receive(socks[index], recvBuff, BUFF_SIZE, 0);

			//Release socket and event if an error occurs
			if (ret <= 0) {
				closesocket(socks[index]);
				socks[index] = 0;
				WSACloseEvent(events[index]);
				nEvents--;
			}
			else {			
				//echo to client
				memset(sendBuff, 0, sizeof(sendBuff));
				memcpy(sendBuff, recvBuff, ret);
				Send(socks[index], sendBuff, ret, 0);
				WSAResetEvent(events[index]);
				printSock(&socks[0],&events[0]);
				//reset event
				/*WSAResetEvent(events[index]);*/
			}
			
		}

		if (sockEvent.lNetworkEvents & FD_CLOSE) {
			if (sockEvent.iErrorCode[FD_CLOSE_BIT] != 0) {
				printf("FD_CLOSE failed with error %d\n", sockEvent.iErrorCode[FD_CLOSE_BIT]);
				break;
			}
			//Release socket and event
			closesocket(socks[index]);
			socks[index] = 0;
			WSACloseEvent(events[index]);
			nEvents--;
		}
	}
	return 0;


}

/* The recv() wrapper function */
int Receive(SOCKET s, char* buff, int size, int flags) {
	int n;

	n = recv(s, buff, size, flags);
	if (n == SOCKET_ERROR)
		printf("Error %d: Cannot receive data.\n", WSAGetLastError());
	else if (n == 0)
		printf("Client disconnects.\n");
	return n;
}

/* The send() wrapper function*/
int Send(SOCKET s, char* buff, int size, int flags) {
	int n;

	n = send(s, buff, size, flags);
	if (n == SOCKET_ERROR)
		printf("Error %d: Cannot send data.\n", WSAGetLastError());

	return n;
}

void printSock(SOCKET* s, WSAEVENT* event) {
	/*char thongdiep[20];
	strcpy(thongdiep, "Ban dang o trong game!");*/
	char m[20] = "client-";
	char n[1] = "";
	/*strcpy(m, "In game!");*/
	for (int i = 1; i < WSA_MAXIMUM_WAIT_EVENTS; i++) {
		if ( s[i] != 0) {
			_itoa(i, n, 1);
			strcat(m, n);
			Send( (SOCKET) s[i], m, sizeof(m), 0);
			WSAResetEvent(event[i]);
			printf("%d", i);
			/*memset(m,0,)*/
		}
	}
}
