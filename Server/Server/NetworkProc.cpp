#include "NetworkProc.h"
#include "CSystemLog.h"
using namespace mylib;
using namespace std;

map<DWORD, st_CLIENT*>		g_ClientMap;
map<DWORD, st_CHATROOM*>	g_ChatRoomMap;
st_CHATROOM*	g_pLobby;
SOCKET			g_ListenSocket = INVALID_SOCKET;
// 유저 고유키, 방 고유키. 할당시마다 ++
DWORD			g_dwClientID;
DWORD			g_dwRoomID;
// 고유키 부분은 실제 게임서버에서는 이처럼 사용할 수 없음. 계정 생성시 고유한 번호가 부여돼야하므로 DB에서 고유한 값을 얻어내는 공식을 통해 생성된다


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
	DWORD	UserTable_ID[FD_SETSIZE];		// FD_SET에 등록된 UserID 저장
	SOCKET	UserTable_SOCKET[FD_SETSIZE];	// FD_SET에 등록된 소켓 저장
	int		iSockCnt = 0;
	//-----------------------------------------------------
	// FD_SET은 FD_SETSIZE만큼씩만 소켓 검사 가능
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
	// ListenSocket 및 모든 클라이언트에 대해 Socket 검사
	//-----------------------------------------------------
	for (map<DWORD, st_CLIENT*>::iterator iter = g_ClientMap.begin(); iter != g_ClientMap.end();)
	{
		pClient = iter->second;
		++iter;	// SelectSocket 함수 내부에서 ClientMap을 삭제하는 경우가 있어서 미리 증가
		//-----------------------------------------------------
		// 해당 클라이언트 ReadSet 등록
		// SendQ에 데이터가 있다면 WriteSet 등록
		//-----------------------------------------------------
		UserTable_ID[iSockCnt] = pClient->dwClientID;
		UserTable_SOCKET[iSockCnt] = pClient->Sock;

		// ReadSet 등록
		FD_SET(pClient->Sock, &ReadSet);

		// WriteSet 등록
		if (pClient->SendQ.GetUseSize() > 0)
			FD_SET(pClient->Sock, &WriteSet);

		++iSockCnt;
		//-----------------------------------------------------
		// select 최대치 도달, 만들어진 테이블 정보로 select 호출 후 정리
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
	// 전체 클라이언트 for 문 종료 후 iSockCnt 수치가 있다면
	// 추가적으로 마지막 Select 호출을 해준다
	//-----------------------------------------------------
	if (iSockCnt > 0)
	{
		SelectSocket(UserTable_ID, UserTable_SOCKET, &ReadSet, &WriteSet);
	}
}

