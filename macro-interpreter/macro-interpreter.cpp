#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <ctime>
#include <windows.h>
#include <winuser.h>
#include <wingdi.h>
#include <windef.h>
#include <mmsystem.h>
#include <fstream>
#include <thread>
#include <string>
#include <set>
#include <timeapi.h>

//#define directx true

using namespace std;

void click(int key);
void mouseClick(int key);//left:key=0,right:key=1
void mousePress(int key);
void mouseRelease(int key);
void mgoto(int, int);
void press(int);
void release(int);
void getCursorAndKeyMode();

void runMacro(int listNum);
void readMacro();
void pause(int t);//sleep(t-1) then spinlock rest(~0.5ms)

BOOL IsUserAdmin(VOID);

bool macroExist = false;
bool macroSwitch = false;
bool macroBreaker = false;
int targetKey = 0;
bool scrollLockDisable = false;
bool verboseMode = false;

int latency = 0;
int macroLineNum = 1;
string macroStr[1001];//idx0 is reserved
struct input
{
	int type;
	int key;
	int x;
	int y;
	int time;
	string sysCmd;
	//int line;
};

vector<input> macroList[3];
int macroCount = 0;
bool directx = false;
string macroFileName = "macro.txt";

/*-------this toggle the getCursorMode-------*/
bool cursorMode = false;
/*-------------------------------------------*/

/*some key should be specified the extend key mode
see https://stackoverflow.com/questions/21197257/keybd-event-keyeventf-extendedkey-explanation-required
and http://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/scancode.doc
and scancode.doc page 18~23*/
set<int> extendKey = { 33,34,35,36,37,38,39,40,44,45,46,111,163,164 };

string mainName = "";
TIMECAPS tc;
//string restartCmd = "";
LARGE_INTEGER m_liPerfFreq = { 0 };
double clockPerMs;
int sleepOffset = 1;

