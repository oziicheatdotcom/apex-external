#include "global.h"
#include "lock.h"

SOCKET g_Sock;
DWORD g_PID;
uint64_t g_Base;
bool g_Locked;

//#define SMOOTH 2.0f

Vector GetEntityBasePosition(uintptr_t ent)
{
	return driver::read<Vector>(g_Sock, g_PID, ent + OFFSET_ORIGIN);
}

uintptr_t GetEntityBoneArray(uintptr_t ent)
{
	return driver::read<uintptr_t>(g_Sock, g_PID, ent + OFFSET_BONES);
}

Vector GetEntityBonePosition(uintptr_t ent, uint32_t BoneId, Vector BasePosition)
{
	unsigned long long pBoneArray = GetEntityBoneArray(ent);

	Vector EntityHead = Vector();

	EntityHead.x = driver::read<float>(g_Sock, g_PID, pBoneArray + 0xCC + (BoneId * 0x30)) + BasePosition.x;
	EntityHead.y = driver::read<float>(g_Sock, g_PID, pBoneArray + 0xDC + (BoneId * 0x30)) + BasePosition.y;
	EntityHead.z = driver::read<float>(g_Sock, g_PID, pBoneArray + 0xEC + (BoneId * 0x30)) + BasePosition.z;

	return EntityHead;
}

Vector GetViewAngles(uintptr_t ent) 
{
	return driver::read<Vector>(g_Sock, g_PID, ent + OFFSET_VIEWANGLES);
}

QAngle GetViewAnglesA(uintptr_t ent)
{
	return driver::read<QAngle>(g_Sock, g_PID, ent + OFFSET_VIEWANGLES);
}

void SetViewAngles(uintptr_t ent, Vector angles)
{
	driver::write<Vector>(g_Sock, g_PID, ent + OFFSET_VIEWANGLES, angles);
}

Vector GetCamPos(uintptr_t ent)
{
	return driver::read<Vector>(g_Sock, g_PID, ent + OFFSET_CAMERAPOS);
}

/*void dump(uintptr_t baseaddr, uintptr_t size, int readsize)
{
	FILE* pFile;
	pFile = fopen("dump.bin", "wb");
	for (uintptr_t i = baseaddr; i < (baseaddr + size); i += readsize)
	{
		uint64_t readshit = driver::read<uint64_t>(g_Sock, g_PID, i);
		fwrite(&readshit, 1, readsize, pFile);
		Console::Debug("%llx", i);
		
	}
	fclose(pFile);
}*/

uint64_t aimentity = 0;
void aimthread() 
{	
	while (!(GetKeyState(0x73) & 0x8000))
	{		
		int aimkey = 0x06;
		int out = sscanf(s_Aimkey, "%d", &aimkey);
		
		if (!(GetKeyState(aimkey) & 0x8000) || !s_Aim) 
		{
			Sleep(10);
			continue;
		}
		
		while (g_Locked) Sleep(1);
		g_Locked = true;
		Sleep(20);

		uint64_t localent = driver::read<uint64_t>(g_Sock, g_PID, g_Base + OFFSET_LOCAL_ENT);
		Vector LocalCamera = GetCamPos(localent);
		Vector ViewAngles = GetViewAngles(localent);
		Vector FeetPosition = GetEntityBasePosition(aimentity);
		Vector HeadPosition = GetEntityBonePosition(aimentity, 8, FeetPosition);
		QAngle angle = Math::CalcAngle(LocalCamera, HeadPosition);
		//Math::ClampAngles(angle);
		Vector CalculatedAngles = *(Vector*)(&(angle));

		Vector Delta = (CalculatedAngles - ViewAngles); // / SMOOTH;

		if (s_Smooth != 0)
		{
			Delta = (CalculatedAngles - ViewAngles) / s_Smooth;
		}

		Vector puredelta = CalculatedAngles - ViewAngles;
		Vector SmoothedAngles = ViewAngles + Delta;

		if (s_Recoil) 
		{
			Vector RecoilVec = driver::read<Vector>(g_Sock, g_PID, localent + OFFSET_AIMPUNCH);
			if (RecoilVec.x != 0 || RecoilVec.y != 0) 
			{
				SmoothedAngles -= RecoilVec;
			}				
		}

		if (abs(puredelta.y) > s_FOV || abs(puredelta.x) > s_FOV)
		{
			g_Locked = false;
			continue;
		}

		if (localent < 0x77359400) 
		{
			g_Locked = false;
			continue;
		}

		SetViewAngles(localent, SmoothedAngles);

		g_Locked = false;
	}
}

