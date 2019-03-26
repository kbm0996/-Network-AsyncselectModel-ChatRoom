#include "stdafx.h"
#include "NetworkProc.h"
#include "Client.h"
using namespace mylib;
using namespace std;

map<DWORD, WCHAR*>	g_ChatRoomMap;
map<DWORD, WCHAR*>	g_EnterRoomUserMap;
SOCKET		g_Socket = INVALID_SOCKET;
DWORD		g_dwEnterRoomID;
CRingBuffer	g_RecvQ;
CRingBuffer	g_SendQ;
BOOL		g_bConnect;
BOOL		g_bSendFlag;

bool Connect()
{
	int err;
	WCHAR szErr[100] = { 0, };
	WSADATA	wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		err = WSAGetLastError();
		swprintf_s(szErr, L"WSAStartup() ErrorCode: %d", err);
		MessageBox(g_hWndLobby, szErr, L"Error", MB_OK | MB_ICONERROR);
		return false;
	}

	g_Socket = socket(AF_INET, SOCK_STREAM, 0);
	if (g_Socket == INVALID_SOCKET)
	{
		err = WSAGetLastError();
		swprintf_s(szErr, L"socket() ErrorCode: %d", err);
		MessageBox(g_hWndLobby, szErr, L"Error", MB_OK | MB_ICONERROR);
		return false;
	}

	BOOL bOptval = TRUE;
	setsockopt(g_Socket, IPPROTO_TCP, TCP_NODELAY, (char *)&bOptval, sizeof(bOptval));

	SOCKADDR_IN serveraddr;
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(dfNETWORK_PORT);
	InetPton(AF_INET, g_szIP, &serveraddr.sin_addr);

	if(WSAAsyncSelect(g_Socket, g_hWndLobby, WM_NETWORK, FD_CONNECT | FD_READ | FD_WRITE | FD_CLOSE))
	{
		err = WSAGetLastError();
		swprintf_s(szErr, L"WSAAsyncSelect() ErrorCode: %d", err);
		MessageBox(g_hWndLobby, szErr, L"Error", MB_OK | MB_ICONERROR);
		return false;
	}

	if (connect(g_Socket, (SOCKADDR *)&serveraddr, sizeof(serveraddr)) == SOCKET_ERROR)
	{
		err = WSAGetLastError();
		if (err != WSAEWOULDBLOCK)
		{
			swprintf_s(szErr, L"connect() ErrorCode: %d", err);
			MessageBox(g_hWndLobby, szErr, L"Error", MB_OK | MB_ICONERROR);
			return false;
		}
	}
	return true;
}

void NetworkClose()
{
	closesocket(g_Socket);
	WSACleanup();
}

bool NetworkProc(WPARAM wParam, LPARAM lParam)
{
	if (WSAGETSELECTERROR(lParam) != 0)
	{
		NetworkClose();
		PostQuitMessage(0);
		return true;
	}

	switch (WSAGETSELECTEVENT(lParam))
	{
	case FD_CONNECT:
		g_bConnect = TRUE;
		break;
	case FD_CLOSE:
		//���� ���� ó��. �����δ� ������ ���������ϴ� �޽��� ���� �����.
		//ProcClose();
		return false;
	case FD_READ:
		RecvEvent();	// Read���� �ݺ��� �־ wouldblock �� ������ ��� ó���ص� �ȴ�. (Ŭ�� ����)
		break;
	case FD_WRITE:
		g_bSendFlag = TRUE;
		SendEvent();
		break;
	}
	return true;
}