int main(int argc, char* argv[])
{
	/*----------init qpc for pause()----------*/
	QueryPerformanceFrequency(&m_liPerfFreq);
	clockPerMs = (double)m_liPerfFreq.QuadPart / 1000;
	/*----------------------------------------*/
	/*----------get time cap(winapi sleep() time resolution)----------*/
	//TIMECAPS tc;
	UINT     wTimerRes;

	if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR)
	{
		// Error; application can't continue.
		cout << "Error occured when getting the time cap" << endl;
		system("pause");
		return 1;
	}
	sleepOffset = tc.wPeriodMin + 1;//set sleep time offset
	//cout << tc.wPeriodMin;
	//system("pause");
	/*----------------------------------------------------------------*/

	//deal with main name
	mainName = string(argv[0]);
	if (argc > 2)
	{
		for (int i = 1; i < argc; i++)
			ShellExecuteA(NULL, "open", mainName.c_str(), argv[i], NULL, SW_SHOWNORMAL);
		return 0;
	}
	if (argc == 2)
		macroFileName = argv[1];
	SetConsoleTitleA(macroFileName.substr(macroFileName.rfind("\\") + 1, (macroFileName.rfind(".") - macroFileName.rfind("\\") - 1)).c_str());
	bool first = true;
	while ((GetKeyState(VK_SCROLL) % 2 == 1))
	{
		Sleep(100);
		if (first)
		{
			cout << "Warning:ScrollLock is on now, turn it off to start" << endl;
			first = false;
		}
	}
	bool flag = false;
	thread macroThread;
	thread cursorThread;
	ifstream f;
	f.open(macroFileName, std::fstream::in);
	if (f)
	{
		string bufStr;
		macroExist = true;
		getline(f, bufStr);//debug mode or directx mode or scrdis or targetKey or latency
		if (bufStr == "debug")
		{
			cursorMode = true;
			cursorThread = thread(getCursorAndKeyMode);
			getline(f, bufStr);//directx mode or scrdis or targetKey or latency
		}
		if (bufStr == "directx")
		{
			if (!IsUserAdmin())
			{
				ShellExecuteA(NULL, "runas", mainName.c_str(), macroFileName.c_str(), mainName.substr(0, mainName.rfind("\\") + 1).c_str(), SW_SHOWNORMAL);
				cout << mainName.substr(0, mainName.rfind("\\") + 1);
				return 0;
			}
			directx = true;
			getline(f, bufStr);//targetKey or latency
		}
		if (bufStr == "scrdis")
		{
			scrollLockDisable = true;
			getline(f, bufStr);//targetKey or latency
		}
		if (bufStr == "verbose")
		{
			verboseMode = true;
			getline(f, bufStr);//targetKey or latency
		}

		if (bufStr[0] == 'F')
		{
			targetKey = bufStr[1] - '0';
			if (bufStr.length() == 3)
			{
				targetKey *= 10;
				targetKey += bufStr[2] - '0';
			}
			targetKey += 111;
			getline(f, bufStr);//latency
		}
		else if (bufStr[0] == 'X')//X1 and X2 are the forth and fifth button of mouse
		{
			targetKey = bufStr[1] - '0';
			targetKey += 4;
			if (targetKey == 4 || targetKey >= 7)
			{
				cout << "Invalid targetKey:X" << targetKey - 4 << endl;
				if (cursorMode)
				{
					cursorMode = false;
					cursorThread.join();
				}
				system("pause");
				return 1;
			}
			getline(f, bufStr);//latency
		}

		if (scrollLockDisable && targetKey == 0)
		{
			cout << "Invalid trigger: Both scrollLock and targetKey are disabled" << endl;
			if (cursorMode)
			{
				cursorMode = false;
				cursorThread.join();
			}
			system("pause");
			return 1;
		}

		latency = 0;
		for (int k = 0; k < bufStr.length(); k++)
		{
			if (bufStr[k] > '9' || bufStr[k] < '0')
			{
				latency = -1;
				break;
			}
			latency *= 10;
			latency += bufStr[k] - '0';
		}
		if (latency <= 0)
		{
			cout << "Invalid latency:" << bufStr << endl;
			if (cursorMode)
			{
				cursorMode = false;
				cursorThread.join();
			}
			system("pause");
			return 1;
		}

		while (getline(f, macroStr[macroLineNum++]))
		{
			if (macroStr[macroLineNum - 1][0] == '%')
				macroCount++;
			if (macroLineNum >= 1001)
			{
				cout << "Macro file can not over 1000 lines" << endl;
				macroExist = false;
				break;
			}
		}
		macroLineNum--;//because last line is empty
		//cout << "Read macro secessfully" << endl;
		cout << "[Latency]:" << latency << " [TimeCap]:" << tc.wPeriodMin <<
			" [Directx]:" << (directx ? "Enable" : "Disable");
		cout << " [ScrollLock]:" << (scrollLockDisable ? "Disable" : "Enable");
		if (targetKey != 0)
			if (targetKey == 5 || targetKey == 6)
				cout << " [TargetKey]:X" << targetKey - 4 << endl;
			else
				cout << " [TargetKey]:F" << targetKey - 111 << endl;
		else
			cout << endl;
		/*cout << "----------Macro----------" << endl;
		for (int i = 1; i < macroLineNum; i++)
			cout << macroStr[i] << endl;
		cout << "-------------------------" << endl;*/
	}
	else
	{
		cout << "Macro not found" << endl;
		if (cursorMode)
		{
			cursorMode = false;
			cursorThread.join();
		}
		system("pause");
		return 1;
	}
	f.close();

	readMacro();
	int focusId = 0, nowFocusId = 0;
	LPDWORD tempOnly = 0;
	bool firstRun = true;
	while (true)
	{
		Sleep(100);
		if ((!scrollLockDisable && (GetKeyState(VK_SCROLL) % 2 == 1)) || (targetKey != 0 && (GetKeyState(targetKey) & 0x8000)))
		{
			//this part is to detect whether user change focus windows,
			//and stop the macro.
			nowFocusId = GetWindowThreadProcessId(GetForegroundWindow(), tempOnly);
			if (focusId != nowFocusId && focusId != 0)
			{
				cout << "Focus on window change detected" << endl;
				cout << "Now:" << nowFocusId << " Macro begin:" << focusId << endl;
				//--------------
				if (macroSwitch)
				{
					//cout << "pause" << endl;
					macroSwitch = false;
					macroBreaker = true;//means interrrupt immediately
					if (macroThread.joinable())
						macroThread.join();
					macroBreaker = false;
				}
				//--------------
				Sleep(400);
				continue;
			}
			//-------------------------
			if (!macroSwitch)
			{
				if (macroThread.joinable())
					macroThread.join();
				focusId = GetWindowThreadProcessId(GetForegroundWindow(), tempOnly);
				macroSwitch = true;
				if (macroList[0].size() != 0)
					runMacro(0);
				macroThread = thread(runMacro, 1);
			}
			else
				continue;
		}
		else
		{
			focusId = 0;
			if (macroSwitch)
			{
				macroSwitch = false;
				macroThread.join();
				if (macroList[2].size() != 0)
					runMacro(2);
			}
		}
	}
}


