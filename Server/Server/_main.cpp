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
	//  c++���� ifstream�̳� ofstream���� ������ ���� ������ �� 
	// ��θ� �Ǵ� ���ϸ� �ѱ��� ���ԵǸ� ���� ������� �������� �ʴ´�.

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