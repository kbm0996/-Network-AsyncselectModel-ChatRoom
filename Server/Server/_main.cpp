#include "NetworkProc.h"
#include <locale.h>

void main()
{
	timeBeginPeriod(1);
	
	/////////////////////////////////////////////////////
	// Unicode Standard Output Setting
	//
	/////////////////////////////////////////////////////
	setlocale(LC_ALL, "");
	//  c++에서 ifstream이나 ofstream으로 파일을 열고 생성할 때 
	// 경로명 또는 파일명에 한글이 포함되면 파일 입출력이 동작하지 않는다.

	if (!NetworkInit())
	{
		wprintf(L"Network Initialize Error\n");
		return;
	}

	while (1)
	{
		NetworkProc();
	}

	NetworkClose();
	timeEndPeriod(1);
}