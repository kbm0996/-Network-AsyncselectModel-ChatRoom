#include "NetworkProc.h"
#include "CSystemLog.h"
using namespace mylib;
using namespace std;

map<DWORD, st_CLIENT*>		g_ClientMap;
map<DWORD, st_CHATROOM*>	g_ChatRoomMap;
st_CHATROOM*	g_pLobby;
SOCKET			g_ListenSocket = INVALID_SOCKET;
// ���� ����Ű, �� ����Ű. �Ҵ�ø��� ++
DWORD			g_dwClientID;
DWORD			g_dwRoomID;
// ����Ű �κ��� ���� ���Ӽ��������� ��ó�� ����� �� ����. ���� ������ ������ ��ȣ�� �ο��ž��ϹǷ� DB���� ������ ���� ���� ������ ���� �����ȴ�


bool NetworkInit()
{
	int	err;
	LOG_SET(LOG_CONSOLE | LOG_FILE, LOG_DEBUG);

	WSADATA		wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		err = WSAGetLastError();
		LOG(L"SYSTEM", LOG_ERROR, L"WSAStartup() ErrorCode: %d", err);
		return false;
	}

	g_ListenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (g_ListenSocket == INVALID_SOCKET)
	{
		err = WSAGetLastError();
		LOG(L"SYSTEM", LOG_ERROR, L"socket() ErrorCode: %d", err);
		return false;
	}

	BOOL bOptval = TRUE;
	setsockopt(g_ListenSocket, IPPROTO_TCP, TCP_NODELAY, (char *)&bOptval, sizeof(bOptval));

	SOCKADDR_IN serveraddr;
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(dfNETWORK_PORT);
	InetPton(AF_INET, L"0.0.0.0", &serveraddr.sin_addr);
	if (bind(g_ListenSocket, (SOCKADDR *)&serveraddr, sizeof(serveraddr)) == SOCKET_ERROR)
	{
		err = WSAGetLastError();
		LOG(L"SYSTEM", LOG_ERROR, L"bind() ErrorCode: %d", err);
		return false;
	}

	if (listen(g_ListenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		err = WSAGetLastError();
		LOG(L"SYSTEM", LOG_ERROR, L"listen() ErrorCode: %d", err);
		return false;
	}

	// Initilize Lobby
	g_pLobby = new st_CHATROOM();
	g_pLobby->dwRoomID = 0;
	wcscpy_s(g_pLobby->szTitle, dfNICK_MAX_LEN, L"Lobby");
	g_pLobby->UserList.clear();
	///g_ChatRoomMap.insert(pair<DWORD, st_CHATROOM*>(g_pLobby->dwRoomID, g_pLobby));
	
	LOG(L"SYSTEM", LOG_SYSTM, L"Server Start");
	return true;
}

void NetworkClose()
{
	for (map<DWORD, st_CLIENT*>::iterator Clientiter = g_ClientMap.begin(); Clientiter != g_ClientMap.end(); ++Clientiter)
	{
		Disconnect(Clientiter->first);
	}

	for (map<DWORD, st_CHATROOM*>::iterator Roomiter = g_ChatRoomMap.begin(); Roomiter != g_ChatRoomMap.end(); ++Roomiter)
	{
		// room delete
		delete Roomiter->second;
	}

	delete g_pLobby;

	closesocket(g_ListenSocket);
	WSACleanup();
}

void NetworkProc()
{
	st_CLIENT * pClient;
	DWORD	UserTable_ID[FD_SETSIZE];		// FD_SET�� ��ϵ� UserID ����
	SOCKET	UserTable_SOCKET[FD_SETSIZE];	// FD_SET�� ��ϵ� ���� ����
	int		iSockCnt = 0;
	//-----------------------------------------------------
	// FD_SET�� FD_SETSIZE��ŭ���� ���� �˻� ����
	//-----------------------------------------------------
	FD_SET ReadSet;
	FD_SET WriteSet;
	FD_ZERO(&ReadSet);
	FD_ZERO(&WriteSet);
	FillMemory(UserTable_ID, sizeof(DWORD) * FD_SETSIZE, 0);
	FillMemory(UserTable_SOCKET, sizeof(SOCKET)*FD_SETSIZE, 0);

	// Listen Socket Setting
	FD_SET(g_ListenSocket, &ReadSet);
	UserTable_ID[iSockCnt] = 0;
	UserTable_SOCKET[iSockCnt] = g_ListenSocket;
	++iSockCnt;

	//-----------------------------------------------------
	// ListenSocket �� ��� Ŭ���̾�Ʈ�� ���� Socket �˻�
	//-----------------------------------------------------
	for (map<DWORD, st_CLIENT*>::iterator iter = g_ClientMap.begin(); iter != g_ClientMap.end();)
	{
		pClient = iter->second;
		++iter;	// SelectSocket �Լ� ���ο��� ClientMap�� �����ϴ� ��찡 �־ �̸� ����
		//-----------------------------------------------------
		// �ش� Ŭ���̾�Ʈ ReadSet ���
		// SendQ�� �����Ͱ� �ִٸ� WriteSet ���
		//-----------------------------------------------------
		UserTable_ID[iSockCnt] = pClient->dwClientID;
		UserTable_SOCKET[iSockCnt] = pClient->Sock;

		// ReadSet ���
		FD_SET(pClient->Sock, &ReadSet);

		// WriteSet ���
		if (pClient->SendQ.GetUseSize() > 0)
			FD_SET(pClient->Sock, &WriteSet);

		++iSockCnt;
		//-----------------------------------------------------
		// select �ִ�ġ ����, ������� ���̺� ������ select ȣ�� �� ����
		//-----------------------------------------------------
		if (FD_SETSIZE <= iSockCnt)
		{
			SelectSocket(UserTable_ID, UserTable_SOCKET, &ReadSet, &WriteSet);

			FD_ZERO(&ReadSet);
			FD_ZERO(&WriteSet);

			FillMemory(UserTable_ID, sizeof(DWORD) * FD_SETSIZE, -1);
			FillMemory(UserTable_SOCKET, sizeof(SOCKET)*FD_SETSIZE, 0);
			iSockCnt = 0;
		}
	}
	//-----------------------------------------------------
	// ��ü Ŭ���̾�Ʈ for �� ���� �� iSockCnt ��ġ�� �ִٸ�
	// �߰������� ������ Select ȣ���� ���ش�
	//-----------------------------------------------------
	if (iSockCnt > 0)
	{
		SelectSocket(UserTable_ID, UserTable_SOCKET, &ReadSet, &WriteSet);
	}
}

