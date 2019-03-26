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
		//연결 끊김 처리. 실제로는 연결이 끊어졌습니다 메시지 띄우고 내비둠.
		//ProcClose();
		return false;
	case FD_READ:
		RecvEvent();	// Read에서 반복문 넣어서 wouldblock 뜰 때까지 모두 처리해도 된다. (클라만 가능)
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
	if (!g_bSendFlag)	// Send 가능여부 확인 / 이는 FD_WRITE 발생여부임 -> AsyncSelect에서는 connect 반응이 FD_WRITE로 옴
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
	// 패킷 전송이 완료됐다는 의미는 아님. 소켓 버퍼에 복사를 완료했다는 의미
	// 송신 큐에서 빼냈던 데이터 제거
	//-----------------------------------------------------
	g_SendQ.MoveReadPos(dwTransferred);
	return true;

	/* send ver*/
	//char SendBuff[CRingBuffer::en_BUFFER_DEFALUT];

	//if (!g_bSendFlag)	// Send 가능여부 확인 / 이는 FD_WRITE 발생여부임 -> AsyncSelect에서는 connect 반응이 FD_WRITE로 옴
	//	return false;

	//int iSendSize = g_SendQ.GetUseSize();
	//iSendSize = min(CRingBuffer::en_BUFFER_DEFALUT, iSendSize);
	//if (iSendSize <= 0)
	//	return true;

	//g_bSendFlag = TRUE;

	//while (1)
	//{
	//	// SendQ 에 데이터가 있는지 확인 (루프용)
	//	iSendSize = g_SendQ.GetUseSize();
	//	iSendSize = min(CRingBuffer::en_BUFFER_DEFALUT, iSendSize);
	//	if (iSendSize <= 0)
	//		break;

	//	// Peek으로 데이터 뽑음. 전송이 제대로 마무리됐을 경우 삭제
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
	//			// 보낼 사이즈보다 더 크다면 오류
	//			// 생기면 안되는 상황이지만 가끔 이런 경우가 생길 수 있다
	//			//-----------------------------------------------------
	//			return false;
	//		}
	//		else
	//		{
	//			//-----------------------------------------------------
	//			// Send Complate
	//			// 패킷 전송이 완료됐다는 의미는 아님. 소켓 버퍼에 복사를 완료했다는 의미
	//			// 송신 큐에서 Peek으로 빼냈던 데이터 제거
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
	// 1 Req 로그인
	//
	//
	// WCHAR[15]	: 닉네임 (유니코드)
	//------------------------------------------------------------
	mpReqLogin(&stPacketHeader, &Packet, g_szNickname);
	SendPacket(&stPacketHeader, &Packet);
}

void SendReq_RoomList()
{
	st_PACKET_HEADER stPacketHeader;
	CSerialBuffer Packet;
	//------------------------------------------------------------
	// 3 Req 대화방 리스트
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
	// 5 Req 대화방 생성
	//
	// 2Byte : 방제목 Size			유니코드 문자 바이트 길이 (널 제외)
	// Size  : 방제목 (유니코드)
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
	// 7 Req 대화방 입장
	//
	//	4Byte : 방 No
	//------------------------------------------------------------
	mpReqRoomEnter(&stPacketHeader, &Packet, dwRoomNO);
	SendPacket(&stPacketHeader, &Packet);
}

void SendReq_Chat()
{
	st_PACKET_HEADER stPacketHeader;
	CSerialBuffer Packet;
	//------------------------------------------------------------
	// 9 Req 채팅송신
	//
	// 2Byte : 메시지 Size
	// Size  : 대화내용(유니코드)
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
	// 11 Req 방퇴장 
	//
	// None
	//------------------------------------------------------------
	mpReqRoomLeave(&stPacketHeader, &Packet);
	SendPacket(&stPacketHeader, &Packet);
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
		++pPtr;
	}
	return (BYTE)(iChecksum % 256);
}