bool SendEvent()
{
	if (!g_bSendFlag)	// Send ���ɿ��� Ȯ�� / �̴� FD_WRITE �߻������� -> AsyncSelect������ connect ������ FD_WRITE�� ��
		return false;

	if (g_SendQ.GetUseSize() < sizeof(st_PACKET_HEADER))
		return true;

	WSABUF wsabuf[2];
	DWORD dwTransferred = 0;
	DWORD dwFlag = 0;
	int iBufCnt = 1;
	wsabuf[0].buf = g_SendQ.GetReadBufferPtr();
	wsabuf[0].len = g_SendQ.GetUnbrokenDequeueSize();
	if (wsabuf[0].len < g_SendQ.GetUseSize())
	{
		wsabuf[1].buf = g_SendQ.GetBufferPtr();
		wsabuf[1].len = g_SendQ.GetUseSize() - wsabuf[0].len;
		++iBufCnt;
	}

	if (WSASend(g_Socket, wsabuf, iBufCnt, &dwTransferred, 0, NULL, NULL) == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != WSAEWOULDBLOCK)
		{
			WCHAR szErr[100] = { 0, };
			swprintf_s(szErr, L"WSASend() ErrorCode: %d", err);
			MessageBox(g_hWndLobby, szErr, L"Error", MB_OK | MB_ICONERROR);
			return false;
		}
		else
		{
			g_bSendFlag = false;
			return false;
		}
	}

	//-----------------------------------------------------
	// Send Complate
	// ��Ŷ ������ �Ϸ�ƴٴ� �ǹ̴� �ƴ�. ���� ���ۿ� ���縦 �Ϸ��ߴٴ� �ǹ�
	// �۽� ť���� ���´� ������ ����
	//-----------------------------------------------------
	g_SendQ.MoveReadPos(dwTransferred);
	return true;

	/* send ver*/
	//char SendBuff[CRingBuffer::en_BUFFER_DEFALUT];

	//if (!g_bSendFlag)	// Send ���ɿ��� Ȯ�� / �̴� FD_WRITE �߻������� -> AsyncSelect������ connect ������ FD_WRITE�� ��
	//	return false;

	//int iSendSize = g_SendQ.GetUseSize();
	//iSendSize = min(CRingBuffer::en_BUFFER_DEFALUT, iSendSize);
	//if (iSendSize <= 0)
	//	return true;

	//g_bSendFlag = TRUE;

	//while (1)
	//{
	//	// SendQ �� �����Ͱ� �ִ��� Ȯ�� (������)
	//	iSendSize = g_SendQ.GetUseSize();
	//	iSendSize = min(CRingBuffer::en_BUFFER_DEFALUT, iSendSize);
	//	if (iSendSize <= 0)
	//		break;

	//	// Peek���� ������ ����. ������ ����� ���������� ��� ����
	//	g_SendQ.Peek(SendBuff, iSendSize);

	//	int iResult = send(g_Socket, SendBuff, iSendSize, 0);
	//	if (iResult == SOCKET_ERROR)
	//	{
	//		int err = WSAGetLastError();
	//		if (err != WSAEWOULDBLOCK)
	//		{
	//			g_bSendFlag = FALSE;
	//			break;
	//		}
	//	}
	//	else
	//	{
	//		if (iResult > iSendSize)
	//		{
	//			//-----------------------------------------------------
	//			// ���� ������� �� ũ�ٸ� ����
	//			// ����� �ȵǴ� ��Ȳ������ ���� �̷� ��찡 ���� �� �ִ�
	//			//-----------------------------------------------------
	//			return false;
	//		}
	//		else
	//		{
	//			//-----------------------------------------------------
	//			// Send Complate
	//			// ��Ŷ ������ �Ϸ�ƴٴ� �ǹ̴� �ƴ�. ���� ���ۿ� ���縦 �Ϸ��ߴٴ� �ǹ�
	//			// �۽� ť���� Peek���� ���´� ������ ����
	//			//-----------------------------------------------------
	//			g_SendQ.MoveReadPos(iResult);
	//		}
	//	}
	//}
	//return true;
}

void SendPacket(st_PACKET_HEADER * pHeader, CSerialBuffer * pPacket)
{
	g_SendQ.Enqueue((char*)pHeader, sizeof(st_PACKET_HEADER));
	g_SendQ.Enqueue((char*)pPacket->GetBufferPtr(), pPacket->GetUseSize());
	SendEvent();
}

void SendReq_Login()
{
	st_PACKET_HEADER stPacketHeader;
	CSerialBuffer Packet;
	//------------------------------------------------------------
	// 1 Req �α���
	//
	//
	// WCHAR[15]	: �г��� (�����ڵ�)
	//------------------------------------------------------------
	mpReqLogin(&stPacketHeader, &Packet, g_szNickname);
	SendPacket(&stPacketHeader, &Packet);
}