void SelectSocket(DWORD *dwpTableID, SOCKET* pTableSocket, FD_SET *pReadSet, FD_SET *pWriteSet)
{
	////////////////////////////////////////////////////////////////////
	// select() 함수의 디폴트 한계 값인 FD_SETSIZE(기본 64소켓)을 넘지 않았다면 
	// TimeOut 시간(Select 대기 시간)을 0으로 만들 필요가 없다.
	//
	// 그러나, FD_SETSIZE(기본 64소켓)을 넘었다면 Select를 두 번 이상 해야한다.
	// 이 경우 TimeOut 시간을 0으로 해야하는데 
	// 그 이유는 첫번째 select에서 블럭이 걸려 두번째 select가 반응하지 않기 떄문이다.
	////////////////////////////////////////////////////////////////////
	//-----------------------------------------------------
	// select 함수의 대기시간 설정
	//-----------------------------------------------------
	timeval Time;
	Time.tv_sec = 0;
	Time.tv_usec = 0;

	//-----------------------------------------------------
	// 접속자 요청과 현재 접속중인 클라이언트들의 메시지 송신 검사
	//-----------------------------------------------------
	//  select() 함수에서 block 상태에 있다가 요청이 들어오면,
	// select() 함수는 몇대에서 요청이 들어왔는지 감지를 하고 그 개수를 return합니다.
	// select() 함수가 return 되는 순간 flag를 든 소켓만 WriteSet, ReadSet에 남아있게 됩니다.
	// 그리고 각각 소켓 셋으로 부터 flag 표시를 한 소켓들을 checking하는 것입니다.
	int retval = select(0, pReadSet, pWriteSet, NULL, &Time);
	// return 값이 0 이상이라면 누군가의 데이터가 있다는 의미
	if (retval > 0)
	{
		//-----------------------------------------------------
		// TableSocket을 돌면서 어떤 소켓에 반응이 있었는지 확인
		//-----------------------------------------------------
		for (int iCnt = 0; iCnt < FD_SETSIZE; ++iCnt)
		{
			if (pTableSocket[iCnt] == INVALID_SOCKET) // Scoket 핸들 반환 실패시 INVALID_SOCKET을 반환함
				continue;

			//-----------------------------------------------------
			// Write 체크
			//-----------------------------------------------------
			if (FD_ISSET(pTableSocket[iCnt], pWriteSet))
			{
				SendEvent(dwpTableID[iCnt]);
			}

			//-----------------------------------------------------
			// Read 체크
			//-----------------------------------------------------
			if (FD_ISSET(pTableSocket[iCnt], pReadSet))
			{
				//-----------------------------------------------------
				// ListenSocket은 접속자 수락 용도이므로 별도 처리 
				//-----------------------------------------------------
				if (dwpTableID[iCnt] == 0) // UserID가 0이면 ListenSocket
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
		if (err != ERROR_IO_PENDING) // ERROR_IO_PENDING : 비동기 입출력을 하고 있다는 의미
		{
			if (err != 10038 && err != 10053 && err != 10054 && err != 10058)
			{
				// WSAENOTSOCK(10038)
				//	의미 : 소켓이 아닌 항목에 대해 소켓 작업을 시도했습니다.
				//	설명 : 소켓 핸들 매개 변수가 올바른 소켓을 참조하지 않았거나 fd_set의 멤버가 올바르지 않습니다.
				// WSAECONNABORTED(10053)
				//	의미: 소프트웨어에서 연결을 중단했습니다.
				//	설명 : 호스트 컴퓨터의 소프트웨어가 설정된 연결을 중지했습니다. 
				//        데이터 전송 시간이 초과되었거나 프로토콜 오류가 발생했기 때문일 수 있습니다.
				// WSAECONNRESET(10054)
				//  의미 : 피어가 연결을 다시 설정했습니다.
				//	설명 : 원격 호스트에 의해 기존 연결이 강제로 끊어졌습니다.
				//       원격 호스트가 갑자기 중지되거나 다시 시작되거나 하드 종료를 사용하는 경우에 발생
				// WSAESHUTDOWN(10058)
				//	의미: 소켓이 종료된 후에는 전송할 수 없습니다.
				//	설명 : 이전의 shutdown에서 해당 방향의 소켓이 이미 종료되었으므로 데이터 전송 또는 수신 요청이 허용되지 않습니다.
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
				//	의미 : 소켓이 아닌 항목에 대해 소켓 작업을 시도했습니다.
				//	설명 : 소켓 핸들 매개 변수가 올바른 소켓을 참조하지 않았거나 fd_set의 멤버가 올바르지 않습니다.
				// WSAECONNABORTED(10053)
				//	의미: 소프트웨어에서 연결을 중단했습니다.
				//	설명 : 호스트 컴퓨터의 소프트웨어가 설정된 연결을 중지했습니다. 
				//        데이터 전송 시간이 초과되었거나 프로토콜 오류가 발생했기 때문일 수 있습니다.
				// WSAECONNRESET(10054)
				//  의미 : 피어가 연결을 다시 설정했습니다.
				//	설명 : 원격 호스트에 의해 기존 연결이 강제로 끊어졌습니다.
				//       원격 호스트가 갑자기 중지되거나 다시 시작되거나 하드 종료를 사용하는 경우에 발생
				// WSAESHUTDOWN(10058)
				//	의미: 소켓이 종료된 후에는 전송할 수 없습니다.
				//	설명 : 이전의 shutdown에서 해당 방향의 소켓이 이미 종료되었으므로 데이터 전송 또는 수신 요청이 허용되지 않습니다.
				LOG(L"SYSTEM", LOG_ERROR, L"WSASend() UserNo: %d, ErrorCode: %d", dwClientID, err);
			}
			Disconnect(pClient->dwClientID);
			return false;
		}
	}

	//-----------------------------------------------------
	// Send Complate
	// 패킷 전송이 완료됐다는 의미는 아님. 소켓 버퍼에 복사를 완료했다는 의미
	// 송신 큐에서 빼냈던 데이터 제거
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
	//		// 보낼 사이즈보다 더 크다면 오류
	//		// 생기면 안되는 상황이지만 가끔 이런 경우가 생길 수 있다
	//		//-----------------------------------------------------
	//		LOG(L"SYSTEM", LOG_ERROR, L"# send() #	send size error, ClientID: %d", pClient->dwClientID);
	//		return false;
	//	}

	//	//-----------------------------------------------------
	//	// Send Complate
	//	// 패킷 전송이 완료됐다는 의미는 아님. 소켓 버퍼에 복사를 완료했다는 의미
	//	// 송신 큐에서 Peek으로 빼냈던 데이터 제거
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
	// 1 Req 로그인
	//
	//
	// WCHAR[15]	: 닉네임 (유니코드)
	//------------------------------------------------------------
	// 256 이상의 글자인 경우 예외처리
	Packet->Dequeue((char*)szNickName, dfNICK_MAX_LEN);

	// 중복 검사
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

		// 로비 입장
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
	// 3 Req 대화방 리스트
	//
	//	None
	//------------------------------------------------------------

	// 결과를 클라이언트로 전송
	//------------------------------------------------------------
	// 4 Res 대화방 리스트
	//
	//  2Byte	: 개수
	//  {
	//		4Byte : 방 No
	//		2Byte : 방이름 byte size
	//		Size  : 방이름 (유니코드)
	//
	//		1Byte : 참여인원		
	//		{
	//			WHCAR[15] : 닉네임
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
	// 5 Req 대화방 생성
	//
	// 2Byte : 방제목 Size			유니코드 문자 바이트 길이 (널 제외)
	// Size  : 방제목 (유니코드)
	//------------------------------------------------------------
	// 256 이상의 글자인 경우 예외처리
	*Packet >> wTitleSize;
	if (wTitleSize > 0 && wTitleSize < 256)
	{
		Packet->Dequeue((char*)szRoomTitle, wTitleSize);

		// 중복 검사
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
			// 방 생성 작업
			pRoom = new st_CHATROOM;

			pRoom->dwRoomID = ++g_dwRoomID;
			wcscpy_s(pRoom->szTitle, szRoomTitle);
			g_ChatRoomMap.insert(pair<DWORD, st_CHATROOM*>(pRoom->dwRoomID, pRoom));
			retval = df_RESULT_ROOM_CREATE_OK;
			LOG(L"SYSTEM", LOG_SYSTM, L"Room %s Create by UserNo:%d (TotalRoom:%d) ", pRoom->szTitle, pClient->dwClientID, g_ChatRoomMap.size());
		}
	}

	//------------------------------------------------------------
	// 6 Res 대화방 생성 (수시로)
	//
	// 1Byte : 결과 (1:OK / 2:방이름 중복 / 3:개수초과 / 4:기타오류)
	//
	//
	// 4Byte : 방 No
	// 2Byte : 방제목 바이트 Size
	// Size  : 방제목 (유니코드)
	//------------------------------------------------------------
	// 방 생성 결과를 클라이언트로 전송
	mpResRoomCreate(&stPacketHeader, Packet, retval, pRoom);

	// 채팅방 생성 결과가 성공인 경우 모든 유저에게 전송
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
	// 7 Req 대화방 입장
	//
	//	4Byte : 방 No
	//------------------------------------------------------------
	*Packet >> dwRoomID;

	// 방 찾기
	st_CHATROOM* pRoom = FindChatRoom(dwRoomID);
	if (pRoom == nullptr)
	{
		LOG(L"SYSTEM", LOG_ERROR, L"OnRecv_RoomEnter() - FindChatRoom() Error. RoomNo:%d", dwRoomID);
		retval = df_RESULT_ROOM_ENTER_NOT;
	}
	// 로비에 있었는가
	else if (pClient->dwEnterRoomID == g_pLobby->dwRoomID)
	{
		retval = df_RESULT_ROOM_ENTER_OK;
	}

	if (retval == df_RESULT_ROOM_ENTER_OK)
	{
		// 로비 탈출
		g_pLobby->UserList.remove(pClient->dwClientID);

		// 방 입장
		pRoom->UserList.push_front(pClient->dwClientID);
		pClient->dwEnterRoomID = pRoom->dwRoomID;
		
		// 방 입장 로그
		LOG(L"SYSTEM", LOG_SYSTM, L"Room %s Enter UserNo:%d", pRoom->szTitle, pClient->dwClientID);
	}

	// 방 입장 결과 전달
	//------------------------------------------------------------
	// 8 Res 대화방 입장
	//
	// 1Byte : 결과 (1:OK / 2:방No 오류 / 3:인원초과 / 4:기타오류)
	//
	// OK 의 경우에만 다음 전송
	//	{
	//		4Byte : 방 No
	//		2Byte : 방제목 Size
	//		Size  : 방제목 (유니코드)
	//
	//		1Byte : 참가인원
	//		{
	//			WCHAR[15] : 닉네임(유니코드)
	//			4Byte     : 사용자No
	//		}
	//	}
	//------------------------------------------------------------
	//------------------------------------------------------------
	// 14 Res 타 사용자 입장 (수시)
	//
	// WCHAR[15] : 닉네임(유니코드)
	// 4Byte : 사용자 No
	//------------------------------------------------------------
	// 방 생성 결과를 클라이언트로 전송
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
	// 9 Req 채팅송신
	//
	// 2Byte : 메시지 Size
	// Size  : 대화내용(유니코드)
	//------------------------------------------------------------
	// 2Byte : 메시지 Size
	*Packet >> wMsgSize;

	// Size  : 대화내용(유니코드)
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
	// 10 Res 채팅수신 (아무때나 올 수 있음)  (나에겐 오지 않음)
	//
	// 4Byte : 송신자 No
	//
	// 2Byte : 메시지 Size
	// Size  : 대화내용(유니코드)
	//------------------------------------------------------------
	mpResChat(&stPacketHeader, Packet, pClient->dwClientID, wMsgSize, szMsg);

	// 해당 채팅방 내에서 본인을 제외한 다른 이에게 전송
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
	// 11 Req 방퇴장 
	//
	// None
	//------------------------------------------------------------
	//------------------------------------------------------------
	// 12 Res 방퇴장 (수시)
	//
	// 4Byte : 사용자 No
	//------------------------------------------------------------

	/////////////////////////////////////////////////////////////////////////
	// 방 탈출
	pRoom->UserList.remove(pClient->dwClientID);

	// 방 탈출 전달
	mpResRoomLeave(&stPacketHeader, Packet, pClient->dwClientID);
	SendPacket_Broadcast(&stPacketHeader, Packet);
	// 방 참여 인원이 0이면 자동으로 방 삭제 (로비 제외)
	if (pRoom->UserList.size() == 0 && pRoom->dwRoomID != 0) // 0 == 로비
	{
		mpResRoomDelete(&stPacketHeader, Packet, pRoom->dwRoomID);
		SendPacket_Broadcast(&stPacketHeader, Packet);

		g_ChatRoomMap.erase(pRoom->dwRoomID);
		delete pRoom;
	}
	/////////////////////////////////////////////////////////////////////////

	// 로비 입장
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
		// 방 탈출
		pRoom->UserList.remove(pClient->dwClientID);

		// 방 탈출 전달
		st_PACKET_HEADER stPacketHeader;
		CSerialBuffer Packet;
		mpResRoomLeave(&stPacketHeader, &Packet, pClient->dwClientID);
		SendPacket_Broadcast(&stPacketHeader, &Packet);

		// 방 참여 인원이 0이면 자동으로 방 삭제 (로비 제외)
		if (pRoom->UserList.size() == 0 && pRoom->dwRoomID != 0) // 0 == 로비
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
		// RecvQ에 Header길이만큼 있는지 확인
		int iRecvQSize = pClient->RecvQ.GetUseSize();
		if (iRecvQSize < sizeof(st_PACKET_HEADER)) 
			break;

		// Packet 길이 확인 (Header크기 + Payload길이 + EndCode 길이)
		pClient->RecvQ.Peek((char*)&stPacketHeader, sizeof(st_PACKET_HEADER));
		if ((WORD)iRecvQSize < stPacketHeader.wPayloadSize + sizeof(st_PACKET_HEADER))
			break;
		// 받은 패킷으로부터 Header 제거
		pClient->RecvQ.MoveReadPos(sizeof(st_PACKET_HEADER));

		if (stPacketHeader.byCode != dfPACKET_CODE)
		{
			LOG(L"SYSTEM", LOG_DEBUG, L"RecvComplete() PacketHeader Code Error. UserNo:%d", pClient->dwClientID);
			Disconnect(pClient->dwClientID);
			break;
		}

		CSerialBuffer pPacket;
		// Payload 부분 패킷 버퍼로 복사
		if (stPacketHeader.wPayloadSize != pClient->RecvQ.Dequeue(pPacket.GetBufferPtr(), stPacketHeader.wPayloadSize))
		{
			LOG(L"SYSTEM", LOG_DEBUG, L"RecvComplete() Payload Size Error. UserNo:%d", pClient->dwClientID);
			Disconnect(pClient->dwClientID);
			break;
		}
		pPacket.MoveWritePos(stPacketHeader.wPayloadSize);

		// Checksum 확인
		BYTE byCheckSum = MakeCheckSum(&pPacket, stPacketHeader.wMsgType);
		if (byCheckSum != stPacketHeader.byCheckSum)
		{
			LOG(L"SYSTEM", LOG_DEBUG, L"RecvComplete() Checksum Error. UserNo:%d", pClient->dwClientID);
			Disconnect(pClient->dwClientID);
			break;
		}

		// 패킷 처리 함수 호출
		OnRecv(pClient, stPacketHeader.wMsgType, &pPacket);
	}
	return 0;	// 패킷 1개 처리 완료, 본 함수 호출부에서 Loop 유도
}

