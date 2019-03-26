#include "stdafx.h"
#include "Client.h"

HINSTANCE g_hInst;
HWND g_hWndLobby;
HWND g_hWndRoom;

WCHAR g_szIP[INET_ADDRSTRLEN];
WCHAR g_szNickname[dfNICK_MAX_LEN];
DWORD g_dwUserID;

bool g_bActiveRoomDlg;
bool g_bActiveLobbyDlg;

BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK	IPDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK	LobbyDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK	RoomDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

bool CreateRoomDlg();
bool Addlistbox_Chat(WCHAR * szNickname, WCHAR * szChat);

bool Addlistbox_ChatUser(WCHAR * szNickname, DWORD dwUserID);
bool Deletelistbox_ChatUser(WCHAR * szNickname);

bool Addlistbox_Room(WCHAR * szRoomTitle, DWORD dwRoomID);
bool Deletelistbox_Room(WCHAR * szRoomTitle);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR    lpCmdLine, _In_ int       nCmdShow)
{
    if (!InitInstance (hInstance, nCmdShow))
        return FALSE;

	if (!Connect())
		return FALSE;

	MSG msg;
	BOOL bRet;
    // 기본 메시지 루프입니다.
    while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
    {
		if (bRet == -1)
		{
			int err = GetLastError();
			break;
		}

		//--------------------------------------------------------------------
		// Room 다이얼로그 활성화시 엔터키가 눌리면 채팅 메시지 전송으로 진행
		//--------------------------------------------------------------------
		if (g_bActiveRoomDlg)
		{
			if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN)
			{
				//  엔터키 입력시 채팅 메시지 전송
				// EditControl의 엔터키 입력받기가 애매하므로 윈도우 메시지 루프에서 직접 처리함
				// 단, 현재 활성화된 윈도우를 체크하여 해당 윈도우가 활성화된 경우에만 적용

				//  윈도우 메시지를 꼭 윈도우 프리시저 내부에서 해야하는 것은 아님
				// 네트워크 처리부를 여기에서 처리해도 됨
				SendReq_Chat();
			}
		}

		bRet = 0;
		//--------------------------------------------------------------------
		// 특정 모달리스 윈도우 존재시 메시지 처리 요청
		// 
		// 여기서 해당 모달리스 윈도우의 메시지가 아닌 메세지가 올 수 있음
		// IsDialogMessage 함수로 구분
		//
		//
		// 단, IsDialogMessage와 DispatchMessage 둘 다 호출시 메시지가 두번 처리되는 문제 발생
		// 따라서 특정 윈도우 존재시 IsDialogMessage()함수를 호출하여 TRUE 리턴시
		// 메시지를 내부에서 처리한 것으로 판단하고 하단의 DispatchMessage() 호출 방지
		//--------------------------------------------------------------------
		if (IsWindow(g_hWndRoom))
		{
			bRet = IsDialogMessage(g_hWndRoom, &msg);
			OutputDebugString(L"Room Pro");
		}
		else if (IsWindow(g_hWndLobby))
		{
			bRet = IsDialogMessage(g_hWndLobby, &msg);
			OutputDebugString(L"Lobby Pro");
		}

		//--------------------------------------------------------------------
		// 어떤 윈도우도 처리된게 없다면 기본 처리 동작
		//--------------------------------------------------------------------
		if (!bRet)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
    }
    return (int) msg.wParam;
}


//
//   함수: InitInstance(HINSTANCE, int)
//
//   목적: 인스턴스 핸들을 저장하고 주 창을 만듭니다.
//
//   설명:
//
//        이 함수를 통해 인스턴스 핸들을 전역 변수에 저장하고
//        주 프로그램 창을 만든 다음 표시합니다.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   g_hInst = hInstance; // 인스턴스 핸들을 전역 변수에 저장합니다.

   if (!DialogBox(hInstance, MAKEINTRESOURCE(IDD_IP), NULL, (DLGPROC)IPDlgProc))
	   return FALSE;

   g_hWndLobby = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_CHATLOBBY), NULL, LobbyDlgProc);
   if (g_hWndLobby == NULL)
	   return FALSE;

   ShowWindow(g_hWndLobby, nCmdShow);

   return TRUE;
}

LRESULT CALLBACK IPDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HWND	hEditBoxIP;
	HWND	hEditBoxNick;

	switch (message)
	{
	case WM_INITDIALOG:
		hEditBoxIP = GetDlgItem(hWnd, IDC_IP);
		hEditBoxNick = GetDlgItem(hWnd, IDC_NICKNAME);
		SetWindowText(hEditBoxIP, L"127.0.0.1");
		SetWindowText(hEditBoxNick, L"Guest");
		return TRUE;

	case WM_COMMAND:
		switch (wParam)
		{
		case WM_NETWORK:
			NetworkProc(wParam, lParam);
			return TRUE;

		case IDOK:
			GetDlgItemText(hWnd, IDC_IP, g_szIP, 16);
			GetDlgItemText(hWnd, IDC_NICKNAME, g_szNickname, dfNICK_MAX_LEN);
			SendReq_Login();
			EndDialog(hWnd, 99939);
			return TRUE;
		case IDCANCEL:
			PostQuitMessage(0);
			EndDialog(hWnd, 99939);
			return FALSE;
		}
		break;
	case WM_DESTROY:	// 윈도우 닫기 시 호출
		break;
	}
	return 0;
}