void SendReq_RoomList()
{
	st_PACKET_HEADER stPacketHeader;
	CSerialBuffer Packet;
	//------------------------------------------------------------
	// 3 Req ��ȭ�� ����Ʈ
	//
	//	None
	//------------------------------------------------------------
	mpReqRoomList(&stPacketHeader, &Packet);
	SendPacket(&stPacketHeader, &Packet);
}

void SendReq_RoomCreate()
{
	st_PACKET_HEADER stPacketHeader;
	CSerialBuffer Packet;
	//------------------------------------------------------------
	// 5 Req ��ȭ�� ����
	//
	// 2Byte : ������ Size			�����ڵ� ���� ����Ʈ ���� (�� ����)
	// Size  : ������ (�����ڵ�)
	//------------------------------------------------------------
	WORD wTitleSize;
	WCHAR szRoomTitle[256] = { 0, };

	GetDlgItemText(g_hWndLobby, IDC_EDIT_ROOM, szRoomTitle, 255);
	wTitleSize = wcslen(szRoomTitle) * sizeof(WCHAR);
	mpReqRoomCreate(&stPacketHeader, &Packet, wTitleSize, szRoomTitle);
	SendPacket(&stPacketHeader, &Packet);
}

void SendReq_RoomEnter(DWORD dwRoomNO)
{
	st_PACKET_HEADER stPacketHeader;
	CSerialBuffer Packet;
	//------------------------------------------------------------
	// 7 Req ��ȭ�� ����
	//
	//	4Byte : �� No
	//------------------------------------------------------------
	mpReqRoomEnter(&stPacketHeader, &Packet, dwRoomNO);
	SendPacket(&stPacketHeader, &Packet);
}

void SendReq_Chat()
{
	st_PACKET_HEADER stPacketHeader;
	CSerialBuffer Packet;
	//------------------------------------------------------------
	// 9 Req ä�ü۽�
	//
	// 2Byte : �޽��� Size
	// Size  : ��ȭ����(�����ڵ�)
	//------------------------------------------------------------
	WCHAR szMsg[256] = { 0, };
	WORD wMsgSize;

	GetDlgItemText(g_hWndRoom, IDC_EDIT_CONTENT, szMsg, 255);
	wMsgSize = wcslen(szMsg) * sizeof(WCHAR);
	if (wMsgSize <= 0)
		return;

	mpReqChat(&stPacketHeader, &Packet, wMsgSize, szMsg);
	SendPacket(&stPacketHeader, &Packet);

	Addlistbox_Chat(g_szNickname, szMsg);

	HWND hEditBox = GetDlgItem(g_hWndRoom, IDC_EDIT_CONTENT);
	SetWindowText(hEditBox, L"");

}

void SendReq_RoomLeave()
{
	st_PACKET_HEADER stPacketHeader;
	CSerialBuffer Packet;
	//------------------------------------------------------------
	// 11 Req ������ 
	//
	// None
	//------------------------------------------------------------
	mpReqRoomLeave(&stPacketHeader, &Packet);
	SendPacket(&stPacketHeader, &Packet);
}

BYTE MakeCheckSum(CSerialBuffer * pPacket, DWORD dwType)
{
	//------------------------------------------------------
	//	checkSum - �� MsgType, Payload �� �� ����Ʈ ���ϱ� % 256
	//------------------------------------------------------
	int iSize = pPacket->GetUseSize();
	BYTE *pPtr = (BYTE*)pPacket->GetBufferPtr();
	int iChecksum = dwType;
	for (int iCnt = 0; iCnt < iSize; ++iCnt)
	{
		iChecksum += *pPtr;
		++pPtr;
	}
	return (BYTE)(iChecksum % 256);
}

