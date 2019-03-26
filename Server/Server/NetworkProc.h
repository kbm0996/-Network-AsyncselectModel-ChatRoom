#ifndef __NETWORK_H__
#define __NETWORK_H__
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "Winmm.lib")
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <list>
#include <map>
#include "CRingBuffer.h"
#include "CSerialBuffer.h"
#include "_Protocol.h"

///////////////////////////////////////////////////////
// Network Process
///////////////////////////////////////////////////////
bool NetworkInit();
void NetworkClose();
void NetworkProc();

void SelectSocket(DWORD *dwpTableID, SOCKET* pTableSocket, FD_SET *pReadSet, FD_SET *pWriteSet);

///////////////////////////////////////////////////////
// Structure
///////////////////////////////////////////////////////
struct st_CLIENT
{
	SOCKET Sock;
	
	mylib::CRingBuffer SendQ;
	mylib::CRingBuffer RecvQ;

	DWORD dwClientID;
	DWORD dwEnterRoomID;
	WCHAR Nickname[dfNICK_MAX_LEN];
	WCHAR szIP[INET_ADDRSTRLEN];
};

struct st_CHATROOM
{
	DWORD dwRoomID;
	WCHAR szTitle[256];
	std::list<DWORD> UserList;
};


///////////////////////////////////////////////////////
// Recv & Send
///////////////////////////////////////////////////////
// 현재 서버부의 Recv, Send는 별도의 RecvQ, SendQ가 있으며 Recv, Send 시에 임시버퍼를 사용 중
// 		recv > 임시버퍼 > RecvQ 버퍼	/		SendQ > 임서버퍼 > send
// 중간에 임시 버퍼를 사용하므로 코드나 로직적으로는 간결하나 불필요한 메모리 복사가 존재
// 		recv > RecvQ 버퍼				/		SendQ 버퍼 > send
// 위의 방식으로 개선되어야 함
void AcceptProc();
bool RecvEvent(DWORD dwClientID);
bool SendEvent(DWORD dwClientID);
void SendPacket_Unicast(st_CLIENT * pClient, st_PACKET_HEADER * pHeader, mylib::CSerialBuffer * pPacket);
void SendPacket_Broadcast(st_PACKET_HEADER * pHeader, mylib::CSerialBuffer *pPacket);
void SendPacket_Broadcast(st_CHATROOM* pRoom, st_CLIENT* pExceptClient, st_PACKET_HEADER * pHeader, mylib::CSerialBuffer *pPacket);

///////////////////////////////////////////////////////
// Contents
///////////////////////////////////////////////////////
void OnRecv(st_CLIENT * pClient, WORD wType, mylib::CSerialBuffer* Packet);
bool OnRecv_Login(st_CLIENT * pClient, mylib::CSerialBuffer* Packet);
bool OnRecv_RoomList(st_CLIENT * pClient, mylib::CSerialBuffer* Packet);
bool OnRecv_RoomCreate(st_CLIENT * pClient, mylib::CSerialBuffer* Packet);
bool OnRecv_RoomEnter(st_CLIENT * pClient, mylib::CSerialBuffer* Packet);
bool OnRecv_Chat(st_CLIENT * pClient, mylib::CSerialBuffer* Packet);
bool OnRecv_RoomLeave(st_CLIENT * pClient, mylib::CSerialBuffer* Packet);
void Disconnect(DWORD iClientID);

///////////////////////////////////////////////////////
// Find
///////////////////////////////////////////////////////
st_CLIENT* FindClient(int iClientID);
st_CHATROOM* FindChatRoom(int iRoomID);

///////////////////////////////////////////////////////
// Packet
///////////////////////////////////////////////////////
int RecvComplete(st_CLIENT * pClient);

BYTE MakeCheckSum(mylib::CSerialBuffer* pPacket, DWORD dwType);
void mpResLogin(st_PACKET_HEADER * pHeader, mylib::CSerialBuffer *pPacket, BYTE byResult, DWORD dwClientID);
void mpResRoomList(st_PACKET_HEADER * pHeader, mylib::CSerialBuffer *pPacket, WORD iRoomCnt);
void mpResRoomCreate(st_PACKET_HEADER * pHeader, mylib::CSerialBuffer *pPacket, BYTE byResult, st_CHATROOM *pRoom);
void mpResRoomEnter(st_PACKET_HEADER * pHeader, mylib::CSerialBuffer *pPacket, BYTE byResult, st_CHATROOM *pRoom);
void mpResChat(st_PACKET_HEADER * pHeader, mylib::CSerialBuffer *pPacket, DWORD dwSendClientID, WORD iMsgSize, WCHAR* pContents);
void mpResRoomLeave(st_PACKET_HEADER * pHeader, mylib::CSerialBuffer *pPacket, DWORD dwClientID);
void mpResRoomDelete(st_PACKET_HEADER * pHeader, mylib::CSerialBuffer *pPacket, DWORD dwRoomID);
void mpResUserEnter(st_PACKET_HEADER * pHeader, mylib::CSerialBuffer *pPacket, WCHAR* pNick, DWORD dwClientID);



#endif