void SelectSocket(DWORD *dwpTableID, SOCKET* pTableSocket, FD_SET *pReadSet, FD_SET *pWriteSet)
{
	////////////////////////////////////////////////////////////////////
	// select() �Լ��� ����Ʈ �Ѱ� ���� FD_SETSIZE(�⺻ 64����)�� ���� �ʾҴٸ� 
	// TimeOut �ð�(Select ��� �ð�)�� 0���� ���� �ʿ䰡 ����.
	//
	// �׷���, FD_SETSIZE(�⺻ 64����)�� �Ѿ��ٸ� Select�� �� �� �̻� �ؾ��Ѵ�.
	// �� ��� TimeOut �ð��� 0���� �ؾ��ϴµ� 
	// �� ������ ù��° select���� ���� �ɷ� �ι�° select�� �������� �ʱ� �����̴�.
	////////////////////////////////////////////////////////////////////
	//-----------------------------------------------------
	// select �Լ��� ���ð� ����
	//-----------------------------------------------------
	timeval Time;
	Time.tv_sec = 0;
	Time.tv_usec = 0;

	//-----------------------------------------------------
	// ������ ��û�� ���� �������� Ŭ���̾�Ʈ���� �޽��� �۽� �˻�
	//-----------------------------------------------------
	//  select() �Լ����� block ���¿� �ִٰ� ��û�� ������,
	// select() �Լ��� ��뿡�� ��û�� ���Դ��� ������ �ϰ� �� ������ return�մϴ�.
	// select() �Լ��� return �Ǵ� ���� flag�� �� ���ϸ� WriteSet, ReadSet�� �����ְ� �˴ϴ�.
	// �׸��� ���� ���� ������ ���� flag ǥ�ø� �� ���ϵ��� checking�ϴ� ���Դϴ�.
	int retval = select(0, pReadSet, pWriteSet, NULL, &Time);
	// return ���� 0 �̻��̶�� �������� �����Ͱ� �ִٴ� �ǹ�
	if (retval > 0)
	{
		//-----------------------------------------------------
		// TableSocket�� ���鼭 � ���Ͽ� ������ �־����� Ȯ��
		//-----------------------------------------------------
		for (int iCnt = 0; iCnt < FD_SETSIZE; ++iCnt)
		{
			if (pTableSocket[iCnt] == INVALID_SOCKET) // Scoket �ڵ� ��ȯ ���н� INVALID_SOCKET�� ��ȯ��
				continue;

			//-----------------------------------------------------
			// Write üũ
			//-----------------------------------------------------
			if (FD_ISSET(pTableSocket[iCnt], pWriteSet))
			{
				SendEvent(dwpTableID[iCnt]);
			}

			//-----------------------------------------------------
			// Read üũ
			//-----------------------------------------------------
			if (FD_ISSET(pTableSocket[iCnt], pReadSet))
			{
				//-----------------------------------------------------
				// ListenSocket�� ������ ���� �뵵�̹Ƿ� ���� ó�� 
				//-----------------------------------------------------
				if (dwpTableID[iCnt] == 0) // UserID�� 0�̸� ListenSocket
				{
					AcceptProc();
				}
				else
				{
					RecvEvent(dwpTableID[iCnt]);
				}
			}
		}
	}
	else if (retval == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		LOG(L"SYSTEM", LOG_ERROR, L"# select() #	ErrorCode: %d", err);
	}
}

void AcceptProc()
{
	SOCKADDR_IN clientaddr;
	SOCKET client_sock;
	int addrlen = sizeof(clientaddr);
	client_sock = accept(g_ListenSocket, (SOCKADDR *)&clientaddr, &addrlen);
	if (client_sock == INVALID_SOCKET)
	{
		int err = WSAGetLastError();
		LOG(L"SYSTEM", LOG_ERROR, L"# accept() #	ErrorCode: %d", err);
		return;
	}

	/////////////////////////////////////////////////////////////////////////
	st_CLIENT* stNewClient = new st_CLIENT();
	stNewClient->dwClientID = ++g_dwClientID;
	stNewClient->Sock = client_sock;
	ZeroMemory(stNewClient->Nickname, dfNICK_MAX_LEN);
	g_ClientMap.insert(pair<DWORD, st_CLIENT*>(stNewClient->dwClientID, stNewClient));
	/////////////////////////////////////////////////////////////////////////

	DWORD dwAddrBufSize = sizeof(stNewClient->szIP);
	WSAAddressToString((SOCKADDR*)&clientaddr, sizeof(SOCKADDR), NULL, stNewClient->szIP, &dwAddrBufSize);

	LOG(L"SYSTEM", LOG_SYSTM, L"Connect Try IP:%s / UserNo:%d", stNewClient->szIP, g_dwClientID);
}