BYTE MakeCheckSum(CSerialBuffer * pPacket, DWORD dwType)
{
	//------------------------------------------------------
	//	checkSum - 각 MsgType, Payload 의 각 바이트 더하기 % 256
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
	// Res 로그인                              
	// 
	// 1Byte	: 결과 (1:OK / 2:중복닉네임 / 3:사용자초과 / 4:기타오류)
	// 4Byte	: 사용자 NO
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
	// 4 Res 대화방 리스트
	//
	//  2Byte	: 개수
	//  {
	//		4Byte : 방 No
	//		2Byte : 방이름 byte size
	//		Size  : 방이름 (유니코드)
	//
	//		1Byte : 참여인원		
	//		{
	//			WHCAR[15] : 닉네임
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
	// Res 대화방 생성 (수시로)
	//
	// 1Byte : 결과 (1:OK / 2:방이름 중복 / 3:개수초과 / 4:기타오류)
	//
	//
	// 4Byte : 방 No
	// 2Byte : 방제목 바이트 Size
	// Size  : 방제목 (유니코드)
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
	// 8 Res 대화방 입장
	//
	// 1Byte : 결과 (1:OK / 2:방No 오류 / 3:인원초과 / 4:기타오류)
	//
	// OK 의 경우에만 다음 전송
	//	{
	//		4Byte : 방 No
	//		2Byte : 방제목 Size
	//		Size  : 방제목 (유니코드)
	//
	//		1Byte : 참가인원
	//		{
	//			WCHAR[15] : 닉네임(유니코드)
	//			4Byte     : 사용자No
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
	// 10 Res 채팅수신 (아무때나 올 수 있음)  (나에겐 오지 않음)
	//
	// 4Byte : 송신자 No
	//
	// 2Byte : 메시지 Size
	// Size  : 대화내용(유니코드)
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
	// 12 Res 방퇴장 (수시)
	//
	// 4Byte : 사용자 No
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
	// 13 Res 방삭제 (수시)
	//
	// 4Byte : 방 No
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
	// 14 Res 타 사용자 입장 (수시)
	//
	// WCHAR[15] : 닉네임(유니코드)
	// 4Byte : 사용자 No
	//------------------------------------------------------------
	pPacket->Clear();
	pPacket->Enqueue((char*)pNick, sizeof(WCHAR) * dfNICK_MAX_LEN);
	*pPacket << dwClientID;
	pHeader->byCode = (BYTE)dfPACKET_CODE;
	pHeader->byCheckSum = MakeCheckSum(pPacket, df_RES_USER_ENTER);
	pHeader->wMsgType = (SHORT)df_RES_USER_ENTER;
	pHeader->wPayloadSize = (SHORT)pPacket->GetUseSize();
}