void mpReqLogin(st_PACKET_HEADER * pHeader, CSerialBuffer *pPacket, WCHAR* szNickName)
{
	//------------------------------------------------------------
	// 1 Req 로그인
	//
	//
	// WCHAR[15]	: 닉네임 (유니코드)
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
	// 3 Req 대화방 리스트
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
	// 5 Req 대화방 생성
	//
	// 2Byte : 방제목 Size			유니코드 문자 바이트 길이 (널 제외)
	// Size  : 방제목 (유니코드)
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
	// 7 Req 대화방 입장
	//
	//	4Byte : 방 No
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
	// 9 Req 채팅송신
	//
	// 2Byte : 메시지 Size
	// Size  : 대화내용(유니코드)
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
	// 11 Req 방퇴장 
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
		// RecvQ에 Header길이만큼 있는지 확인
		int g_iRecvQSize = g_RecvQ.GetUseSize();
		if (g_iRecvQSize < sizeof(st_PACKET_HEADER))
			break;

		// Packet 길이 확인 (Header크기 + Payload길이 + EndCode 길이)
		g_RecvQ.Peek((char*)&stPacketHeader, sizeof(st_PACKET_HEADER));
		if ((WORD)g_iRecvQSize < stPacketHeader.wPayloadSize + sizeof(st_PACKET_HEADER))
			break;
		// 받은 패킷으로부터 Header 제거
		g_RecvQ.MoveReadPos(sizeof(st_PACKET_HEADER));

		if (stPacketHeader.byCode != dfPACKET_CODE)
		{
			swprintf_s(szErr, L"RecvComplete() - PacketHeader Code Error");
			MessageBox(g_hWndLobby, szErr, L"Error", MB_OK | MB_ICONERROR);
			exit(1);
		}

		CSerialBuffer Packet;
		// Payload 부분 패킷 버퍼로 복사
		if (stPacketHeader.wPayloadSize != g_RecvQ.Dequeue(Packet.GetBufferPtr(), stPacketHeader.wPayloadSize))
		{
			swprintf_s(szErr, L"RecvComplete() - Payload Size Error");
			MessageBox(g_hWndLobby, szErr, L"Error", MB_OK | MB_ICONERROR);
			exit(1);
		}

		Packet.MoveWritePos(stPacketHeader.wPayloadSize);

		// Checksum 확인
		BYTE byCheckSum = MakeCheckSum(&Packet, stPacketHeader.wMsgType);
		if (byCheckSum != stPacketHeader.byCheckSum)
		{
			swprintf_s(szErr, L"RecvComplete() - Checksum Error");
			MessageBox(g_hWndLobby, szErr, L"Error", MB_OK | MB_ICONERROR);
			exit(1);
		}

		// 패킷 처리 함수 호출
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
	// 2 Res 로그인                              
	// 
	// 1Byte	: 결과 (1:OK / 2:중복닉네임 / 3:사용자초과 / 4:기타오류)
	// 4Byte	: 사용자 NO
	//------------------------------------------------------------
	BYTE byResult;
	
	// 1Byte	: 결과 (1:OK / 2:중복닉네임 / 3:사용자초과 / 4:기타오류)
	*Packet >> byResult;
	if (byResult != df_RESULT_LOGIN_OK)
	{
		WCHAR szErr[100] = { 0, };
		swprintf_s(szErr, L"Login Failed. Error:%d", byResult);
		MessageBox(g_hWndLobby, szErr, L"Error", MB_OK | MB_ICONERROR);
		exit(byResult);
	}

	// 4Byte	: 사용자 NO
	*Packet >> g_dwUserID;

	// 다이얼로그 문자열 변경
	HWND hStaticID;
	WCHAR szID[dfNICK_MAX_LEN];
	_itow_s(g_dwUserID, szID, 10);
	hStaticID = GetDlgItem(g_hWndLobby, IDC_STATICID);
	SetWindowText(hStaticID, szID);

	// 방 목록 요청
	SendReq_RoomList();
}

void OnRecv_RoomList(CSerialBuffer * Packet)
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
	WORD	wRoomCnt;
	DWORD	dwChatRoomID;
	WORD	wTitleSize;
	BYTE	byRoomUser;
	WCHAR	szNickname[dfNICK_MAX_LEN];

	//  2Byte	: 개수
	*Packet >> wRoomCnt;
	for (int iCnt = 0; iCnt < wRoomCnt; ++iCnt)
	{	
		// 4Byte : 방 No
		*Packet >> dwChatRoomID;		

		// 2Byte : 방제목 Size
		*Packet >> wTitleSize;				

		WCHAR* szRoomTitle = new WCHAR[(wTitleSize + 1) / 2];
		// Size  : 방제목 (유니코드)
		Packet->Dequeue((char*)szRoomTitle, wTitleSize); 
		szRoomTitle[wTitleSize / 2] = L'\0';
		Addlistbox_Room(szRoomTitle, dwChatRoomID);
		g_ChatRoomMap.insert(pair<DWORD, WCHAR*>(dwChatRoomID, szRoomTitle));

		//// 1Byte : 참가인원
		*Packet >> byRoomUser;					
		for (int iCnt = 0; iCnt < byRoomUser; ++iCnt)
		{
			// WCHAR[15] : 닉네임(유니코드)
			Packet->Dequeue((char*)szNickname, sizeof(szNickname));
		}
		
	}
}