void mouseClick(int key)
{
	INPUT ip[2] = { 0 };
	ip[0].type = INPUT_MOUSE;
	ip[1].type = INPUT_MOUSE;
	ip[0].mi.mouseData = 0;
	ip[1].mi.mouseData = 0;
	ip[0].mi.time = 0;
	ip[1].mi.time = 0;
	if (key == 0)
	{
		ip[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
		ip[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
	}
	else if (key == 1)
	{
		ip[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
		ip[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
	}
	SendInput(1, ip, sizeof(INPUT));
	pause(latency);
	//Sleep(latency);
	SendInput(1, &ip[1], sizeof(INPUT));
	pause(latency);
	//Sleep(latency);
}

void mousePress(int key)
{
	INPUT ip = { 0 };
	ip.type = INPUT_MOUSE;
	ip.mi.mouseData = 0;
	ip.mi.time = 0;
	if (key == 0)
		ip.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
	else if (key == 1)
		ip.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
	SendInput(1, &ip, sizeof(INPUT));
	pause(latency);
	//Sleep(latency);
}

void mouseRelease(int key)
{
	INPUT ip = { 0 };
	ip.type = INPUT_MOUSE;
	ip.mi.mouseData = 0;
	ip.mi.time = 0;
	if (key == 0)
		ip.mi.dwFlags = MOUSEEVENTF_LEFTUP;
	else if (key == 1)
		ip.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
	SendInput(1, &ip, sizeof(INPUT));
	pause(latency);
	//Sleep(latency);
}

void click(int key)
{
	INPUT ip[2] = { 0 };
	ip[0].type = INPUT_KEYBOARD;
	ip[1].type = INPUT_KEYBOARD;
	ip[0].ki.wVk = key;
	ip[1].ki.wVk = key;
	ip[1].ki.dwFlags = KEYEVENTF_KEYUP;
	if (directx)
	{
		ip[0].ki.wScan = MapVirtualKey(key, 0);//MAPVK_VK_TO_VSC=0
		ip[1].ki.wScan = MapVirtualKey(key, 0);
		ip[0].ki.dwFlags |= KEYEVENTF_SCANCODE;
		ip[1].ki.dwFlags |= KEYEVENTF_SCANCODE;
		if (extendKey.find(key) != extendKey.end())
		{
			ip[0].ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
			ip[1].ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
		}
	}
	/*------*/
	//ip[0].ki.dwFlags |= KEYEVENTF_UNICODE;
	//ip[1].ki.dwFlags |= KEYEVENTF_UNICODE;
	/*------*/
	SendInput(1, ip, sizeof(INPUT));
	pause(latency);
	//Sleep(latency);
	SendInput(1, &ip[1], sizeof(INPUT));
	pause(latency);
	//Sleep(latency);
}

void press(int key)
{
	INPUT ip = { 0 };
	ip.type = INPUT_KEYBOARD;
	ip.ki.wVk = key;
	if (directx)
	{
		ip.ki.wScan = MapVirtualKey(key, 0);
		ip.ki.dwFlags |= KEYEVENTF_SCANCODE;
		if (extendKey.find(key) != extendKey.end())
		{
			ip.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
		}
	}
	SendInput(1, &ip, sizeof(INPUT));
	pause(latency);
	//Sleep(latency);
}

void release(int key)
{
	INPUT ip = { 0 };
	ip.type = INPUT_KEYBOARD;
	ip.ki.wVk = key;
	ip.ki.dwFlags = KEYEVENTF_KEYUP;
	if (directx)
	{
		ip.ki.wScan = MapVirtualKey(key, 0);
		ip.ki.dwFlags |= KEYEVENTF_SCANCODE;
		if (extendKey.find(key) != extendKey.end())
		{
			ip.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
		}
	}
	SendInput(1, &ip, sizeof(INPUT));
	pause(latency);
	//Sleep(latency);
}

void mgoto(int x, int y)
{
	if (x < 0)x = 0;
	if (y < 0)y = 0;

	INPUT ip = { 0 };
	ip.type = INPUT_MOUSE;
	ip.mi.dx = x * (65536 / GetSystemMetrics(SM_CXSCREEN));
	ip.mi.dy = y * (65536 / GetSystemMetrics(SM_CYSCREEN));
	ip.mi.mouseData = 0;
	ip.mi.time = 0;
	ip.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
	if (GetKeyState(1) % 2 == 1)
		ip.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
	if (GetKeyState(2) % 2 == 1)
		ip.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
	if (GetKeyState(4) % 2 == 1)
		ip.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
	SendInput(1, &ip, sizeof(INPUT));
	pause(latency);
	//Sleep(latency);
}

void mgotor(int x, int y)
{

	INPUT ip = { 0 };
	ip.type = INPUT_MOUSE;
	ip.mi.dx = x;
	ip.mi.dy = y;
	ip.mi.mouseData = 0;
	ip.mi.time = 0;
	ip.mi.dwFlags = MOUSEEVENTF_MOVE;
	if (GetKeyState(1) % 2 == 1)
		ip.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
	if (GetKeyState(2) % 2 == 1)
		ip.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
	if (GetKeyState(4) % 2 == 1)
		ip.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
	SendInput(1, &ip, sizeof(INPUT));
	pause(latency);
	//Sleep(latency);
}

void runMacro(int listNum)
{
	if (macroList[listNum].size() == 0)
		return;
	switch (listNum)
	{
	case 0:
		cout << "--- Pre-macro start ---" << endl;
		break;
	case 1:
		if (macroCount == 0)
			cout << "--- Macro start ---" << endl;
		else
			cout << "--- Main-macro start ---" << endl;
		break;
	case 2:
		cout << "--- Post-macro start ---" << endl;
		break;
	}
	for (int i = 0; i <= macroList[listNum].size(); i++)
	{
		if (macroBreaker)
		{
			if (macroCount == 0)
				cout << "--- Macro interrupt ---" << endl << endl;
			else
				cout << "--- Main-macro interrupt ---" << endl;
			return;
		}
		if (i == macroList[listNum].size())
		{
			if (listNum == 1)
			{
				if (!macroSwitch)
				{
					if (macroCount == 0)
						cout << "--- Macro stop ---" << endl << endl;
					else
						cout << "--- Main-macro stop ---" << endl;
					return;
				}
				else
				{
					i = -1;
					continue;
				}
			}
			else
				break;
		}
		//cout << "type:" << macroList[i].type << " key:" << macroList[i].key << endl;
		//printf("Line%3d: ", macroList[i].line);
		switch (macroList[listNum][i].type)
		{
		case 1:
			if (verboseMode) {
				if (macroList[listNum][i].key >= 'A' && macroList[listNum][i].key <= 'Z')
					cout << "click " << (char)macroList[listNum][i].key << endl;
				else
					cout << "click " << macroList[listNum][i].key << endl;
			}
			click(macroList[listNum][i].key);
			break;
		case 2:
			if (verboseMode) {
				if (macroList[listNum][i].key >= 'A' && macroList[listNum][i].key <= 'Z')
					cout << "press " << (char)macroList[listNum][i].key << endl;
				else
					cout << "press " << macroList[listNum][i].key << endl;
			}
			press(macroList[listNum][i].key);
			break;
		case 3:
			if (verboseMode) {
				if (macroList[listNum][i].key >= 'A' && macroList[listNum][i].key <= 'Z')
					cout << "release " << (char)macroList[listNum][i].key << endl;
				else
					cout << "release " << macroList[listNum][i].key << endl;
			}
			release(macroList[listNum][i].key);
			break;
		case 4:
			if (verboseMode) {
				cout << "mgoto " << macroList[listNum][i].x << "," << macroList[listNum][i].y << endl;
			}
			mgoto(macroList[listNum][i].x, macroList[listNum][i].y);
			break;
		case 5:
			if (verboseMode) {
				cout << "mgotor " << macroList[listNum][i].x << "," << macroList[listNum][i].y << endl;
			}
			mgotor(macroList[listNum][i].x, macroList[listNum][i].y);
			break;
		case 6:
			if (verboseMode) {
				cout << "pause " << macroList[listNum][i].time << endl;
			}
			pause(macroList[listNum][i].time);
			//Sleep(macroList[listNum][i].time);
			break;
		case 7:
			if (macroList[listNum][i].key < 2)
			{
				if (verboseMode) {
					if (macroList[listNum][i].key == 0)
						cout << "mouse " << "left" << endl;
					else
						cout << "mouse " << "right" << endl;
				}
				mouseClick(macroList[listNum][i].key);
			}
			else
			{
				switch (macroList[listNum][i].key)
				{
				case 2:
					if (verboseMode) {
						cout << "mouse " << "left press" << endl;
					}
					mousePress(0);
					break;
				case 3:
					if (verboseMode) {
						cout << "mouse " << "left release" << endl;
					}
					mouseRelease(0);
					break;
				case 4:
					if (verboseMode) {
						cout << "mouse " << "right press" << endl;
					}
					mousePress(1);
					break;
				case 5:
					if (verboseMode) {
						cout << "mouse " << "right release" << endl;
					}
					mouseRelease(1);
					break;
				}
			}
			break;
		case 8:
			if (verboseMode) {
				cout << "system " << macroList[listNum][i].sysCmd << endl;
			}
			system(macroList[listNum][i].sysCmd.c_str());
			break;
		}
	}
	switch (listNum)
	{
	case 0:
		cout << "--- Pre-macro end ---" << endl;
		break;
	case 1:
		if (macroCount == 0)
			cout << "--- Macro end ---" << endl << endl;
		else
			cout << "--- Main-macro end ---" << endl;
		break;
	case 2:
		cout << "--- Post-macro end ---" << endl << endl;
	}
}


void readMacro()
{
	stringstream ss;
	string type;
	string key;
	int vk_key, x, y, time;
	int listNum = 1;
	//cout << "macro start" << endl;
	cout << "----------Macro----------" << endl;
	if (macroCount != 0)
	{
		if (macroCount != 2)
		{
			cout << "Unknown format" << endl;
			exit(1);
		}
		cout << "pre-macro part:" << endl;
	}
	for (int i = 1; i < macroLineNum; i++)
	{
		if (macroStr[i].length() == 0)
		{
			cout << endl;
			continue;
		}
		ss.str("");
		ss.clear();
		ss.str(macroStr[i]);
		ss >> type;
		if (type[0] == '%')
		{
			for (int j = 0; j < macroList[listNum].size(); j++)
			{
				macroList[listNum - 1].push_back(macroList[listNum][j]);
			}
			macroList[listNum].clear();
			if (macroCount == 2)
			{
				cout << "main-macro part:" << endl;
				listNum++;
				macroCount--;
			}
			else if (macroCount == 1)
			{
				cout << "post-macro part:" << endl;
			}

			continue;
		}
		input* cmd = new input;
		if (type == "click")
		{
			cmd->type = 1;
			cout << "click ";
		}
		else if (type == "press")
		{
			cmd->type = 2;
			cout << "press ";
		}
		else if (type == "release")
		{
			cmd->type = 3;
			cout << "release ";
		}
		else if (type == "mgoto")
		{
			cmd->type = 4;
			cout << "mgoto ";
		}
		else if (type == "mgotor")
		{
			cmd->type = 5;
			cout << "mgotor ";
		}
		else if (type == "pause")
		{
			cmd->type = 6;
			cout << "pause ";
		}
		else if (type == "mouse")
		{
			cmd->type = 7;
			cout << "mouse ";
		}
		else if (type == "system")
		{
			cmd->type = 8;
			cout << "system ";
		}
		else
			cmd->type = 0;
		ss.get();
		switch (cmd->type)
		{
		case 1:
		case 2:
		case 3:
			if (ss.peek() >= '0' && ss.peek() <= '9')
			{
				ss >> vk_key;
				if (vk_key >= 'A' && vk_key <= 'Z')
					cout << (char)vk_key << endl;
				else
					cout << vk_key << endl;
			}
			else
			{
				ss >> key;
				if (key[0] >= 'a' && key[0] <= 'z') key[0] -= 32;
				vk_key = key[0];
				cout << key[0] << endl;
			}
			cmd->key = vk_key;
			break;
		case 4:
		case 5:
			ss >> x >> y;
			cout << x << "," << y << endl;
			cmd->x = x;
			cmd->y = y;
			break;
		case 6:
			ss >> time;
			cout << time << endl;
			cmd->time = time;
			break;
		case 7:		//  0 ,1  2   3   4   5
			ss >> key;//L, R, L0, L1, R0, R1
			if (key[0] == 'L')
			{
				cout << "left";
				cmd->key = 0;
			}
			else if (key[0] == 'R')
			{
				cout << "right";
				cmd->key = 1;
			}
			if (key.length() > 1)
			{
				cmd->key = (cmd->key + 1) * 2;
				if (key[1] == '0')
				{
					cout << "-press";
				}
				else if (key[1] == '1')
				{
					cout << "-release";
					cmd->key += 1;
				}
			}
			cout << endl;
			break;
		case 8:
			getline(ss, cmd->sysCmd);//should be notice
			cout << cmd->sysCmd << endl;
			cmd->sysCmd = "\"" + cmd->sysCmd + "\"";//brace fold
			break;
		default:
			cout << "unknown type:" << type << endl;
			continue;
		}
		macroList[listNum].push_back(*cmd);
	}
	cout << "-------------------------" << endl << endl;
}

void getCursorAndKeyMode()
{
	BYTE stat[256];
	wstring buf;
	wstring str;
	wstringstream ss;
	POINT pt = { 0 };
	while (cursorMode)
	{
		GetKeyState(0);
		//ref https://stackoverflow.com/questions/45719020/winapi-getkeyboardstate-behavior-modified-by-getkeystate-when-application-is-out
		GetKeyboardState(stat);
		GetCursorPos(&pt);
		str = L"[DEBUG] X: ";
		ss << pt.x << " " << pt.y;// << " " << tc.wPeriodMin;
		ss >> buf;
		str += buf;
		ss >> buf;
		str += L" Y:";
		str += buf;
		str += L" ";
		/*str += L" TRes:";//sleep call time resolution
		ss >> buf;
		str += buf;*/
		str += L" VK:";
		ss.str(L"");
		ss.clear();
		for (int i = 0; i < 256; i++)
		{
			if (stat[i] & 0x80)
				ss << i << L" ";
		}
		while (ss >> buf)
		{
			str += buf;
			str += L" ";
		}
		ss.str(L"");
		ss.clear();
		//cout << endl;
		SetConsoleTitle(str.c_str());
		//cout<<"X:"<<pt.x<<" Y:"<<pt.y<<endl;
		//cout<<str<<endl;
		Sleep(200);
	}
}


//from https ://msdn.microsoft.com/en-us/library/aa376389.aspx
BOOL IsUserAdmin(VOID)
/*++
Routine Description: This routine returns TRUE if the caller's
process is a member of the Administrators local group. Caller is NOT
expected to be impersonating anyone and is expected to be able to
open its own process and process token.
Arguments: None.
Return Value:
TRUE - Caller has Administrators local group.
FALSE - Caller does not have Administrators local group. --
*/
{
	BOOL b;
	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
	PSID AdministratorsGroup;
	b = AllocateAndInitializeSid(
		&NtAuthority,
		2,
		SECURITY_BUILTIN_DOMAIN_RID,
		DOMAIN_ALIAS_RID_ADMINS,
		0, 0, 0, 0, 0, 0,
		&AdministratorsGroup);
	if (b)
	{
		if (!CheckTokenMembership(NULL, AdministratorsGroup, &b))
		{
			b = FALSE;
		}
		FreeSid(AdministratorsGroup);
	}

	return(b);
}

/*this is very high precision sleep function
with error less than 0.05ms(when using Sleep((int)(t - 1));)
			less than 0.01%(when using Sleep((int)(t * 0.9));) but higher cpu usage*/
			//with pause(100), cpu usage of former is 0.2%, later is 0.5~1.7%
			//burst error are:35us vs 2us
			//but with almost the same avg error over 100 testcase:<=1us

void pause(int t)
{
	LARGE_INTEGER m_liPerfStart = { 0 };
	QueryPerformanceCounter(&m_liPerfStart);

	//LARGE_INTEGER m_liPerfFreq = { 0 };
	LARGE_INTEGER liPerfNow = { 0 };
	long duration;
	//QueryPerformanceFrequency(&m_liPerfFreq);
	//cout << "freq:" << m_liPerfFreq.QuadPart << endl;
	//double clockPerMs = (double)m_liPerfFreq.QuadPart / 1000;
	//cout << "clockPerMs:" << clockPerMs << endl;
	long clockToWait = round(clockPerMs * t);
	//cout << "clockToWait:" << clockToWait << endl;
	Sleep(t - sleepOffset);
	//long last = 0;
	do
	{
		//last = decodeDulation;
		QueryPerformanceCounter(&liPerfNow);
		duration = (liPerfNow.QuadPart - m_liPerfStart.QuadPart);
	} while (duration < clockToWait);
	//cout << last << endl;
	//cout << decodeDulation << endl;
	//system("pause");
	return;
}