LRESULT CALLBACK LobbyDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	WCHAR	szRoomCreateTitle[128] = { 0, };
	int		iSelIndex;
	DWORD	dwRoomNO;
	HWND	hStaticNick;

	switch (message)
	{
	case WM_NETWORK:
		NetworkProc(wParam, lParam);
		return TRUE;

	case WM_INITDIALOG:
		// 다이얼로그에 대한 초기화
		// 수시로 필요한 고정 컨트롤이 있으면 윈도우 핸들을 미리 얻어둔다거나
		// 리스트박스, 에디트박스, 스태틱 텍스트 컨트롤 등의 초기 데이터 입력 등..
		hStaticNick = GetDlgItem(hWnd, IDC_STATICNICK);
		SetWindowText(hStaticNick, g_szNickname);

		return TRUE;

	case WM_ACTIVATE:
		// 다이얼로그 활성화에 따라서 전역 플래그를 바꿔준다
		// 클릭으로 활성화되거나 Alt-Tab 등으로 활성화되는 경우 두가지 모두 감지
		// 비활성화 되는 경우는 WM_INACTIVE로 체크
		if (wParam == WA_ACTIVE || wParam == WA_CLICKACTIVE)
			g_bActiveLobbyDlg = TRUE;
		else if (wParam == WA_INACTIVE)
			g_bActiveLobbyDlg = FALSE;
		return TRUE;

	case WM_CLOSE:
		PostQuitMessage(0);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_BUTTON_ROOM:
			SendReq_RoomCreate();
			return TRUE;

		case IDC_LIST_ROOM:
			switch (HIWORD(wParam))
			{
			case LBN_DBLCLK:
				// 리스트 박스 더블클릭 방 입장
				// 현재 선택된 리스트 박스의 아이템 얻기
				iSelIndex = SendMessage((HWND)lParam, LB_GETCURSEL, 0, 0);
				if (iSelIndex != LB_ERR)
				{
					dwRoomNO = SendMessage((HWND)lParam, LB_GETITEMDATA, (WPARAM)iSelIndex, 0);
					SendReq_RoomEnter(dwRoomNO);
				}
				break;
			}
			return TRUE;
		}
		return FALSE;
	}
	return FALSE;
}

LRESULT CALLBACK RoomDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		g_hWndRoom = hWnd;
		g_bActiveRoomDlg = TRUE;
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDSEND:
			SendReq_Chat();
			return TRUE;
		}
		return TRUE;
	case WM_CLOSE:
		SendReq_RoomLeave();
		EndDialog(hWnd, 99939);
		g_bActiveRoomDlg = FALSE;
		return TRUE;
	}
	return FALSE;
}

bool CreateRoomDlg()
{
	g_hWndRoom = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_CHATROOM), g_hWndLobby, RoomDlgProc);
	ShowWindow(g_hWndRoom, SW_SHOW);
	return true;
}

bool Addlistbox_Chat(WCHAR * szNickname, WCHAR * szChat)
{
	WCHAR szSendMsg[256] = { 0, };
	wsprintf(szSendMsg, L"%s : ", szNickname);
	wcscat_s(szSendMsg, szChat);

	HWND hListBox = GetDlgItem(g_hWndRoom, IDC_LIST_CONTENT);
	int iIndex = SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)szSendMsg);
	SendMessage(hListBox, LB_SETITEMDATA, (WPARAM)iIndex, 0);
	SendMessage(hListBox, LB_SETCURSEL, (WPARAM)iIndex, 0);
	return true;
}

bool Addlistbox_ChatUser(WCHAR * szNickname, DWORD dwUserID)
{
	HWND hListBox = GetDlgItem(g_hWndRoom, IDC_LIST_CLIENT);
	int iIndex = SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)szNickname);
	SendMessage(hListBox, LB_SETITEMDATA, (WPARAM)iIndex, dwUserID);
	return true;
}

bool Deletelistbox_ChatUser(WCHAR * szNickname)
{
	if (szNickname == nullptr)
		return false;

	HWND hListBox = GetDlgItem(g_hWndRoom, IDC_LIST_CLIENT);
	int iIndex = SendMessage(hListBox, LB_FINDSTRING, 0, (LPARAM)szNickname);
	if (iIndex != LB_ERR)
	{
		SendMessage(hListBox, LB_DELETESTRING, (WPARAM)iIndex, 0);
		return true;
	}
	return false;
}

bool Addlistbox_Room(WCHAR * szRoomTitle, DWORD dwRoomID)
{
	HWND hListBox = GetDlgItem(g_hWndLobby, IDC_LIST_ROOM);
	int iIndex = SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)szRoomTitle);
	SendMessage(hListBox, LB_SETITEMDATA, (WPARAM)iIndex, dwRoomID);
	return true;
}

bool Deletelistbox_Room(WCHAR * szRoomTitle)
{
	if (szRoomTitle == nullptr)
		return false;

	HWND hListBox = GetDlgItem(g_hWndLobby, IDC_LIST_ROOM);
	int iIndex = SendMessage(hListBox, LB_FINDSTRING, 0, (LPARAM)szRoomTitle);
	if (iIndex != LB_ERR)
	{
		SendMessage(hListBox, LB_DELETESTRING, (WPARAM)iIndex, 0);
		return true;
	}
	return false;
}


