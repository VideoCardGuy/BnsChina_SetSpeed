// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include <MyTools/Character.h>
#include <MyTools/CLSearchBase.h>
#include <MyTools/CLLock.h>
#include <MyTools/Log.h>
#define _SELF L"dllmain.cpp"

#define SZFILE_NAME_SHAREDINFO	L"Bns_SetSpeed_ShareName"
typedef struct _Bns_Game_Info
{
	BOOL	bExist;				// �Ƿ�ռ��
	WCHAR	wszPlayerName[64];	// ��ɫ��
	float	fSpeed;				// �ٶ�
	DWORD	dwPid;				// ����ID
	BOOL	bKeepAlive;			// ����
}Bns_Game_Info, *PBns_Game_Info;

typedef struct _Bns_Share_Info
{
	Bns_Game_Info GameArray[20];
}Bns_Share_Info, *PBns_Share_Info;

//
PBns_Share_Info g_pSharedInfo		= NULL;
PBns_Game_Info	g_pAccountGame		= NULL;
DWORD			g_dwPersonBase		= NULL;
DWORD			g_dwMoveBase		= NULL;
DWORD			g_dwGameChartBase	= NULL;
DWORD			g_dwHookSpeedAddr	= NULL;
DWORD			g_dwSpeedCALL		= NULL;

BOOL InitMapFile()
{
	//��ȡ�ļ�ӳ��
	HANDLE hFileMap = OpenFileMappingW(FILE_MAP_WRITE | FILE_MAP_READ, FALSE, SZFILE_NAME_SHAREDINFO);
	if (hFileMap == NULL)
		return FALSE;

	g_pSharedInfo = (PBns_Share_Info)::MapViewOfFile(hFileMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(Bns_Share_Info));
	if (!g_pSharedInfo)
	{
		::CloseHandle(hFileMap);
		return FALSE;
	}

	return TRUE;
}

BOOL SetAccountGame()
{
	for (int i = 0; i < 20; ++i)
	{
		if (!g_pSharedInfo->GameArray[i].bExist)
		{
			g_pAccountGame = &g_pSharedInfo->GameArray[i];
			g_pAccountGame->dwPid = ::GetCurrentProcessId();
			g_pAccountGame->bExist = TRUE;
			return TRUE;
		}
	}

	return FALSE;
}

BOOL CALLBACK lpEnumSetWinName(HWND h, LPARAM l)
{
	if (IsWindow(h) && IsWindowVisible(h) && (GetWindowLong(h, GWL_EXSTYLE)&WS_EX_TOOLWINDOW) != WS_EX_TOOLWINDOW && (GetWindowLong(h, GWL_HWNDPARENT) == 0))
	{
		wchar_t str[0x100] = { 0 };
		wchar_t str1[0x100] = { 0 };
		GetWindowText(h, str1, sizeof(str1));
		if (GetClassName(h, str, sizeof(str)) > 0)
		{
			if (MyTools::CCharacter::wstrcmp_my(str, L"LaunchUnrealUWindowsClient"))//����ܱ��������,������
			{
				DWORD PID;
				::GetWindowThreadProcessId(h, &PID);

				if (PID == ::GetCurrentProcessId())
				{
					return false;
				}
			}
		}
	}
	return true;
}

BOOL SearchMemBase()
{
	MyTools::CLSearchBase SearchBase;
	// �����ַ
	g_dwPersonBase = SearchBase.FindBase("807E1000740E", 0x59 - 0x43, 1, 0, L"Client.exe");
	if (g_dwPersonBase == NULL)
	{
		//LogMsgBox(LOG_LEVEL_EXCEPTION, L"InitDLL ʧ�� g_dwPersonBase = NULL!");
		return FALSE;
	}

	// ���ٻ�ַ
	///////////����///////////////////////////////////////////////////////////////
	g_dwMoveBase = SearchBase.FindBase("8b0885c97433", 0x102C4B18 - 0x102C4B00, 0x1, 0x0, L"bsengine_Shipping.dll", 0xFFFFFFFF);
	if (g_dwMoveBase == NULL)
	{
		//LogMsgBox(LOG_LEVEL_EXCEPTION, L"InitDLL ʧ�� g_dwMoveBase = NULL!");
		return FALSE;
	}

	/////////��ͼ�ж�/////////////////////////////////////////////////////////////////
	g_dwGameChartBase = SearchBase.FindBase("81e10080000081f9", 0x06C5CF1D - 0x06C5CEF2, 0x2, 0x0, L"bsengine_Shipping.dll", 0xFFFFFFFF);
	if (g_dwGameChartBase == NULL)
	{
		//LogMsgBox(LOG_LEVEL_EXCEPTION, L"InitDLL ʧ�� g_dwGameChartBase = NULL!");
		return FALSE;
	}

	g_dwHookSpeedAddr = MyTools::CCharacter::ReadDWORD(MyTools::CCharacter::ReadDWORD(MyTools::CCharacter::ReadDWORD(MyTools::CCharacter::ReadDWORD(MyTools::CCharacter::ReadDWORD(MyTools::CCharacter::ReadDWORD(g_dwMoveBase) + 0x3BC) + 0x0) + 0x40) + 0x214) + 0x0) + 0x178;
	g_dwSpeedCALL = MyTools::CCharacter::ReadDWORD(g_dwHookSpeedAddr);

	return TRUE;
}