void mpReqLogin(st_PACKET_HEADER * pHeader, CSerialBuffer *pPacket, WCHAR* szNickName)
{
	//------------------------------------------------------------
	// 1 Req �α���
	//
	//
	// WCHAR[15]	: �г��� (�����ڵ�)
	//------------------------------------------------------------
	pPacket->Clear();

	pPacket->Enqueue((char*)szNickName, dfNICK_MAX_LEN * sizeof(WCHAR));

	pHeader->byCode = (BYTE)dfPACKET_CODE;
	pHeader->byCheckSum = MakeCheckSum(pPacket, df_REQ_LOGIN);
	pHeader->wMsgType = (SHORT)df_REQ_LOGIN;
	pHeader->wPayloadSize = (SHORT)pPacket->GetUseSize();
}

void mpReqRoomList(st_PACKET_HEADER * pHeader, CSerialBuffer *pPacket)
{
	//------------------------------------------------------------
	// 3 Req ��ȭ�� ����Ʈ
	//
	//	None
	//------------------------------------------------------------
	pPacket->Clear();

	pHeader->byCode = (BYTE)dfPACKET_CODE;
	pHeader->byCheckSum = MakeCheckSum(pPacket, df_REQ_ROOM_LIST);
	pHeader->wMsgType = (SHORT)df_REQ_ROOM_LIST;
	pHeader->wPayloadSize = (SHORT)pPacket->GetUseSize();
}

void mpReqRoomCreate(st_PACKET_HEADER * pHeader, CSerialBuffer *pPacket, WORD wTitleSize, WCHAR* szRoomName)
{
	//------------------------------------------------------------
	// 5 Req ��ȭ�� ����
	//
	// 2Byte : ������ Size			�����ڵ� ���� ����Ʈ ���� (�� ����)
	// Size  : ������ (�����ڵ�)
	//------------------------------------------------------------
	pPacket->Clear();

	*pPacket << wTitleSize;
	pPacket->Enqueue((char*)szRoomName, wTitleSize * sizeof(WCHAR));

	pHeader->byCode = (BYTE)dfPACKET_CODE;
	pHeader->byCheckSum = MakeCheckSum(pPacket, df_REQ_ROOM_CREATE);
	pHeader->wMsgType = (SHORT)df_REQ_ROOM_CREATE;
	pHeader->wPayloadSize = (SHORT)pPacket->GetUseSize();
}

void mpReqRoomEnter(st_PACKET_HEADER * pHeader, CSerialBuffer *pPacket, DWORD dwRoomID)
{
	//------------------------------------------------------------
	// 7 Req ��ȭ�� ����
	//
	//	4Byte : �� No
	//------------------------------------------------------------

	pPacket->Clear();

	*pPacket << dwRoomID;

	pHeader->byCode = (BYTE)dfPACKET_CODE;
	pHeader->byCheckSum = MakeCheckSum(pPacket, df_REQ_ROOM_ENTER);
	pHeader->wMsgType = (SHORT)df_REQ_ROOM_ENTER;
	pHeader->wPayloadSize = (SHORT)pPacket->GetUseSize();
}

void mpReqChat(st_PACKET_HEADER * pHeader, CSerialBuffer *pPacket, WORD wMsgSize, WCHAR * szContents)
{
	//------------------------------------------------------------
	// 9 Req ä�ü۽�
	//
	// 2Byte : �޽��� Size
	// Size  : ��ȭ����(�����ڵ�)
	//------------------------------------------------------------
	pPacket->Clear();

	*pPacket << wMsgSize;
	pPacket->Enqueue((char*)szContents, wMsgSize * sizeof(WCHAR));

	pHeader->byCode = (BYTE)dfPACKET_CODE;
	pHeader->byCheckSum = MakeCheckSum(pPacket, df_REQ_CHAT);
	pHeader->wMsgType = (SHORT)df_REQ_CHAT;
	pHeader->wPayloadSize = (SHORT)pPacket->GetUseSize();
}

void mpReqRoomLeave(st_PACKET_HEADER * pHeader, CSerialBuffer *pPacket)
{
	//------------------------------------------------------------
	// 11 Req ������ 
	//
	// None
	//------------------------------------------------------------
	pPacket->Clear();

	pHeader->byCode = (BYTE)dfPACKET_CODE;
	pHeader->byCheckSum = MakeCheckSum(pPacket, df_REQ_ROOM_LEAVE);
	pHeader->wMsgType = (SHORT)df_REQ_ROOM_LEAVE;
	pHeader->wPayloadSize = (SHORT)pPacket->GetUseSize();
}

