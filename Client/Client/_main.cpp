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
    // �⺻ �޽��� �����Դϴ�.
    while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
    {
		if (bRet == -1)
		{
			int err = GetLastError();
			break;
		}

		//--------------------------------------------------------------------
		// Room ���̾�α� Ȱ��ȭ�� ����Ű�� ������ ä�� �޽��� �������� ����
		//--------------------------------------------------------------------
		if (g_bActiveRoomDlg)
		{
			if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN)
			{
				//  ����Ű �Է½� ä�� �޽��� ����
				// EditControl�� ����Ű �Է¹ޱⰡ �ָ��ϹǷ� ������ �޽��� �������� ���� ó����
				// ��, ���� Ȱ��ȭ�� �����츦 üũ�Ͽ� �ش� �����찡 Ȱ��ȭ�� ��쿡�� ����

				//  ������ �޽����� �� ������ �������� ���ο��� �ؾ��ϴ� ���� �ƴ�
				// ��Ʈ��ũ ó���θ� ���⿡�� ó���ص� ��
				SendReq_Chat();
			}
		}

		bRet = 0;
		//--------------------------------------------------------------------
		// Ư�� ��޸��� ������ ����� �޽��� ó�� ��û
		// 
		// ���⼭ �ش� ��޸��� �������� �޽����� �ƴ� �޼����� �� �� ����
		// IsDialogMessage �Լ��� ����
		//
		//
		// ��, IsDialogMessage�� DispatchMessage �� �� ȣ��� �޽����� �ι� ó���Ǵ� ���� �߻�
		// ���� Ư�� ������ ����� IsDialogMessage()�Լ��� ȣ���Ͽ� TRUE ���Ͻ�
		// �޽����� ���ο��� ó���� ������ �Ǵ��ϰ� �ϴ��� DispatchMessage() ȣ�� ����
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
		// � �����쵵 ó���Ȱ� ���ٸ� �⺻ ó�� ����
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
//   �Լ�: InitInstance(HINSTANCE, int)
//
//   ����: �ν��Ͻ� �ڵ��� �����ϰ� �� â�� ����ϴ�.
//
//   ����:
//
//        �� �Լ��� ���� �ν��Ͻ� �ڵ��� ���� ������ �����ϰ�
//        �� ���α׷� â�� ���� ���� ǥ���մϴ�.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   g_hInst = hInstance; // �ν��Ͻ� �ڵ��� ���� ������ �����մϴ�.

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
	case WM_DESTROY:	// ������ �ݱ� �� ȣ��
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
		// ���̾�α׿� ���� �ʱ�ȭ
		// ���÷� �ʿ��� ���� ��Ʈ���� ������ ������ �ڵ��� �̸� ���дٰų�
		// ����Ʈ�ڽ�, ����Ʈ�ڽ�, ����ƽ �ؽ�Ʈ ��Ʈ�� ���� �ʱ� ������ �Է� ��..
		hStaticNick = GetDlgItem(hWnd, IDC_STATICNICK);
		SetWindowText(hStaticNick, g_szNickname);

		return TRUE;

	case WM_ACTIVATE:
		// ���̾�α� Ȱ��ȭ�� ���� ���� �÷��׸� �ٲ��ش�
		// Ŭ������ Ȱ��ȭ�ǰų� Alt-Tab ������ Ȱ��ȭ�Ǵ� ��� �ΰ��� ��� ����
		// ��Ȱ��ȭ �Ǵ� ���� WM_INACTIVE�� üũ
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
				// ����Ʈ �ڽ� ����Ŭ�� �� ����
				// ���� ���õ� ����Ʈ �ڽ��� ������ ���
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


