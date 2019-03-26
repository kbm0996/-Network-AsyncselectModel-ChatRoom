// stdafx.h : ���� ��������� ���� ��������� �ʴ�
// ǥ�� �ý��� ���� ���� �Ǵ� ������Ʈ ���� ���� ������
// ��� �ִ� ���� �����Դϴ�.
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // ���� ������ �ʴ� ������ Windows ������� �����մϴ�.
// Windows ��� ����:
#include <windows.h>

// C ��Ÿ�� ��� �����Դϴ�.
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>


// TODO: ���α׷��� �ʿ��� �߰� ����� ���⿡�� �����մϴ�.
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

// ���� ����:
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