bool RecvEvent()
{
	WSABUF wsabuf[2];
	DWORD dwTransferred = 0;
	DWORD dwFlag = 0;
	int iBufCnt = 1;

	wsabuf[0].buf = g_RecvQ.GetWriteBufferPtr();
	wsabuf[0].len = g_RecvQ.GetUnbrokenEnqueueSize();
	if (wsabuf[0].len < g_RecvQ.GetFreeSize())
	{
		wsabuf[1].buf = g_RecvQ.GetBufferPtr();
		wsabuf[1].len = g_RecvQ.GetFreeSize() - wsabuf[0].len;
		++iBufCnt;
	}

	int iResult = WSARecv(g_Socket, wsabuf, iBufCnt, &dwTransferred, &dwFlag, NULL, NULL);
	if (iResult == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != WSAEWOULDBLOCK)
		{
			WCHAR szErr[100] = { 0, };
			swprintf_s(szErr, L"WSARecv() ErrorCode: %d", err);
			MessageBox(g_hWndLobby, szErr, L"Error", MB_OK | MB_ICONERROR);
			return false;
		}

	}

	g_RecvQ.MoveWritePos(dwTransferred);
	RecvComplete();
	return true;
}

int RecvComplete()
{
	WCHAR szErr[100] = { 0, };
	st_PACKET_HEADER stPacketHeader;
	while (1)
	{	
		// RecvQ�� Header���̸�ŭ �ִ��� Ȯ��
		int g_iRecvQSize = g_RecvQ.GetUseSize();
		if (g_iRecvQSize < sizeof(st_PACKET_HEADER))
			break;

		// Packet ���� Ȯ�� (Headerũ�� + Payload���� + EndCode ����)
		g_RecvQ.Peek((char*)&stPacketHeader, sizeof(st_PACKET_HEADER));
		if ((WORD)g_iRecvQSize < stPacketHeader.wPayloadSize + sizeof(st_PACKET_HEADER))
			break;
		// ���� ��Ŷ���κ��� Header ����
		g_RecvQ.MoveReadPos(sizeof(st_PACKET_HEADER));

		if (stPacketHeader.byCode != dfPACKET_CODE)
		{
			swprintf_s(szErr, L"RecvComplete() - PacketHeader Code Error");
			MessageBox(g_hWndLobby, szErr, L"Error", MB_OK | MB_ICONERROR);
			exit(1);
		}

		CSerialBuffer Packet;
		// Payload �κ� ��Ŷ ���۷� ����
		if (stPacketHeader.wPayloadSize != g_RecvQ.Dequeue(Packet.GetBufferPtr(), stPacketHeader.wPayloadSize))
		{
			swprintf_s(szErr, L"RecvComplete() - Payload Size Error");
			MessageBox(g_hWndLobby, szErr, L"Error", MB_OK | MB_ICONERROR);
			exit(1);
		}

		Packet.MoveWritePos(stPacketHeader.wPayloadSize);

		// Checksum Ȯ��
		BYTE byCheckSum = MakeCheckSum(&Packet, stPacketHeader.wMsgType);
		if (byCheckSum != stPacketHeader.byCheckSum)
		{
			swprintf_s(szErr, L"RecvComplete() - Checksum Error");
			MessageBox(g_hWndLobby, szErr, L"Error", MB_OK | MB_ICONERROR);
			exit(1);
		}

		// ��Ŷ ó�� �Լ� ȣ��
		OnRecv(stPacketHeader.wMsgType, &Packet);
	}
	return 0;
}

WCHAR * FindUser(int iClientID)
{
	map<DWORD, WCHAR*>::iterator iter = g_EnterRoomUserMap.find(iClientID);
	if (g_EnterRoomUserMap.end() == iter)
		return nullptr;
	return iter->second;
}

WCHAR * FindRoom(int iRoomID)
{
	map<DWORD, WCHAR*>::iterator iter = g_ChatRoomMap.find(iRoomID);
	if (g_ChatRoomMap.end() == iter)
		return nullptr;
	return iter->second;
}