void OnRecv_RoomCreate(CSerialBuffer * Packet)
{
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
	BYTE byResult;
	DWORD dwChatRoomID;
	WORD wTitleSize;

	// 1Byte : 결과 (1:OK / 2:방이름 중복 / 3:개수초과 / 4:기타오류)
	*Packet >> byResult;
	if (byResult != df_RESULT_ROOM_CREATE_OK)
	{
		WCHAR szErr[100] = { 0, };
		swprintf_s(szErr, L"Room Create Failed. Error:%d", byResult);
		MessageBox(g_hWndLobby, szErr, L"Error", MB_OK | MB_ICONERROR);
		return;
	}

	// 4Byte : 방 No
	*Packet >> dwChatRoomID;	

	auto result = g_ChatRoomMap.insert(pair<DWORD, WCHAR*>(dwChatRoomID, nullptr));
	if (result.second)
	{
		// 2Byte : 방제목 바이트 Size
		*Packet >> wTitleSize;

		// Size  : 방제목 (유니코드)
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
	BYTE byResult;
	WORD wTitleSize;
	BYTE byRoomUser;
	DWORD dwUserID;
	WCHAR szRoomTitle[256];

	// 1Byte : 결과 (1:OK / 2:방No 오류 / 3:인원초과 / 4:기타오류)
	*Packet >> byResult;
	if (byResult != df_RESULT_ROOM_ENTER_OK)
	{
		swprintf_s(szErr, L"Room Enter Failed. Error:%d", byResult);
		MessageBox(g_hWndLobby, szErr, L"Error", MB_OK | MB_ICONERROR);
		exit(byResult);
	}

	// 방 다이얼로그 생성
	CreateRoomDlg();

	// 4Byte : 방 No
	*Packet >> g_dwEnterRoomID;	

	// 2Byte : 방제목 Size
	*Packet >> wTitleSize;

	// Size  : 방제목 (유니코드)
	Packet->Dequeue((char*)szRoomTitle, wTitleSize);
	szRoomTitle[wTitleSize / 2] = L'\0';

	// 채팅방 이름 세팅
	SetDlgItemText(g_hWndRoom, IDC_STATICTITLE, szRoomTitle);
	
	// 1Byte : 참가인원
	*Packet >> byRoomUser;

	
	for (int iCnt = 0; iCnt < byRoomUser; ++iCnt)
	{
		// WCHAR[15] : 닉네임(유니코드)
		WCHAR szNickname[dfNICK_MAX_LEN];
		Packet->Dequeue((char*)szNickname, sizeof(szNickname));

		// 4Byte : 사용자No
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
	// 10 Res 채팅수신 (아무때나 올 수 있음)  (내건 오지 않음)
	//
	// 4Byte : 송신자 No
	//
	// 2Byte : 메시지 Size
	// Size  : 대화내용(유니코드)
	//------------------------------------------------------------
	DWORD SendID;
	WORD wMsgSize;

	// 4Byte : 송신자 No
	*Packet >> SendID;

	// 2Byte : 메시지 Size
	*Packet >> wMsgSize;

	// Size  : 대화내용(유니코드)
	WCHAR* szMsg = new WCHAR[(wMsgSize + 1) / 2];
	Packet->Dequeue((char*)szMsg, wMsgSize);
	szMsg[wMsgSize / 2] = L'\0';

	Addlistbox_Chat(FindUser(SendID), szMsg);

	delete[] szMsg;
}

void OnRecv_RoomLeave(CSerialBuffer * Packet)
{
	//------------------------------------------------------------
	// 12 Res 방퇴장 (수시)
	//
	// 4Byte : 사용자 No
	//------------------------------------------------------------
	DWORD dwLeaveID;
	*Packet >> dwLeaveID;

	
	Deletelistbox_ChatUser(FindUser(dwLeaveID));
	DeleteUser(dwLeaveID);
}


void OnRecv_RoomDelete(mylib::CSerialBuffer * Packet)
{
	//------------------------------------------------------------
	// 13 Res 방삭제 (수시)
	//
	// 4Byte : 방 No
	//------------------------------------------------------------
	DWORD dwDeleteRoomID;
	*Packet >> dwDeleteRoomID;

	Deletelistbox_Room(FindRoom(dwDeleteRoomID));
	DeleteRoom(dwDeleteRoomID);
}

void OnRecv_UserEnter(mylib::CSerialBuffer * Packet)
{
	//------------------------------------------------------------
	// TODO:14 Res 타 사용자 입장 (수시)
	//
	// WCHAR[15] : 닉네임(유니코드)
	// 4Byte : 사용자 No
	//------------------------------------------------------------
	DWORD dwEnterID;

	// WCHAR[15] : 닉네임(유니코드)
	WCHAR szNickname[dfNICK_MAX_LEN];
	Packet->Dequeue((char*)szNickname, sizeof(szNickname));

	// 4Byte : 사용자 No
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
