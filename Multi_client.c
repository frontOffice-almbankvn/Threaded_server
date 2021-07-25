#include "winsock2.h"
#include "stdio.h"
#include "WS2tcpip.h"
#include "string.h"
#include "windows.h"
#include <stdbool.h>;

#define SERVER_ADDR "127.0.0.1"
#define PORT 5500
//#define DATA_BUFSIZE 8192
#define DATA_BUFSIZE 100
#define RECEIVE 0
#define SEND 1



#pragma comment(lib, "Ws2_32.lib")

//WIN32;_DEBUG;_CONSOLE;%(PreprocessorDefinitions);_WINSOCK_DEPRECATED_NO_WARNINGS;_CRT_SECURE_NO_WARNINGS
/*Struct contains information of the socket communicating with client*/
typedef struct SocketInfo {
	SOCKET socket;
	WSAOVERLAPPED overlapped;
	WSABUF dataBuf;
	char buffer[DATA_BUFSIZE];
	int operation;
	int sentBytes;
	int recvBytes;
	
	int order_steps;
	char clientName[100];
	int id;
	int listKhieuChien[WSA_MAXIMUM_WAIT_EVENTS];
	int cKC;
	int idOpp;
	SOCKET socketOpp;

} SocketInfo;

int ma01[10] = { '0','1','2','3','4','5','6','7','8','9'};

void freeSockInfo(SocketInfo* siArray[], int n);
void closeEventInArray(WSAEVENT eventArr[], int n);
void checkNumberClient(SocketInfo* siArray[]);
int Send(SOCKET s, char* buff, int size, int flags, const char * c);
void manageAClient(SocketInfo* siArray[], WSAEVENT eventArr[], int n, int * nevents);
bool checkIDList(int danhsach[], int id);