bool DeleteUser(int iClientID)
{
	for (auto iter = g_EnterRoomUserMap.begin();iter != g_EnterRoomUserMap.end();)
	{
		if (iter->first == iClientID)
		{
			delete[] iter->second;
			iter = g_EnterRoomUserMap.erase(iter);
			return true;
		}
		else
			++iter;
	}
	return false;
}

bool DeleteRoom(int iRoomID)
{
	for (auto iter = g_ChatRoomMap.begin(); iter != g_ChatRoomMap.end();)
	{
		if (iter->first == iRoomID)
		{
			delete[] iter->second;
			iter = g_ChatRoomMap.erase(iter);
			return true;
		}
		else
			++iter;
	}
	return false;
}

void OnRecv(WORD wType, CSerialBuffer * Packet)
{
	switch (wType)
	{
	case df_RES_LOGIN:
		OnRecv_Login(Packet);
		break;
	case df_RES_ROOM_LIST:
		OnRecv_RoomList(Packet);
		break;
	case df_RES_ROOM_CREATE:
		OnRecv_RoomCreate(Packet);
		break;
	case df_RES_ROOM_ENTER:
		OnRecv_RoomEnter(Packet);
		break;
	case df_RES_CHAT:
		OnRecv_Chat(Packet);
		break;
	case df_RES_ROOM_LEAVE:
		OnRecv_RoomLeave(Packet);
		break;
	case df_RES_ROOM_DELETE:
		OnRecv_RoomDelete(Packet);
		break;
	case df_RES_USER_ENTER:
		OnRecv_UserEnter(Packet);
		break;
	}
}

void OnRecv_Login(CSerialBuffer * Packet)
{
	//------------------------------------------------------------
	// 2 Res �α���                              
	// 
	// 1Byte	: ��� (1:OK / 2:�ߺ��г��� / 3:������ʰ� / 4:��Ÿ����)
	// 4Byte	: ����� NO
	//------------------------------------------------------------
	BYTE byResult;
	
	// 1Byte	: ��� (1:OK / 2:�ߺ��г��� / 3:������ʰ� / 4:��Ÿ����)
	*Packet >> byResult;
	if (byResult != df_RESULT_LOGIN_OK)
	{
		WCHAR szErr[100] = { 0, };
		swprintf_s(szErr, L"Login Failed. Error:%d", byResult);
		MessageBox(g_hWndLobby, szErr, L"Error", MB_OK | MB_ICONERROR);
		exit(byResult);
	}

	// 4Byte	: ����� NO
	*Packet >> g_dwUserID;

	// ���̾�α� ���ڿ� ����
	HWND hStaticID;
	WCHAR szID[dfNICK_MAX_LEN];
	_itow_s(g_dwUserID, szID, 10);
	hStaticID = GetDlgItem(g_hWndLobby, IDC_STATICID);
	SetWindowText(hStaticID, szID);

	// �� ��� ��û
	SendReq_RoomList();
}

void OnRecv_RoomList(CSerialBuffer * Packet)
{
	//------------------------------------------------------------
	// 4 Res ��ȭ�� ����Ʈ
	//
	//  2Byte	: ����
	//  {
	//		4Byte : �� No
	//		2Byte : ���̸� byte size
	//		Size  : ���̸� (�����ڵ�)
	//
	//		1Byte : �����ο�		
	//		{
	//			WHCAR[15] : �г���
	//		}
	//	
	//	}
	//------------------------------------------------------------
	WORD	wRoomCnt;
	DWORD	dwChatRoomID;
	WORD	wTitleSize;
	BYTE	byRoomUser;
	WCHAR	szNickname[dfNICK_MAX_LEN];

	//  2Byte	: ����
	*Packet >> wRoomCnt;
	for (int iCnt = 0; iCnt < wRoomCnt; ++iCnt)
	{	
		// 4Byte : �� No
		*Packet >> dwChatRoomID;		

		// 2Byte : ������ Size
		*Packet >> wTitleSize;				

		WCHAR* szRoomTitle = new WCHAR[(wTitleSize + 1) / 2];
		// Size  : ������ (�����ڵ�)
		Packet->Dequeue((char*)szRoomTitle, wTitleSize); 
		szRoomTitle[wTitleSize / 2] = L'\0';
		Addlistbox_Room(szRoomTitle, dwChatRoomID);
		g_ChatRoomMap.insert(pair<DWORD, WCHAR*>(dwChatRoomID, szRoomTitle));

		//// 1Byte : �����ο�
		*Packet >> byRoomUser;					
		for (int iCnt = 0; iCnt < byRoomUser; ++iCnt)
		{
			// WCHAR[15] : �г���(�����ڵ�)
			Packet->Dequeue((char*)szNickname, sizeof(szNickname));
		}
		
	}
}