bool RecvEvent(DWORD dwClientID)
{
	st_CLIENT* pClient = FindClient(dwClientID);
	if (pClient == nullptr)
	{
		LOG(L"SYSTEM", LOG_ERROR, L"RecvEvent() - FindClient() Error. UserNo:%d", dwClientID);
		return false;
	}
	
	WSABUF wsabuf[2];
	DWORD dwTransferred = 0;
	DWORD dwFlag = 0;
	int iBufCnt = 1;
	wsabuf[0].buf = pClient->RecvQ.GetWriteBufferPtr();
	wsabuf[0].len = pClient->RecvQ.GetUnbrokenEnqueueSize();
	if (wsabuf[0].len < pClient->RecvQ.GetFreeSize())
	{
		wsabuf[1].buf = pClient->RecvQ.GetBufferPtr();
		wsabuf[1].len = pClient->RecvQ.GetFreeSize() - wsabuf[0].len;
		++iBufCnt;
	}

	if (WSARecv(pClient->Sock, wsabuf, iBufCnt, &dwTransferred, &dwFlag, NULL, NULL) == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != ERROR_IO_PENDING) // ERROR_IO_PENDING : �񵿱� ������� �ϰ� �ִٴ� �ǹ�
		{
			if (err != 10038 && err != 10053 && err != 10054 && err != 10058)
			{
				// WSAENOTSOCK(10038)
				//	�ǹ� : ������ �ƴ� �׸� ���� ���� �۾��� �õ��߽��ϴ�.
				//	���� : ���� �ڵ� �Ű� ������ �ùٸ� ������ �������� �ʾҰų� fd_set�� ����� �ùٸ��� �ʽ��ϴ�.
				// WSAECONNABORTED(10053)
				//	�ǹ�: ����Ʈ����� ������ �ߴ��߽��ϴ�.
				//	���� : ȣ��Ʈ ��ǻ���� ����Ʈ��� ������ ������ �����߽��ϴ�. 
				//        ������ ���� �ð��� �ʰ��Ǿ��ų� �������� ������ �߻��߱� ������ �� �ֽ��ϴ�.
				// WSAECONNRESET(10054)
				//  �ǹ� : �Ǿ ������ �ٽ� �����߽��ϴ�.
				//	���� : ���� ȣ��Ʈ�� ���� ���� ������ ������ ���������ϴ�.
				//       ���� ȣ��Ʈ�� ���ڱ� �����ǰų� �ٽ� ���۵ǰų� �ϵ� ���Ḧ ����ϴ� ��쿡 �߻�
				// WSAESHUTDOWN(10058)
				//	�ǹ�: ������ ����� �Ŀ��� ������ �� �����ϴ�.
				//	���� : ������ shutdown���� �ش� ������ ������ �̹� ����Ǿ����Ƿ� ������ ���� �Ǵ� ���� ��û�� ������ �ʽ��ϴ�.
				LOG(L"SYSTEM", LOG_ERROR, L"WSARecv() UserNo: %d, ErrorCode: %d", dwClientID, err);
			}
			Disconnect(dwClientID);
			return false;
		}
		
	}

	pClient->RecvQ.MoveWritePos(dwTransferred);
	RecvComplete(pClient);
	return true;
}

bool SendEvent(DWORD dwClientID)
{
	st_CLIENT *pClient = FindClient(dwClientID);
	if (pClient == nullptr)
	{
		LOG(L"SYSTEM", LOG_ERROR, L"SendEvent() - FindClient() Error. UserNo:%d", dwClientID);
		return false;
	}

	WSABUF wsabuf[2];
	DWORD dwTransferred = 0;
	DWORD dwFlag = 0;
	int iBufCnt = 1;
	wsabuf[0].buf = pClient->SendQ.GetReadBufferPtr();
	wsabuf[0].len = pClient->SendQ.GetUnbrokenDequeueSize();
	if (wsabuf[0].len < pClient->SendQ.GetUseSize())
	{
		wsabuf[1].buf = pClient->SendQ.GetBufferPtr();
		wsabuf[1].len = pClient->SendQ.GetUseSize() - wsabuf[0].len;
		++iBufCnt;
	}

	if (WSASend(pClient->Sock, wsabuf, iBufCnt, &dwTransferred, 0, NULL, NULL) == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != ERROR_IO_PENDING)
		{
			if (err != 10038 && err != 10053 && err != 10054 && err != 10058)
			{
				// WSAENOTSOCK(10038)
				//	�ǹ� : ������ �ƴ� �׸� ���� ���� �۾��� �õ��߽��ϴ�.
				//	���� : ���� �ڵ� �Ű� ������ �ùٸ� ������ �������� �ʾҰų� fd_set�� ����� �ùٸ��� �ʽ��ϴ�.
				// WSAECONNABORTED(10053)
				//	�ǹ�: ����Ʈ����� ������ �ߴ��߽��ϴ�.
				//	���� : ȣ��Ʈ ��ǻ���� ����Ʈ��� ������ ������ �����߽��ϴ�. 
				//        ������ ���� �ð��� �ʰ��Ǿ��ų� �������� ������ �߻��߱� ������ �� �ֽ��ϴ�.
				// WSAECONNRESET(10054)
				//  �ǹ� : �Ǿ ������ �ٽ� �����߽��ϴ�.
				//	���� : ���� ȣ��Ʈ�� ���� ���� ������ ������ ���������ϴ�.
				//       ���� ȣ��Ʈ�� ���ڱ� �����ǰų� �ٽ� ���۵ǰų� �ϵ� ���Ḧ ����ϴ� ��쿡 �߻�
				// WSAESHUTDOWN(10058)
				//	�ǹ�: ������ ����� �Ŀ��� ������ �� �����ϴ�.
				//	���� : ������ shutdown���� �ش� ������ ������ �̹� ����Ǿ����Ƿ� ������ ���� �Ǵ� ���� ��û�� ������ �ʽ��ϴ�.
				LOG(L"SYSTEM", LOG_ERROR, L"WSASend() UserNo: %d, ErrorCode: %d", dwClientID, err);
			}
			Disconnect(pClient->dwClientID);
			return false;
		}
	}

	//-----------------------------------------------------
	// Send Complate
	// ��Ŷ ������ �Ϸ�ƴٴ� �ǹ̴� �ƴ�. ���� ���ۿ� ���縦 �Ϸ��ߴٴ� �ǹ�
	// �۽� ť���� ���´� ������ ����
	//-----------------------------------------------------
	pClient->SendQ.MoveReadPos(dwTransferred);
	return true;

	/* send() */
	
	//int iSendSize = pClient->SendQ.GetUseSize();
	//if (iSendSize < sizeof(st_PACKET_HEADER))
	//	return true;
	//while (iSendSize != 0)
	//{
	//	int iResult = send(pClient->Sock, pClient->SendQ.GetReadBufferPtr(), pClient->SendQ.GetUnbrokenDequeueSize(), 0);
	//	if (iResult == SOCKET_ERROR)
	//	{
	//		int err = WSAGetLastError();
	//		if (err == WSAEWOULDBLOCK)
	//		{
	//			return true;
	//		}
	//		if (err != 10054)
	//		{
	//			LOG(L"SYSTEM", LOG_ERROR, L"# send() #	ClientID: %d, ErrorCode: %d", pClient->dwClientID, err);
	//		}
	//		Disconnect(dwClientID);
	//		return false;
	//	}
	//	else if (iResult == 0)
	//	{
	//		LOG(L"SYSTEM", LOG_DEBUG, L"# send() #	bytes 0, ClientID: %d", pClient->dwClientID);
	//	}

	//	if (iResult > iSendSize)
	//	{
	//		//-----------------------------------------------------
	//		// ���� ������� �� ũ�ٸ� ����
	//		// ����� �ȵǴ� ��Ȳ������ ���� �̷� ��찡 ���� �� �ִ�
	//		//-----------------------------------------------------
	//		LOG(L"SYSTEM", LOG_ERROR, L"# send() #	send size error, ClientID: %d", pClient->dwClientID);
	//		return false;
	//	}

	//	//-----------------------------------------------------
	//	// Send Complate
	//	// ��Ŷ ������ �Ϸ�ƴٴ� �ǹ̴� �ƴ�. ���� ���ۿ� ���縦 �Ϸ��ߴٴ� �ǹ�
	//	// �۽� ť���� Peek���� ���´� ������ ����
	//	//-----------------------------------------------------
	//	pClient->SendQ.MoveReadPos(iResult);
	//	iSendSize -= iResult;
	//}
	//return true;
}

