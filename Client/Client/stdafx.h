// stdafx.h : 자주 사용하지만 자주 변경되지는 않는
// 표준 시스템 포함 파일 또는 프로젝트 관련 포함 파일이
// 들어 있는 포함 파일입니다.
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // 거의 사용되지 않는 내용은 Windows 헤더에서 제외합니다.
// Windows 헤더 파일:
#include <windows.h>

// C 런타임 헤더 파일입니다.
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>


// TODO: 프로그램에 필요한 추가 헤더는 여기에서 참조합니다.
#define _WINSOCK_DEPRECATED_NO_WARNINGS

////////////////////////////////////////
// Network & System Option
////////////////////////////////////////
#include <cstdlib>
#include <cstdio>
#include <winSock2.h>
#include <Ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "Winmm.lib")
#include "_Protocol.h"

////////////////////////////////////////
// Container
////////////////////////////////////////
#include <map>
#include "CRingBuffer.h"
#include "CSerialBuffer.h"

////////////////////////////////////////
// Network
////////////////////////////////////////
#define WM_NETWORK (WM_USER+1)
#include "NetworkProc.h"

// 전역 변수:
extern HINSTANCE g_hInst;
extern HWND g_hWndLobby;
extern HWND g_hWndRoom;

extern WCHAR g_szIP[INET_ADDRSTRLEN];
extern WCHAR g_szNickname[dfNICK_MAX_LEN];
extern DWORD g_dwUserID;

extern bool g_bActiveRoomDlg;
extern bool g_bActiveLobbyDlg;

bool CreateRoomDlg();
bool Addlistbox_Chat(WCHAR * szNickname, WCHAR * szChat);

bool Addlistbox_ChatUser(WCHAR * szNickname, DWORD dwUserID);
bool Deletelistbox_ChatUser(WCHAR * szNickname);

bool Addlistbox_Room(WCHAR * szRoomTitle, DWORD dwRoomID);
bool Deletelistbox_Room(WCHAR * szRoomTitle);