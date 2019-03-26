#ifndef __NETWORK_H__
#define __NETWORK_H__

///////////////////////////////////////////////////////
// Network
//
///////////////////////////////////////////////////////
bool Connect();
void NetworkClose();
bool NetworkProc(WPARAM wParam, LPARAM lParam);

///////////////////////////////////////////////////////
// Send
//
///////////////////////////////////////////////////////
bool SendEvent();

void SendPacket(st_PACKET_HEADER * pHeader, mylib::CSerialBuffer * pPacket);

void SendReq_Login();
void SendReq_RoomList();
void SendReq_RoomCreate();
void SendReq_RoomEnter(DWORD dwRoomNO);
void SendReq_Chat();
void SendReq_RoomLeave();

BYTE MakeCheckSum(mylib::CSerialBuffer* pPacket, DWORD dwType);
void mpReqLogin(st_PACKET_HEADER * pHeader, mylib::CSerialBuffer *pPacket, WCHAR* szNickName);
void mpReqRoomList(st_PACKET_HEADER * pHeader, mylib::CSerialBuffer *pPacket);
void mpReqRoomCreate(st_PACKET_HEADER * pHeader, mylib::CSerialBuffer *pPacket, WORD wTitleSize, WCHAR* szRoomName);
void mpReqRoomEnter(st_PACKET_HEADER * pHeader, mylib::CSerialBuffer *pPacket, DWORD dwRoomID);
void mpReqChat(st_PACKET_HEADER * pHeader, mylib::CSerialBuffer *pPacket, WORD wMsgSize, WCHAR* szContents);
void mpReqRoomLeave(st_PACKET_HEADER * pHeader, mylib::CSerialBuffer *pPacket);

///////////////////////////////////////////////////////
// Recv
//
///////////////////////////////////////////////////////
bool RecvEvent();
int RecvComplete();

WCHAR* FindUser(int iClientID);
WCHAR* FindRoom(int iRoomID);

bool DeleteUser(int iClientID);
bool DeleteRoom(int iRoomID);

void OnRecv(WORD wType, mylib::CSerialBuffer* Packet);
void OnRecv_Login(mylib::CSerialBuffer* Packet);
void OnRecv_RoomList(mylib::CSerialBuffer* Packet);
void OnRecv_RoomCreate(mylib::CSerialBuffer* Packet);
void OnRecv_RoomEnter(mylib::CSerialBuffer* Packet);
void OnRecv_Chat(mylib::CSerialBuffer* Packet);
void OnRecv_RoomLeave(mylib::CSerialBuffer* Packet);
void OnRecv_RoomDelete(mylib::CSerialBuffer* Packet);
void OnRecv_UserEnter(mylib::CSerialBuffer* Packet);
#endif