void SendPacket_Unicast(st_CLIENT * pClient, st_PACKET_HEADER * pHeader, CSerialBuffer * pPacket)
{
	if (pClient == nullptr)
		return;
	pClient->SendQ.Enqueue((char*)pHeader, sizeof(st_PACKET_HEADER));
	pClient->SendQ.Enqueue((char*)pPacket->GetBufferPtr(), pPacket->GetUseSize());
}

void SendPacket_Broadcast(st_PACKET_HEADER * pHeader, CSerialBuffer * pPacket)
{
	for (map<DWORD, st_CLIENT*>::iterator Clientiter = g_ClientMap.begin(); Clientiter != g_ClientMap.end(); ++Clientiter)
	{
		SendPacket_Unicast((*Clientiter).second, pHeader, pPacket);
	}
}

void SendPacket_Broadcast(st_CHATROOM * pRoom, st_CLIENT * pExceptClient, st_PACKET_HEADER * pHeader, CSerialBuffer * pPacket)
{
	for (list<DWORD>::iterator Useriter = pRoom->UserList.begin(); Useriter != pRoom->UserList.end(); ++Useriter)
	{
		st_CLIENT* pDestClient = FindClient(*Useriter);
		if (pDestClient == nullptr)
		{
			LOG(L"SYSTEM", LOG_ERROR, L"SendPacket_Broadcast() - FindClient() Error. UserNo:%d", *Useriter);
			continue;
		}
		if (pDestClient == pExceptClient)
			continue;
		SendPacket_Unicast(pDestClient, pHeader, pPacket);
	}
}

void OnRecv(st_CLIENT * pClient, WORD wType, CSerialBuffer * Packet)
{
	LOG(L"SYSTEM", LOG_DEBUG, L"OnRecv UserNo:%d, Type:%d", pClient->dwClientID, wType);
	switch (wType)
	{
	case df_REQ_LOGIN:
		OnRecv_Login(pClient, Packet);
		break;
	case df_REQ_ROOM_LIST:
		OnRecv_RoomList(pClient, Packet);
		break;
	case df_REQ_ROOM_CREATE:
		OnRecv_RoomCreate(pClient, Packet);
		break;
	case df_REQ_ROOM_ENTER:
		OnRecv_RoomEnter(pClient, Packet);
		break;
	case df_REQ_CHAT:
		OnRecv_Chat(pClient, Packet);
		break;
	case df_REQ_ROOM_LEAVE:
		OnRecv_RoomLeave(pClient, Packet);
		break;
	default:
		LOG(L"SYSTEM", LOG_ERROR, L"Unknown Packet ClientID: %d, PacketType: %d", pClient->dwClientID, wType);
		Disconnect(pClient->dwClientID);
		break;
	}
}

bool OnRecv_Login(st_CLIENT * pClient, CSerialBuffer * Packet)
{
	st_PACKET_HEADER stPacketHeader;
	WCHAR	szNickName[dfNICK_MAX_LEN] = { 0, };
	BYTE	retval = df_RESULT_LOGIN_ETC;

	//------------------------------------------------------------
	// 1 Req �α���
	//
	//
	// WCHAR[15]	: �г��� (�����ڵ�)
	//------------------------------------------------------------
	// 256 �̻��� ������ ��� ����ó��
	Packet->Dequeue((char*)szNickName, dfNICK_MAX_LEN);

	// �ߺ� �˻�
	for (map<DWORD, st_CLIENT*>::iterator Clientiter = g_ClientMap.begin(); Clientiter != g_ClientMap.end(); ++Clientiter)
	{
		if (wcscmp((*Clientiter).second->Nickname, szNickName) == 0)
		{
			LOG(L"SYSTEM", LOG_DEBUG, L"# OnRecv_Login() # duplicated nickname  ClientID: %d", pClient->dwClientID);
			retval = df_RESULT_LOGIN_DNICK;
			mpResLogin(&stPacketHeader, Packet, retval, pClient->dwClientID);
			break;
		}
	}

	if (retval != df_RESULT_LOGIN_DNICK)
	{
		wcscpy_s(pClient->Nickname, dfNICK_MAX_LEN, szNickName);

		// �κ� ����
		pClient->dwEnterRoomID = g_pLobby->dwRoomID;
		g_pLobby->UserList.push_front(pClient->dwClientID);

		retval = df_RESULT_LOGIN_OK;

		LOG(L"SYSTEM", LOG_SYSTM, L"Connect Success IP:%s / UserNo:%d", pClient->szIP, g_dwClientID);
	}

	mpResLogin(&stPacketHeader, Packet, retval, pClient->dwClientID);
	SendPacket_Unicast(pClient, &stPacketHeader, Packet);

	if (retval != df_RESULT_LOGIN_OK)
		return false;
	return true;
}

