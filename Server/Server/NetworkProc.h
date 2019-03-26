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
// ���� �������� Recv, Send�� ������ RecvQ, SendQ�� ������ Recv, Send �ÿ� �ӽù��۸� ��� ��
// 		recv > �ӽù��� > RecvQ ����	/		SendQ > �Ӽ����� > send
// �߰��� �ӽ� ���۸� ����ϹǷ� �ڵ峪 ���������δ� �����ϳ� ���ʿ��� �޸� ���簡 ����
// 		recv > RecvQ ����				/		SendQ ���� > send
// ���� ������� �����Ǿ�� ��
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