int main() {
	SocketInfo* socks[WSA_MAXIMUM_WAIT_EVENTS];
	WSAEVENT events[WSA_MAXIMUM_WAIT_EVENTS];
	int nEvents = 0; //Bien dem su kien
	int idClient = 100;

	WSADATA wsaData;
	if (WSAStartup((2, 2), &wsaData) != 0) {
		printf("WSAStartup() failed with error %d\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	//Khoi tao gia tri mac dinh cho mang sock va event
	for (int i = 0; i < WSA_MAXIMUM_WAIT_EVENTS; i++) {
		socks[i] = 0;
		events[i] = 0;
	}

	//Bat dau mot socket de lang nghe
	SOCKET listenSocket;
	if ((listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET) {
		printf("Error %d: Cannot create server socket.", WSAGetLastError());
		return 1;
	}

	//Gan doi tuong SOCKET lang nghe voi 1 doi tuong event
	events[0] = WSACreateEvent();
	nEvents++;
	WSAEventSelect(listenSocket, events[0], FD_ACCEPT | FD_CLOSE);

	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	inet_pton(AF_INET, SERVER_ADDR, &serverAddr.sin_addr);

	if (bind(listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr))) {
		printf("Error %d: Cannot associate a local address with server socket.", WSAGetLastError());
		return 0;
	}

	if (listen(listenSocket, 10)) {
		printf("Error %d: Cannot place server socket in state LISTEN.", WSAGetLastError());
		return 0;
	}

	printf("Server started!\n");

	int index, ret;
	SOCKET connSock;
	SOCKADDR_IN clientAddr;
	int clientAddrLen = sizeof(clientAddr);

	while (1) {
		//Cho su kien network tren cac socket
		index = WSAWaitForMultipleEvents(nEvents, events, FALSE, WSA_INFINITE, FALSE);

		if (index == WSA_WAIT_FAILED) {
			printf("Error %d: WSAWaitForMultipleEvents() failed \n", WSAGetLastError());
			return 0;
		}

		index = index - WSA_WAIT_EVENT_0;
		DWORD flags, transferredBytes;

		//If the event trigger =0 => mot no luc ket noi da duoc ghi nhan
		//
		if (index == 0) {
			WSAResetEvent(events[0]);
			//Khi mot socket moi duoc ket noi den, no se tra ve connection socket
			//Van de la socket listen van chi co 1 => vao ra bat dong bo theo su kien thuc chat la gi?
			if ((connSock = accept(listenSocket, (SOCKADDR*) &clientAddr, &clientAddrLen)) == INVALID_SOCKET) {
				printf("Error %d: Cannot permit incoming connection.\n", WSAGetLastError());
				return 0;
			}

			int i;
			//Bat dau kiem tra va tao socket client
			if (nEvents == WSA_MAXIMUM_WAIT_EVENTS) {
				printf("\nToo many clients");
				closesocket;
			}
			else {
				//Ngat socket nay voi tat cac cac event object
				WSAEventSelect(connSock, NULL, 0);

				//them socket nay vao mang SocketInfo
				i = nEvents;
				events[i] = WSACreateEvent();
				socks[i] = (SocketInfo *) malloc(sizeof(SocketInfo));
				socks[i]->socket = connSock;
				memset(&socks[i]->overlapped, 0, sizeof(WSAOVERLAPPED));
				socks[i]->overlapped.hEvent = events[i];
				socks[i]->dataBuf.buf = socks[i]->buffer;
				socks[i]->dataBuf.len = DATA_BUFSIZE;
				socks[i]->operation = RECEIVE;
				socks[i]->recvBytes = 0;
				socks[i]->sentBytes = 0;
				//Bo sung them ID
				socks[i]->id = idClient ;
				//So nguoi khieu chien
				socks[i]->cKC = 0;
				idClient++;

				nEvents++;

				//dua yeu cau overlap I/O request de bat dau nhan du lieu tu socket
				flags = 0;
				if (WSARecv(socks[i]->socket, &(socks[i]->dataBuf ),1, &transferredBytes,&flags, &(socks[i]->overlapped) , NULL) == SOCKET_ERROR ) {
					if (WSAGetLastError() != WSA_IO_PENDING) {
						printf("WSARecv() failed with error %d\n", WSAGetLastError());
						closeEventInArray(events, i);
						freeSockInfo(socks, i);
						nEvents--;
					}
				}
			}
		}
		else {
			manageAClient(socks, events, index, &nEvents);
		}
		//else { //Neu su kien duoc kich hoat khong phai la 0 thi yeu cau I/O hoan thanh
		//	SocketInfo* client;
		//	client = socks[index];
		//	WSAResetEvent(events[index]);
		//	BOOL result;
		//	result = WSAGetOverlappedResult(client->socket, &(client->overlapped), &transferredBytes, FALSE, &flags);
		//	if (result == FALSE || transferredBytes == 0) {
		//		closesocket(client->socket);
		//		closeEventInArray(events, index);
		//		freeSockInfo(socks, index);
		//		client = 0;
		//		nEvents--;
		//		continue;
		//	}

		//	// Check to see if the operation field equals RECEIVE. If this is so, then
		//	// this means a WSARecv call just completed => Tuc la phai hieu doan nay the nao>?
		//	if (client->operation == RECEIVE) {
		//		client->recvBytes = transferredBytes;
		//		client->sentBytes = 0;
		//		client->operation = SEND;
		//	}
		//	else
		//		client->sentBytes += transferredBytes;

		//	// Post another I/O operation
		//	// Since WSASend() is not guaranteed to send all of the bytes requested,
		//	// continue posting WSASend() calls until all received bytes are sent. => Dam bao gui du du lieu
		//	if (client->recvBytes > client->sentBytes) {
		//		client->dataBuf.buf = client->buffer + client->sentBytes;
		//		client->dataBuf.len = client->recvBytes - client->sentBytes;
		//		client->operation = SEND;
		//		if (WSASend(client->socket, &(client->dataBuf),1,&transferredBytes, flags,&(client ->overlapped),NULL) == SOCKET_ERROR ) {
		//			if (WSAGetLastError() != WSA_IO_PENDING) {
		//				printf("WSASend() failed with error %d\n", WSAGetLastError());
		//				closesocket(client->socket);
		//				closeEventInArray(events, index);
		//				freeSockInfo(socks, index);
		//				client = 0;
		//				nEvents--;
		//				continue;
		//			}
		//		}
		//	}
		//	else {
		//		// No more bytes to send post another WSARecv() request
		//		memset(&(client->overlapped), 0, sizeof(WSAOVERLAPPED));
		//		client->overlapped.hEvent = events[index];
		//		client->recvBytes = 0;
		//		client->operation = RECEIVE;
		//		client->dataBuf.buf = client->buffer;
		//		client->dataBuf.len = DATA_BUFSIZE;
		//		flags = 0;
		//		if (WSARecv(client->socket, &(client->dataBuf), 1, &transferredBytes, &flags, &(client->overlapped), NULL) == SOCKET_ERROR) {
		//			if (WSAGetLastError() != WSA_IO_PENDING) {
		//				printf("WSARecv() failed with error %d\n", WSAGetLastError());
		//				closesocket(client->socket);
		//				closeEventInArray(events, index);
		//				freeSockInfo(socks, index);
		//				client = 0;
		//				nEvents--;
		//			}
		//		}
		//	}
		//}
		checkNumberClient(socks);
		//if (index != 0) {
		//	printf("\n");
		//	for (int i = 1; i < WSA_MAXIMUM_WAIT_EVENTS; i++) {
		//		if ((socks[i] != 0) ) {
		//			//&& (socks[i]->operation == RECEIVE)
		//			char m[10];
		//			strcpy(m, "New client");
		//			SocketInfo* client;
		//			client = socks[i];
		//			WSABUF buff;
		//			buff.buf = m;
		//			buff.len = sizeof(m);
		//			/*WSASend(client->socket, &(buff), 1, &buff.len, &flags, &(client->overlapped), NULL);*/
		//			/*WSASend(client->socket, &(buff), 1, &buff.len, &flags, &(client->overlapped), NULL);*/
		//			Send(client->socket, m, 10, 0);
		//		}
		//	}
		//}

	/*	sentToClient(socks, events, index);*/
	}


	return 0;
}

//giai phong 1 socket khoi mang
void freeSockInfo(SocketInfo* siArray[], int n) {
	closesocket(siArray[n]->socket);
	free(siArray[n]);
	siArray[n] = 0;
	printf("Socket %d closed\n", n);
	for (int i = n; i < WSA_MAXIMUM_WAIT_EVENTS - 1; i++) {
		//Doan nay don nhung socket con o vi tri phia sau len tren
		siArray[i] = siArray[i + 1];
	}
}

//giai phong event khoi array
void closeEventInArray(WSAEVENT eventArr[], int n) {
	WSACloseEvent(eventArr[n]);
	printf("Event %d closed\n", n);
	for (int i = n; i < WSA_MAXIMUM_WAIT_EVENTS - 1; i++) {
		//Doan nay don nhung event o phai sau len tren 1 don vi 

		eventArr[i] = eventArr[i + 1];
	}
}
void checkNumberClient(SocketInfo* siArray[]) {
	for (int i = 1; i < WSA_MAXIMUM_WAIT_EVENTS; i++) {
		if (siArray[i] != 0) {
			printf("Sock %d is active\n", i);
		}
	}
}

int Send(SOCKET s, char* buff, int size, int flags, const char* c) {
	int n;

	n = send(s, buff, size, flags);
	if (n == SOCKET_ERROR) {
		printf("Error %d: Cannot send data.\n", WSAGetLastError());
		printf("Error: %s\n", c);
	}
	
	return n;
}

void manageAClient(SocketInfo* socks[], WSAEVENT events[], int n, int* nevents) {
	SocketInfo* client;
	client = socks[n];
	WSAResetEvent(events[n]);
	BOOL result;
	DWORD flags, transferredBytes;
	bool orderCheck;

	flags = 0;
	transferredBytes = 50;

	result = WSAGetOverlappedResult(client->socket, &(client->overlapped), &transferredBytes, FALSE, &flags);

	if (result == FALSE || transferredBytes == 0) {
		closesocket(client->socket);
		closeEventInArray(events, n);
		freeSockInfo(socks, n);
		client = 0;
		*nevents =  *nevents -1;
		return 0;
	}
	/*Send(client->socket, "Thanh Cong", 50, 0);*/

	//Chuyen trang thai socket thanh Receive
	if (client->operation == RECEIVE) {
		client->recvBytes = transferredBytes;	//the number of bytes which is received from client
		client->sentBytes = 0;					//the number of bytes which is sent to client
		client->operation = SEND;				//set operation to send reply message
	}

	char messContent[200];
	strcpy(messContent, client->dataBuf.buf);

	printf(messContent);
	printf("\n");

	//Vao he thong choi game
	if ( ( strncmp(messContent, "10",2) == 0 ) && (client->order_steps <= 10)) {
		Send(client->socket, "Nhap vao ten cua ban (20 [TenBan]):", 50, 0,"Gui du lieu buoc 1");
		client->order_steps = 10;
	}

	//Dat ten nhan vat
	else if ( (strncmp(messContent, "20",2) == 0)  && (client->order_steps <= 20)) {

		strncpy(client->clientName, messContent + 3, 50 - 3);
		char m[100] = "Ten cua ban la: ";

		strncat(m, client->clientName, 50);
		Send(client->socket, m, 50, 0, "Gui du lieu buoc 2");
		client->order_steps = 20;
	}

	//Tim kiem doi thu
	else if ( ( strncmp( messContent, "30",2) == 0 ) && (client->order_steps >= 20) && (client->order_steps <= 30)) {
		char o[100];
		char sid[4];
		strcpy(o, "3List");

		Send(client->socket, o, 50, 0, "Gui du lieu buoc 3, lan1");
		for (int i = 1; i < WSA_MAXIMUM_WAIT_EVENTS - 1; i++) {
			//Dam bao dieu kien ve trang thai cua socks, thu tu step, id
			if (socks[i] != 0) {
				if ((socks[i]->operation == RECEIVE) && (socks[i]->order_steps >= 20) && (socks[i]->order_steps <= 40) && (socks[i]->id != client->id)) {
					memset(o, 0, sizeof(o));
					memset(sid, 0, sizeof(sid));

					/*itoa(socks[i]->id, sid, 10);*/

					snprintf(sid, 4, "%d", socks[i]->id);

					strcpy(o, sid);
					strcat(o, "-");
					strncat(o, socks[i]->clientName,50);
					Send(client->socket, o, 50, 0, "Gui du lieu buoc 3, thong tin nguoi choi");
				}
			}
		}
		Send(client->socket, "END", 50, 0, "Ket thuc buoc 3");
		client->order_steps = 30;
	}

	//Khop doi thu
	else if ((strncmp(messContent, "40", 2) == 0)) {
		char idDoiThu[3];
		int idDT;
		int ct;

		char o[100];
		char sid[4];

		strncpy(idDoiThu, messContent + 3, 3);

		ct = 0;
		idDT = atoi(idDoiThu);
		if (idDT == 0) {
			send(client->socket, "ID Doi thu khong hop le", 50, 0);
		}
		else if (client->cKC >= WSA_MAXIMUM_WAIT_EVENTS - 1) {
			send(client->socket, "Da khieu chien qua nhieu, an 41 de clear", 50, 0);
		}
		else {
			for (int i = 1; i < WSA_MAXIMUM_WAIT_EVENTS - 1; i++) {
				if (socks[i] != 0) {
					//Trang thai, order step, id
					if ((socks[i]->operation == RECEIVE) && (socks[i]->order_steps >= 20) && (socks[i]->order_steps <= 40) && (socks[i]->id != client->id) && (socks[i]->id == idDT)) {
						/*client->order_steps = 41;*/
						send(client->socket, "Da gui yeu cau chien dau", 50, 0);
						//Gui yeu cau chien dau
						snprintf(sid, 4, "%d", client->id);
						strcpy(o, "Khieu chien: ");
						strcat(o, "-");
						strcat(o, sid);
						strncat(o, client->clientName, 50);
						//strcat(o, " ");

						send(socks[i]->socket, o, 50, 0);

						//Luu danh sach khieu chien
						client->listKhieuChien[client->cKC] = socks[i]->id;
						client->cKC = client->cKC + 1;
						ct++;
						break;
					}
				}
			}
		}

		if ((ct == 0) && (idDT != 0)) {
			client->order_steps = 40;
			send(client->socket, "Khong tim thay doi thu", 50, 0);
		}
	}

	else if ( (strncmp(messContent, "41", 2) == 0) )  {
		if (client->order_steps < 50) {
			client->order_steps = 40;
			client->cKC = 0;
			client->socketOpp = 0;
			memset(client->listKhieuChien, 0, sizeof(client->listKhieuChien));
			send(client->socket, "Da reset, ban dang o trang thai 40", 50, 0);
		}
		else {
			send(client->socket, "Ban dang trong trang thai chien dau!", 50, 0);
		}	
	}

	//Chap nhan khieu chien
	else if ((strncmp(messContent, "45", 2) == 0)) {
		char idDoiThu[3];
		int idDT;
		int ct;

		char o[100];
		char sid[4];

		strncpy(idDoiThu, messContent + 3, 3);

		ct = 0;
		idDT = atoi(idDoiThu);
		if (idDT == 0) {
			send(client->socket, "ID Doi thu khong hop le", 50, 0);
		}
		else {
			for (int i = 1; i < WSA_MAXIMUM_WAIT_EVENTS - 1; i++) {
				if (socks[i] != 0) {
					if ((socks[i]->operation == RECEIVE) && (socks[i]->order_steps >= 20) && (socks[i]->order_steps <= 40) && (socks[i]->id != client->id) && (socks[i]->id == idDT)) {
						//neu nam trong danh sach khieu chien
						if (checkIDList(socks[i]->listKhieuChien, client->id)) {


							snprintf(sid, 4, "%d", client->id);
							strcpy(o, "Phan Hoi KC: ");
							strcat(o, "-");
							strcat(o, sid);
							strncat(o, client->clientName, 50);
							//strcat(o, " ");

							send(socks[i]->socket, o, 50, 0);
							send(client->socket, "Da gui phan hoi khieu chien", 50, 0);
							client->order_steps = 45; //Cho xac nhan
							client->idOpp = socks[i]->id; // Lay id doi thu
							break;
						}
						else
						{
							send(client->socket, "Khong co trong danh sach khieu chien", 50, 0);
						}
					}
				}
				
			}
		}
	}

	//Phan hoi chap nhan khieu chien
	else if ((strncmp(messContent, "50", 2) == 0) && (client->order_steps < 50)) {
		char idDoiThu[3];
		int idDT;
		int ct;

		char o[100];
		char sid[4];

		strncpy(idDoiThu, messContent + 3, 3);

		ct = 0;
		idDT = atoi(idDoiThu);
		if (idDT == 0) {
			send(client->socket, "ID Doi thu khong hop le", 50, 0);
		}
		else {
			
			for (int i = 1; i < WSA_MAXIMUM_WAIT_EVENTS - 1; i++) {
				if (socks[i] != 0) {
					//Chi tim socket cos order_step == 45
					if ((socks[i]->operation == RECEIVE) && (socks[i]->order_steps == 45) && (socks[i]->id != client->id) && (socks[i]->id == idDT)) {
						if (socks[i]->idOpp == client->id) {
							client->idOpp = socks[i]->id;
							socks[i]->socketOpp = client->socket;
							client->socketOpp = socks[i]->socket;


							//Chuyen sang trang thai chien dau
							client->order_steps = 50;
							socks[i]->order_steps = 50;


							//Gui lai thong diep
							memset(o, 0, sizeof(o));
							memset(sid, 0, sizeof(sid));

							/*itoa(socks[i]->id, sid, 10);*/

							snprintf(sid, 4, "%d", socks[i]->id);

							strcpy(o, "Ghep Cap: ");
							strcat(o, sid);
							strcat(o, "-");

							memset(sid, 0, sizeof(sid));
							snprintf(sid, 4, "%d", client->id);

							strcat(o, sid);

							send(client->socket, o, 50, 0);
							send(client->socketOpp, o, 50, 0);
						}
						else
						{
							send(client->socket, "Doi thu khong xac nhan ban!", 50, 0);
						}
					}
				}
			}
		}

	}
	
	
	
	//Dat lai trang thai receive
	memset(&(client->overlapped), 0, sizeof(WSAOVERLAPPED));
	client->overlapped.hEvent = events[n];
	client->recvBytes = 0;
	client->operation = RECEIVE;
	client->dataBuf.buf = client->buffer;
	client->dataBuf.len = DATA_BUFSIZE;
	flags = 0;

	if (WSARecv(client->socket, &(client->dataBuf), 1, &transferredBytes, &flags, &(client->overlapped), NULL) == SOCKET_ERROR) {
		if (WSAGetLastError() != WSA_IO_PENDING) {
			printf("WSARecv() failed with error %d\n", WSAGetLastError());
			closesocket(client->socket);
			closeEventInArray(events, n);
			freeSockInfo(socks, n);
			client = 0;
			*nevents = *nevents - 1;
		}
	}


}

bool checkIDList(int danhsach[], int id) {
	for (int i = 0; i < WSA_MAXIMUM_WAIT_EVENTS - 1; i++) {
		if (id == danhsach[i]) {
			return true;
		}
	}
	return false;
}