bool OnRecv_RoomList(st_CLIENT * pClient, CSerialBuffer * Packet)
{
	st_PACKET_HEADER stPacketHeader;
	//------------------------------------------------------------
	// 3 Req ��ȭ�� ����Ʈ
	//
	//	None
	//------------------------------------------------------------

	// ����� Ŭ���̾�Ʈ�� ����
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
	mpResRoomList(&stPacketHeader, Packet, (WORD)g_ChatRoomMap.size());
	SendPacket_Unicast(pClient, &stPacketHeader, Packet);
	return true;
}

bool OnRecv_RoomCreate(st_CLIENT * pClient, CSerialBuffer * Packet)
{
	st_PACKET_HEADER stPacketHeader;
	WCHAR	szRoomTitle[256] = { 0, };
	WORD	wTitleSize;
	st_CHATROOM *pRoom = nullptr;
	BYTE retval = df_RESULT_ROOM_CREATE_ETC;

	//------------------------------------------------------------
	// 5 Req ��ȭ�� ����
	//
	// 2Byte : ������ Size			�����ڵ� ���� ����Ʈ ���� (�� ����)
	// Size  : ������ (�����ڵ�)
	//------------------------------------------------------------
	// 256 �̻��� ������ ��� ����ó��
	*Packet >> wTitleSize;
	if (wTitleSize > 0 && wTitleSize < 256)
	{
		Packet->Dequeue((char*)szRoomTitle, wTitleSize);

		// �ߺ� �˻�
		for (map<DWORD, st_CHATROOM*>::iterator iter = g_ChatRoomMap.begin(); iter != g_ChatRoomMap.end(); ++iter)
		{
			if (wcscmp(szRoomTitle, (*iter).second->szTitle) == 0)
			{
				LOG(L"SYSTEM", LOG_DEBUG, L"# OnRecv_RoomCreate() # duplicated room name %s", szRoomTitle);
				retval = df_RESULT_ROOM_CREATE_DNICK;
				break;
			}
		}

		if (retval != df_RESULT_ROOM_CREATE_DNICK)
		{
			// �� ���� �۾�
			pRoom = new st_CHATROOM;

			pRoom->dwRoomID = ++g_dwRoomID;
			wcscpy_s(pRoom->szTitle, szRoomTitle);
			g_ChatRoomMap.insert(pair<DWORD, st_CHATROOM*>(pRoom->dwRoomID, pRoom));
			retval = df_RESULT_ROOM_CREATE_OK;
			LOG(L"SYSTEM", LOG_SYSTM, L"Room %s Create by UserNo:%d (TotalRoom:%d) ", pRoom->szTitle, pClient->dwClientID, g_ChatRoomMap.size());
		}
	}

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
	// �� ���� ����� Ŭ���̾�Ʈ�� ����
	mpResRoomCreate(&stPacketHeader, Packet, retval, pRoom);

	// ä�ù� ���� ����� ������ ��� ��� �������� ����
	if (retval != df_RESULT_ROOM_CREATE_OK)
	{
		SendPacket_Unicast(pClient, &stPacketHeader, Packet);
		return false;
	}
	SendPacket_Broadcast(&stPacketHeader, Packet);
	return true;
}

bool OnRecv_RoomEnter(st_CLIENT * pClient, CSerialBuffer * Packet)
{
	st_PACKET_HEADER stPacketHeader;
	DWORD dwRoomID = 0;
	int retval = df_RESULT_ROOM_ENTER_ETC;

	//------------------------------------------------------------
	// 7 Req ��ȭ�� ����
	//
	//	4Byte : �� No
	//------------------------------------------------------------
	*Packet >> dwRoomID;

	// �� ã��
	st_CHATROOM* pRoom = FindChatRoom(dwRoomID);
	if (pRoom == nullptr)
	{
		LOG(L"SYSTEM", LOG_ERROR, L"OnRecv_RoomEnter() - FindChatRoom() Error. RoomNo:%d", dwRoomID);
		retval = df_RESULT_ROOM_ENTER_NOT;
	}
	// �κ� �־��°�
	else if (pClient->dwEnterRoomID == g_pLobby->dwRoomID)
	{
		retval = df_RESULT_ROOM_ENTER_OK;
	}

	if (retval == df_RESULT_ROOM_ENTER_OK)
	{
		// �κ� Ż��
		g_pLobby->UserList.remove(pClient->dwClientID);

		// �� ����
		pRoom->UserList.push_front(pClient->dwClientID);
		pClient->dwEnterRoomID = pRoom->dwRoomID;
		
		// �� ���� �α�
		LOG(L"SYSTEM", LOG_SYSTM, L"Room %s Enter UserNo:%d", pRoom->szTitle, pClient->dwClientID);
	}

	// �� ���� ��� ����
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
	//------------------------------------------------------------
	// 14 Res Ÿ ����� ���� (����)
	//
	// WCHAR[15] : �г���(�����ڵ�)
	// 4Byte : ����� No
	//------------------------------------------------------------
	// �� ���� ����� Ŭ���̾�Ʈ�� ����
	mpResRoomEnter(&stPacketHeader, Packet, retval, pRoom);
	SendPacket_Unicast(pClient, &stPacketHeader, Packet);
	if (pRoom != nullptr)
	{
		if (pRoom->UserList.size() != 0)
		{
			mpResUserEnter(&stPacketHeader, Packet, pClient->Nickname, pClient->dwClientID);
			SendPacket_Broadcast(pRoom, pClient, &stPacketHeader, Packet);
		}
	}
	return true;
}