void OnRecv_RoomCreate(CSerialBuffer * Packet)
{
	//------------------------------------------------------------
	// 6 Res ��ȭ�� ���� (���÷�)
	//
	// 1Byte : ��� (1:OK / 2:���̸� �ߺ� / 3:�����ʰ� / 4:��Ÿ����)
	//
	//
	// 4Byte : �� No
	// 2Byte : ������ ����Ʈ Size
	// Size  : ������ (�����ڵ�)
	//------------------------------------------------------------
	BYTE byResult;
	DWORD dwChatRoomID;
	WORD wTitleSize;

	// 1Byte : ��� (1:OK / 2:���̸� �ߺ� / 3:�����ʰ� / 4:��Ÿ����)
	*Packet >> byResult;
	if (byResult != df_RESULT_ROOM_CREATE_OK)
	{
		WCHAR szErr[100] = { 0, };
		swprintf_s(szErr, L"Room Create Failed. Error:%d", byResult);
		MessageBox(g_hWndLobby, szErr, L"Error", MB_OK | MB_ICONERROR);
		return;
	}

	// 4Byte : �� No
	*Packet >> dwChatRoomID;	

	auto result = g_ChatRoomMap.insert(pair<DWORD, WCHAR*>(dwChatRoomID, nullptr));
	if (result.second)
	{
		// 2Byte : ������ ����Ʈ Size
		*Packet >> wTitleSize;

		// Size  : ������ (�����ڵ�)
		result.first->second = new WCHAR[(wTitleSize + 1) / sizeof(WCHAR)];
		result.first->second[wTitleSize / 2] = L'\0';
		Packet->Dequeue((char*)result.first->second, wTitleSize);

		Addlistbox_Room(result.first->second, dwChatRoomID);
	}
}

void OnRecv_RoomEnter(CSerialBuffer * Packet)
{
	WCHAR szErr[100] = { 0, };
	//------------------------------------------------------------
	// 8 Res ��ȭ�� ����
	//
	// 1Byte : ��� (1:OK / 2:��No ���� / 3:�ο��ʰ� / 4:��Ÿ����)
	//
	// OK �� ��쿡�� ���� ����
	//	{
	//		4Byte : �� No
	//		2Byte : ������ Size
	//		Size  : ������ (�����ڵ�)
	//
	//		1Byte : �����ο�
	//		{
	//			WCHAR[15] : �г���(�����ڵ�)
	//			4Byte     : �����No
	//		}
	//	}
	//------------------------------------------------------------
	BYTE byResult;
	WORD wTitleSize;
	BYTE byRoomUser;
	DWORD dwUserID;
	WCHAR szRoomTitle[256];

	// 1Byte : ��� (1:OK / 2:��No ���� / 3:�ο��ʰ� / 4:��Ÿ����)
	*Packet >> byResult;
	if (byResult != df_RESULT_ROOM_ENTER_OK)
	{
		swprintf_s(szErr, L"Room Enter Failed. Error:%d", byResult);
		MessageBox(g_hWndLobby, szErr, L"Error", MB_OK | MB_ICONERROR);
		exit(byResult);
	}

	// �� ���̾�α� ����
	CreateRoomDlg();

	// 4Byte : �� No
	*Packet >> g_dwEnterRoomID;	

	// 2Byte : ������ Size
	*Packet >> wTitleSize;

	// Size  : ������ (�����ڵ�)
	Packet->Dequeue((char*)szRoomTitle, wTitleSize);
	szRoomTitle[wTitleSize / 2] = L'\0';

	// ä�ù� �̸� ����
	SetDlgItemText(g_hWndRoom, IDC_STATICTITLE, szRoomTitle);
	
	// 1Byte : �����ο�
	*Packet >> byRoomUser;

	
	for (int iCnt = 0; iCnt < byRoomUser; ++iCnt)
	{
		// WCHAR[15] : �г���(�����ڵ�)
		WCHAR szNickname[dfNICK_MAX_LEN];
		Packet->Dequeue((char*)szNickname, sizeof(szNickname));

		// 4Byte : �����No
		*Packet >> dwUserID;

		auto result = g_EnterRoomUserMap.insert(pair<DWORD, WCHAR*>(dwUserID, nullptr));
		if (result.second)
		{
			result.first->second = new WCHAR[dfNICK_MAX_LEN];
			result.first->second[dfNICK_MAX_LEN - 1] = L'\0';
			wcscpy_s(result.first->second, sizeof(szNickname), szNickname);

			Addlistbox_ChatUser(result.first->second, dwUserID);
		}
	}
}