int main()
{	
	JUNK();
	Console::PrintTitle("aaaapex");

	JUNK();
	Console::Info("Connecting to driver...");
	
	JUNK();
	Console::Debug("Initializing...");
	driver::initialize();
	g_Locked = false;

	JUNK();
	Console::Debug("Connecting...");
	g_Sock = driver::connect();
	if (g_Sock == INVALID_SOCKET)
	{
		Console::Error("Connection failed!");
		Console::WaitAndExit();
	}

	JUNK();
	Console::Info("Getting PID of game...");

	JUNK();
	Console::Debug("Getting PID using CreateToolhelp32Snapshot...");
	g_PID = Utils::GetPID(L"r5apex.exe");
	if (g_PID == 0) 
	{
		Console::Error("Failed to get PID! Please make sure the game is running.");
		Console::WaitAndExit();
	}
	Console::Debug("Game PID should be %u.", g_PID);

	JUNK();
	Console::Info("Getting base address...");

	JUNK();
	Console::Debug("Using driver to obtain base...");
	g_Base = driver::get_process_base_address(g_Sock, g_PID);
	if (g_Base == 0) 
	{
		Console::Error("Failed  to get base address!");
		Console::WaitAndExit();
	}
	Console::Debug("Base address should be %llx.", g_Base);

	JUNK();
	Console::Info("Running threads...");
	Console::Debug("Aimbot thread...");
	std::thread taim(aimthread);
	Console::Debug("GUI thread...");
	std::thread tgui(rungui);

	JUNK();
	//dump(g_Base, 3221225472, 8);

	while (!(GetKeyState(0x73) & 0x8000)) // F4
	{
		JUNK();
		Sleep(1);
		uint64_t entitylist = g_Base + OFFSET_ENTITYLIST;

		while (g_Locked) Sleep(1);
		g_Locked = true;
		Sleep(30);
		
		uint64_t baseent = driver::read<uint64_t>(g_Sock, g_PID, entitylist);
		if (baseent == 0) 
		{
			continue;
		}

		Vector vec = { 0, 0, 0 };
		float max = 999.0f;
		for (int i = 0; i <= 150; i++) // TODO: Check if 1 is local
		{			
			// ---------------------------------------------------------------
			// ENTITY BASE CODE	
			// ---------------------------------------------------------------
			uint64_t centity = driver::read<uint64_t>(g_Sock, g_PID, entitylist + ((uint64_t)i << 5));

			// ---------------------------------------------------------------
			// INDENTIFICATION
			// ---------------------------------------------------------------	
			uint64_t name = driver::read<uint64_t>(g_Sock, g_PID, centity + OFFSET_NAME);
			if (name != 125780153691248)  // "player.."
			{
				continue;
			}

			// ---------------------------------------------------------------
			// ENGINE GLOW	
			// ---------------------------------------------------------------			
			int health = driver::read<int>(g_Sock, g_PID, centity + OFFSET_HEALTH);
			if (health < 1 || health > 100)
				continue;

			int shield = driver::read<int>(g_Sock, g_PID, centity + OFFSET_SHIELD);
			int total = health + shield;		

			float green = 0;
			float red = 0;
			float blue = 0;

			if (s_Health) 
			{
				if (s_Shield) 
				{
					if (total > 100)
					{
						total -= 100;
						blue = (float)total / 125.f;
						green = (125.f - (float)total) / 125.f;
					}
					else
					{
						green = (float)total / 100.f;
						red = (100.f - (float)total) / 100.f;
					}
				}
				else 
				{
					green = (float)total / 100.f;
					red = (100.f - (float)total) / 100.f;
				}
			}

			if (s_Glow) 
			{
				driver::write(g_Sock, g_PID, centity + OFFSET_GLOW_ENABLE, true);
				driver::write(g_Sock, g_PID, centity + OFFSET_GLOW_CONTEXT, 1);

				// TODO: Color by health
				driver::write(g_Sock, g_PID, centity + OFFSET_GLOW_COLORS, red);
				driver::write(g_Sock, g_PID, centity + OFFSET_GLOW_COLORS + 0x4, green);
				driver::write(g_Sock, g_PID, centity + OFFSET_GLOW_COLORS + 0x8, blue);

				for (int offset = 0x2D0; offset <= 0x2E8; offset += 0x4)
					driver::write(g_Sock, g_PID, centity + offset, FLT_MAX);

				driver::write(g_Sock, g_PID, centity + OFFSET_GLOW_RANGE, FLT_MAX);
			}

			// ---------------------------------------------------------------
			// AIMBOT PREPARATION
			// ---------------------------------------------------------------			
			uint64_t localent = driver::read<uint64_t>(g_Sock, g_PID, g_Base + OFFSET_LOCAL_ENT);

			if (localent == centity) 
			{
				continue;
			}

			Vector LocalCamera = GetCamPos(localent);
			Vector ViewAngles = GetViewAngles(localent);
			QAngle ViewAnglesA = GetViewAnglesA(localent);
			Vector FeetPosition = GetEntityBasePosition(centity);
			Vector HeadPosition = GetEntityBonePosition(centity, 8, FeetPosition);
			QAngle angle = Math::CalcAngle(LocalCamera, HeadPosition);

			float fov = Math::GetFov(ViewAnglesA, angle);

			if (fov < max)
			{
				max = fov;
				aimentity = centity;
			}
		}
		g_Locked = false;
	}
	
	JUNK();
	Console::Debug("Deinitializing...");
	driver::deinitialize();

	JUNK();
	std::cout << "Press any key to exit...\n";
	Console::WaitForInput();
	return 0;
}