bool OnRecv_Chat(st_CLIENT * pClient, CSerialBuffer * Packet)
{
	st_PACKET_HEADER stPacketHeader;
	WORD	wMsgSize = 0;
	//------------------------------------------------------------
	// 9 Req ä�ü۽�
	//
	// 2Byte : �޽��� Size
	// Size  : ��ȭ����(�����ڵ�)
	//------------------------------------------------------------
	// 2Byte : �޽��� Size
	*Packet >> wMsgSize;

	// Size  : ��ȭ����(�����ڵ�)
	WCHAR* szMsg = new WCHAR[(wMsgSize + 1) / 2];
	Packet->Dequeue((char*)szMsg, wMsgSize);

	st_CHATROOM* pRoom = FindChatRoom(pClient->dwEnterRoomID);
	if (pRoom == nullptr)
	{
		LOG(L"SYSTEM", LOG_ERROR, L"OnRecv_Chat() - FindChatRoom() Error. RoomNo:%d", pClient->dwEnterRoomID);
		delete szMsg;
		Disconnect(pClient->dwClientID);
		return false;
	}

	//------------------------------------------------------------
	// 10 Res ä�ü��� (�ƹ����� �� �� ����)  (������ ���� ����)
	//
	// 4Byte : �۽��� No
	//
	// 2Byte : �޽��� Size
	// Size  : ��ȭ����(�����ڵ�)
	//------------------------------------------------------------
	mpResChat(&stPacketHeader, Packet, pClient->dwClientID, wMsgSize, szMsg);

	// �ش� ä�ù� ������ ������ ������ �ٸ� �̿��� ����
	SendPacket_Broadcast(pRoom, pClient, &stPacketHeader, Packet);

	delete szMsg;
	return true;
}

bool OnRecv_RoomLeave(st_CLIENT * pClient, CSerialBuffer * Packet)
{
	st_PACKET_HEADER stPacketHeader;
	st_CHATROOM* pRoom = FindChatRoom(pClient->dwEnterRoomID);
	if (pRoom == nullptr)
	{
		LOG(L"SYSTEM", LOG_ERROR, L"OnRecv_RoomLeave() - FindChatRoom() Error. RoomNo:%d", pClient->dwEnterRoomID);
		Disconnect(pClient->dwClientID);
		return false;
	}
	//------------------------------------------------------------
	// 11 Req ������ 
	//
	// None
	//------------------------------------------------------------
	//------------------------------------------------------------
	// 12 Res ������ (����)
	//
	// 4Byte : ����� No
	//------------------------------------------------------------

	/////////////////////////////////////////////////////////////////////////
	// �� Ż��
	pRoom->UserList.remove(pClient->dwClientID);

	// �� Ż�� ����
	mpResRoomLeave(&stPacketHeader, Packet, pClient->dwClientID);
	SendPacket_Broadcast(&stPacketHeader, Packet);
	// �� ���� �ο��� 0�̸� �ڵ����� �� ���� (�κ� ����)
	if (pRoom->UserList.size() == 0 && pRoom->dwRoomID != 0) // 0 == �κ�
	{
		mpResRoomDelete(&stPacketHeader, Packet, pRoom->dwRoomID);
		SendPacket_Broadcast(&stPacketHeader, Packet);

		g_ChatRoomMap.erase(pRoom->dwRoomID);
		delete pRoom;
	}
	/////////////////////////////////////////////////////////////////////////

	// �κ� ����
	pClient->dwEnterRoomID = g_pLobby->dwRoomID;
	g_pLobby->UserList.push_front(pClient->dwClientID);

	return true;
}

void Disconnect(DWORD dwClientID)
{
	st_CLIENT* pClient = FindClient(dwClientID);
	if (pClient == nullptr)
	{
		LOG(L"SYSTEM", LOG_ERROR, L"Disconnect() - FindClient() Error. UserNo:%d / Total: %d", dwClientID, g_ClientMap.size());
		g_ClientMap.erase(dwClientID);
		closesocket(pClient->Sock);
		return;
	}

	st_CHATROOM* pRoom = FindChatRoom(pClient->dwEnterRoomID);
	if (pRoom != nullptr)
	{
		/////////////////////////////////////////////////////////////////////////
		// �� Ż��
		pRoom->UserList.remove(pClient->dwClientID);

		// �� Ż�� ����
		st_PACKET_HEADER stPacketHeader;
		CSerialBuffer Packet;
		mpResRoomLeave(&stPacketHeader, &Packet, pClient->dwClientID);
		SendPacket_Broadcast(&stPacketHeader, &Packet);

		// �� ���� �ο��� 0�̸� �ڵ����� �� ���� (�κ� ����)
		if (pRoom->UserList.size() == 0 && pRoom->dwRoomID != 0) // 0 == �κ�
		{
			mpResRoomDelete(&stPacketHeader, &Packet, pRoom->dwRoomID);
			SendPacket_Broadcast(&stPacketHeader, &Packet);

			g_ChatRoomMap.erase(pRoom->dwRoomID);
			delete pRoom;
		}
		/////////////////////////////////////////////////////////////////////////
	}
	delete pClient;
	
	g_ClientMap.erase(dwClientID);
	closesocket(pClient->Sock);

	LOG(L"SYSTEM", LOG_SYSTM, L"Disconnect IP:%s / UserNo:%d / Total: %d", pClient->szIP, g_dwClientID, g_ClientMap.size());
}