#define ReadPersonBase	MyTools::CCharacter::ReadDWORD(MyTools::CCharacter::ReadDWORD(MyTools::CCharacter::ReadDWORD(g_dwPersonBase) + 0x50) + 0x80)
#define GetPersonHp		MyTools::CCharacter::ReadDWORD(ReadPersonBase + 0xC0)

float g_fNowSpeed = 1.0f;
int WINAPI HookSpeed(float f, int param)
{
	_asm
	{
		mov ebx, g_dwMoveBase
			mov ebx, [ebx]
			mov ebx, [ebx + 0x3BC]
			mov ebx, [ebx]
			mov ebx, [ebx + 0x40]
			mov ebx, [ebx + 0x214]
			cmp ebx, ecx
			jne EXIT

			lea ebx, f
			fld[ebx]
			lea eax, g_fNowSpeed
			fmul[eax]
			fstp[ebx]
		EXIT:
			push param
				push f
				call g_dwSpeedCALL
	}
}

BOOL FindGameWindows(__in DWORD dwPid, __out HWND* pWnd, __in BOOL bForce)
{
	HWND hChild = NULL;
	do
	{
		hChild = ::FindWindowEx(NULL, hChild, L"LaunchUnrealUWindowsClient", NULL);
		if (hChild)
		{
			DWORD dwRetPid = 0;
			::GetWindowThreadProcessId(hChild, &dwRetPid);
			if (dwRetPid == dwPid)
			{
				if ((IsWindow(hChild) && IsWindowVisible(hChild)) || bForce)
				{
					*pWnd = hChild;
					return TRUE;
				}
				return FALSE;
			}
		}
	} while (hChild);
	return FALSE;
}

HANDLE hWorkThread = NULL;
DWORD WINAPI _WorkThread(LPVOID lpParm)
{
	if (!InitMapFile())
	{
		LOG_MSG_CF(L"ӳ���ڴ�ʧ��!");
		return 0;
	}

	// �ȴ�����Logo����, ������Ϸ����
	LOG_C_D(L"ö�ٴ����С���");
	
	/*while (EnumWindows((WNDENUMPROC)lpEnumSetWinName, NULL))
		Sleep(1000);*/
	DWORD dwTick = ::GetTickCount();
	HWND hWnd = NULL;
	while (::GetTickCount() - dwTick <= 2 * 60 * 1000 && hWnd == NULL)
	{
		FindGameWindows(::GetCurrentProcessId(), &hWnd, FALSE);
		Sleep(1000);
	}

	if (hWnd == NULL)
		FindGameWindows(::GetCurrentProcessId(), &hWnd, TRUE);

	if (hWnd == NULL)
	{
		LOG_MSG_CF(L"�޷��������Լ��Ĵ���, ϴϴ˯��!");
		return 0;
	}

	LOG_C_D(L"�����ڴ��С���");
	if (!SearchMemBase())
	{
		LOG_MSG_CF(L"�������Ѿ�ʧЧ��!");
		return 0;
	}

	// �ȴ���ȫ������Ϸ�� ��ȡ����Ϸ��ɫ��
	LOG_C_D(L"�ȴ���ȫ������Ϸ");
	while (ReadPersonBase == NULL)
		Sleep(1000);

	LOG_C_D(L"�ȴ���ȡ��������");
	while (GetPersonHp == NULL)
		Sleep(1000);

	::Sleep(10 * 1000);

	if (!SetAccountGame())
	{
		LOG_MSG_CF(L"20���ʺ��Ѿ���ռ�����!");
		return 0;
	}

	DWORD dwNameAddr = ReadPersonBase + 0x8C;
	UINT uLen = MyTools::CCharacter::ReadDWORD(dwNameAddr - 0x4);
	MyTools::CCharacter::wstrcpy_my(g_pAccountGame->wszPlayerName, (LPCWSTR)MyTools::CCharacter::ReadDWORD(dwNameAddr), uLen);

	//CPrintLog::PrintLog_W(_SELF, __LINE__, L"Party Start! Name:%s", g_pAccountGame->wszPlayerName);
	MyTools::CCharacter::WriteDWORD(g_dwHookSpeedAddr, (DWORD)HookSpeed);
	while (true)
	{
		Sleep(1000);
		g_pAccountGame->bKeepAlive = TRUE;

		if (g_pAccountGame->fSpeed != g_fNowSpeed)
		{
			if (g_pAccountGame->fSpeed == 0.0f)
				g_pAccountGame->fSpeed = 1.0f;
			g_fNowSpeed = g_pAccountGame->fSpeed;
			//CPrintLog::PrintLog_W(_SELF,__LINE__,L"��Ϸ�ٶ����ó�:%.2f", g_fNowSpeed);
		}
	}

	return 0;
}


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		if (hWorkThread == NULL)
			hWorkThread = ::CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)_WorkThread, NULL, NULL, NULL);
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