void OnRecv_Chat(CSerialBuffer * Packet)
{
	//------------------------------------------------------------
	// 10 Res ä�ü��� (�ƹ����� �� �� ����)  (���� ���� ����)
	//
	// 4Byte : �۽��� No
	//
	// 2Byte : �޽��� Size
	// Size  : ��ȭ����(�����ڵ�)
	//------------------------------------------------------------
	DWORD SendID;
	WORD wMsgSize;

	// 4Byte : �۽��� No
	*Packet >> SendID;

	// 2Byte : �޽��� Size
	*Packet >> wMsgSize;

	// Size  : ��ȭ����(�����ڵ�)
	WCHAR* szMsg = new WCHAR[(wMsgSize + 1) / 2];
	Packet->Dequeue((char*)szMsg, wMsgSize);
	szMsg[wMsgSize / 2] = L'\0';

	Addlistbox_Chat(FindUser(SendID), szMsg);

	delete[] szMsg;
}

void OnRecv_RoomLeave(CSerialBuffer * Packet)
{
	//------------------------------------------------------------
	// 12 Res ������ (����)
	//
	// 4Byte : ����� No
	//------------------------------------------------------------
	DWORD dwLeaveID;
	*Packet >> dwLeaveID;

	
	Deletelistbox_ChatUser(FindUser(dwLeaveID));
	DeleteUser(dwLeaveID);
}


void OnRecv_RoomDelete(mylib::CSerialBuffer * Packet)
{
	//------------------------------------------------------------
	// 13 Res ����� (����)
	//
	// 4Byte : �� No
	//------------------------------------------------------------
	DWORD dwDeleteRoomID;
	*Packet >> dwDeleteRoomID;

	Deletelistbox_Room(FindRoom(dwDeleteRoomID));
	DeleteRoom(dwDeleteRoomID);
}

void OnRecv_UserEnter(mylib::CSerialBuffer * Packet)
{
	//------------------------------------------------------------
	// TODO:14 Res Ÿ ����� ���� (����)
	//
	// WCHAR[15] : �г���(�����ڵ�)
	// 4Byte : ����� No
	//------------------------------------------------------------
	DWORD dwEnterID;

	// WCHAR[15] : �г���(�����ڵ�)
	WCHAR szNickname[dfNICK_MAX_LEN];
	Packet->Dequeue((char*)szNickname, sizeof(szNickname));

	// 4Byte : ����� No
	*Packet >> dwEnterID;

	auto result = g_EnterRoomUserMap.insert(pair<DWORD, WCHAR*>(dwEnterID, nullptr));
	if (result.second)
	{
		result.first->second = new WCHAR[dfNICK_MAX_LEN];
		result.first->second[dfNICK_MAX_LEN - 1] = L'\0';
		wcscpy_s(result.first->second, sizeof(szNickname), szNickname);

		Addlistbox_ChatUser(szNickname, dwEnterID);
	}
}