st_CLIENT* FindClient(int iClientID)
{
	map<DWORD, st_CLIENT*>::iterator Clientiter = g_ClientMap.find(iClientID);
	if (g_ClientMap.end() == Clientiter)
		return nullptr;
	return Clientiter->second;
}

st_CHATROOM* FindChatRoom(int iRoomID)
{
	map<DWORD, st_CHATROOM*>::iterator iter = g_ChatRoomMap.find(iRoomID);
	if (g_ChatRoomMap.end() == iter)
		return nullptr;
	return iter->second;
}

int RecvComplete(st_CLIENT * pClient)
{
	st_PACKET_HEADER stPacketHeader;
	while (1)
	{
		// RecvQ�� Header���̸�ŭ �ִ��� Ȯ��
		int iRecvQSize = pClient->RecvQ.GetUseSize();
		if (iRecvQSize < sizeof(st_PACKET_HEADER)) 
			break;

		// Packet ���� Ȯ�� (Headerũ�� + Payload���� + EndCode ����)
		pClient->RecvQ.Peek((char*)&stPacketHeader, sizeof(st_PACKET_HEADER));
		if ((WORD)iRecvQSize < stPacketHeader.wPayloadSize + sizeof(st_PACKET_HEADER))
			break;
		// ���� ��Ŷ���κ��� Header ����
		pClient->RecvQ.MoveReadPos(sizeof(st_PACKET_HEADER));

		if (stPacketHeader.byCode != dfPACKET_CODE)
		{
			LOG(L"SYSTEM", LOG_DEBUG, L"RecvComplete() PacketHeader Code Error. UserNo:%d", pClient->dwClientID);
			Disconnect(pClient->dwClientID);
			break;
		}

		CSerialBuffer pPacket;
		// Payload �κ� ��Ŷ ���۷� ����
		if (stPacketHeader.wPayloadSize != pClient->RecvQ.Dequeue(pPacket.GetBufferPtr(), stPacketHeader.wPayloadSize))
		{
			LOG(L"SYSTEM", LOG_DEBUG, L"RecvComplete() Payload Size Error. UserNo:%d", pClient->dwClientID);
			Disconnect(pClient->dwClientID);
			break;
		}
		pPacket.MoveWritePos(stPacketHeader.wPayloadSize);

		// Checksum Ȯ��
		BYTE byCheckSum = MakeCheckSum(&pPacket, stPacketHeader.wMsgType);
		if (byCheckSum != stPacketHeader.byCheckSum)
		{
			LOG(L"SYSTEM", LOG_DEBUG, L"RecvComplete() Checksum Error. UserNo:%d", pClient->dwClientID);
			Disconnect(pClient->dwClientID);
			break;
		}

		// ��Ŷ ó�� �Լ� ȣ��
		OnRecv(pClient, stPacketHeader.wMsgType, &pPacket);
	}
	return 0;	// ��Ŷ 1�� ó�� �Ϸ�, �� �Լ� ȣ��ο��� Loop ����
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
		pPtr++;
	}
	return (BYTE)(iChecksum % 256);
}

void mpResLogin(st_PACKET_HEADER * pHeader, CSerialBuffer *pPacket, BYTE byResult, DWORD dwClientID)
{
	//------------------------------------------------------------
	// Res �α���                              
	// 
	// 1Byte	: ��� (1:OK / 2:�ߺ��г��� / 3:������ʰ� / 4:��Ÿ����)
	// 4Byte	: ����� NO
	//------------------------------------------------------------
	pPacket->Clear();

	if (byResult == df_RESULT_LOGIN_OK)
	{
		*pPacket << byResult;
		*pPacket << dwClientID;
	}
	pHeader->byCode = (BYTE)dfPACKET_CODE;
	pHeader->byCheckSum = MakeCheckSum(pPacket, df_RES_LOGIN);
	pHeader->wMsgType = (SHORT)df_RES_LOGIN;
	pHeader->wPayloadSize = (SHORT)pPacket->GetUseSize();
}

void mpResRoomList(st_PACKET_HEADER * pHeader, CSerialBuffer *pPacket, WORD iRoomCnt)
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
	WORD wTitleSize;

	*pPacket << iRoomCnt;

	for (map<DWORD, st_CHATROOM*>::iterator iter = g_ChatRoomMap.begin(); iter != g_ChatRoomMap.end(); ++iter)
	{
		*pPacket << (*iter).second->dwRoomID;
		wTitleSize = (WORD)wcslen((*iter).second->szTitle) * sizeof(WCHAR);
		*pPacket << wTitleSize;
		pPacket->Enqueue((char*)(*iter).second->szTitle, wTitleSize);

		*pPacket << (BYTE)(*iter).second->UserList.size();
		for (list<DWORD>::iterator inneriter = (*iter).second->UserList.begin(); inneriter != (*iter).second->UserList.end(); ++inneriter)
		{
			st_CLIENT* pClient = FindClient(*inneriter);
			if (pClient == nullptr)
			{
				LOG(L"SYSTEM", LOG_ERROR, L"mpResRoomList() - FindClient() Error. UserNo:%d", *inneriter);
				continue;
			}
			pPacket->Enqueue((char*)pClient->Nickname, sizeof(pClient->Nickname));
		}
	}

	pHeader->byCode = (BYTE)dfPACKET_CODE;
	pHeader->byCheckSum = MakeCheckSum(pPacket, df_RES_ROOM_LIST);
	pHeader->wMsgType = (SHORT)df_RES_ROOM_LIST;
	pHeader->wPayloadSize = (SHORT)pPacket->GetUseSize();
}

void mpResRoomCreate(st_PACKET_HEADER * pHeader, CSerialBuffer *pPacket, BYTE byResult, st_CHATROOM *pRoom)
{
	//------------------------------------------------------------
	// Res ��ȭ�� ���� (���÷�)
	//
	// 1Byte : ��� (1:OK / 2:���̸� �ߺ� / 3:�����ʰ� / 4:��Ÿ����)
	//
	//
	// 4Byte : �� No
	// 2Byte : ������ ����Ʈ Size
	// Size  : ������ (�����ڵ�)
	//------------------------------------------------------------
	WORD wTitleSize;

	pPacket->Clear();
	*pPacket << byResult;
	if (byResult == df_RESULT_ROOM_CREATE_OK)
	{
		*pPacket << pRoom->dwRoomID;
		wTitleSize = (WORD)wcslen(pRoom->szTitle) * sizeof(WCHAR);
		*pPacket << wTitleSize;
		pPacket->Enqueue((char*)pRoom->szTitle, wTitleSize);
	}
	pHeader->byCode = (BYTE)dfPACKET_CODE;
	pHeader->byCheckSum = MakeCheckSum(pPacket, df_RES_ROOM_CREATE);
	pHeader->wMsgType = (SHORT)df_RES_ROOM_CREATE;
	pHeader->wPayloadSize = (SHORT)pPacket->GetUseSize();
}

void mpResRoomEnter(st_PACKET_HEADER * pHeader, CSerialBuffer *pPacket, BYTE byResult, st_CHATROOM *pRoom)
{
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
	WORD wTitleSize;

	pPacket->Clear();
	*pPacket << byResult;

	if (byResult == df_RESULT_ROOM_ENTER_OK)
	{
		*pPacket << pRoom->dwRoomID;
		wTitleSize = (WORD)wcslen(pRoom->szTitle) * sizeof(WCHAR);
		*pPacket << wTitleSize;
		pPacket->Enqueue((char*)pRoom->szTitle, wTitleSize);
		*pPacket << (BYTE)pRoom->UserList.size();
		for (list<DWORD>::iterator iter = pRoom->UserList.begin(); iter != pRoom->UserList.end(); ++iter)
		{
			st_CLIENT* pClient = FindClient(*iter);
			if (pClient == nullptr)
			{
				LOG(L"SYSTEM", LOG_ERROR, L"mpResRoomEnter() - FindClient() Error. UserNo:%d", *iter);
				continue;
			}
			pPacket->Enqueue((char*)pClient->Nickname, sizeof(pClient->Nickname));
			*pPacket << *iter;
		}
	}

	pHeader->byCode = (BYTE)dfPACKET_CODE;
	pHeader->byCheckSum = MakeCheckSum(pPacket, df_RES_ROOM_ENTER);
	pHeader->wMsgType = (SHORT)df_RES_ROOM_ENTER;
	pHeader->wPayloadSize = (SHORT)pPacket->GetUseSize();
}

void mpResChat(st_PACKET_HEADER * pHeader, CSerialBuffer *pPacket, DWORD dwSendClientID, WORD iMsgSize, WCHAR * pContents)
{
	//------------------------------------------------------------
	// 10 Res ä�ü��� (�ƹ����� �� �� ����)  (������ ���� ����)
	//
	// 4Byte : �۽��� No
	//
	// 2Byte : �޽��� Size
	// Size  : ��ȭ����(�����ڵ�)
	//------------------------------------------------------------
	pPacket->Clear();
	*pPacket << dwSendClientID;
	*pPacket << iMsgSize;
	pPacket->Enqueue((char*)pContents, iMsgSize);

	pHeader->byCode = (BYTE)dfPACKET_CODE;
	pHeader->byCheckSum = MakeCheckSum(pPacket, df_RES_CHAT);
	pHeader->wMsgType = (SHORT)df_RES_CHAT;
	pHeader->wPayloadSize = (SHORT)pPacket->GetUseSize();
}

void mpResRoomLeave(st_PACKET_HEADER * pHeader, CSerialBuffer *pPacket, DWORD dwClientID)
{
	//------------------------------------------------------------
	// 12 Res ������ (����)
	//
	// 4Byte : ����� No
	//------------------------------------------------------------
	pPacket->Clear();
	*pPacket << dwClientID;
	pHeader->byCode = (BYTE)dfPACKET_CODE;
	pHeader->byCheckSum = MakeCheckSum(pPacket, df_RES_ROOM_LEAVE);
	pHeader->wMsgType = (SHORT)df_RES_ROOM_LEAVE;
	pHeader->wPayloadSize = (SHORT)pPacket->GetUseSize();
}

void mpResRoomDelete(st_PACKET_HEADER * pHeader, CSerialBuffer *pPacket, DWORD dwRoomID)
{
	//------------------------------------------------------------
	// 13 Res ����� (����)
	//
	// 4Byte : �� No
	//------------------------------------------------------------
	pPacket->Clear();
	*pPacket << dwRoomID;
	pHeader->byCode = (BYTE)dfPACKET_CODE;
	pHeader->byCheckSum = MakeCheckSum(pPacket, df_RES_ROOM_DELETE);
	pHeader->wMsgType = (SHORT)df_RES_ROOM_DELETE;
	pHeader->wPayloadSize = (SHORT)pPacket->GetUseSize();
}

void mpResUserEnter(st_PACKET_HEADER * pHeader, CSerialBuffer *pPacket, WCHAR * pNick, DWORD dwClientID)
{
	//------------------------------------------------------------
	// 14 Res Ÿ ����� ���� (����)
	//
	// WCHAR[15] : �г���(�����ڵ�)
	// 4Byte : ����� No
	//------------------------------------------------------------
	pPacket->Clear();
	pPacket->Enqueue((char*)pNick, sizeof(WCHAR) * dfNICK_MAX_LEN);
	*pPacket << dwClientID;
	pHeader->byCode = (BYTE)dfPACKET_CODE;
	pHeader->byCheckSum = MakeCheckSum(pPacket, df_RES_USER_ENTER);
	pHeader->wMsgType = (SHORT)df_RES_USER_ENTER;
	pHeader->wPayloadSize = (SHORT)pPacket->GetUseSize();
}