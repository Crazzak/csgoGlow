//
//  Wall.hpp
//  Wall
//
//  for csgo wall hack
//  special edition
//
//  Created by sheep on 16/5/14.
//  Copyright © 2016年 nadp. All rights reserved.
//
//  Edited by Fricker95 on 1/1/20.

#include "Wall.hpp"

Wall::Wall(double refreshRate, double maxFlash, bool noTeammates) {
	this->refreshRate = refreshRate;
	this->maxFlash = maxFlash;
	this->noTeammates = noTeammates;
	
	stop.store(false);
	
	g_cProc = new Process;
	mem = new MemMngr(g_cProc);
	off = new sOffsets;
	
	g_cProc->mainPid() = g_cProc->get("csgo_osx64");
	
	if (g_cProc->mainPid() != -1){
		printf("Found CSGO's PID\t\t= %s\n", cT::print(std::to_string(g_cProc->mainPid()).c_str(), cT::fG::green, cT::sT::bold).c_str());
		g_cProc->mainTask() = g_cProc->task(g_cProc->mainPid());
	} else {
		printf("%s\n", cT::print("Error: Can't find CSGO's PID", cT::fG::red, cT::sT::bold).c_str());
		deinit();
		exit(0);
	}
	
	if (g_cProc->mainTask()){
		g_cProc->getModule(g_cProc->mainTask(), &engine_moduleStartAddress, &engine_moduleLength, "/engine.dylib");
		
		if (engine_moduleStartAddress != 0x0) {
			printf("Found the %s address\t= %s0x%llx%s ... %s0x%llx%s\n", cT::print("engine.dylib", cT::fG::yellow).c_str(),  cT::getColor(cT::fG::green).c_str(),
				   engine_moduleStartAddress,
				   cT::getStyle(cT::sT::bold).c_str(),
				   cT::getColor(cT::fG::green).c_str(),
				   engine_moduleStartAddress +
				   engine_moduleLength,
				   cT::getStyle(cT::sT::bold).c_str());
		} else {
			printf("%s\n", cT::print("Error: Failed to get the engine.dylib address", cT::fG::red, cT::sT::bold).c_str());
			deinit();
			exit(0);
		}
		
		g_cProc->getModule(g_cProc->mainTask(), &client_moduleStartAddress, &client_moduleLength, "/client.dylib");
		
		if (client_moduleStartAddress != 0x0) {
			printf("Found the %s address\t= %s0x%llx%s ... %s0x%llx%s\n", cT::print("client.dylib", cT::fG::yellow).c_str(),
				   cT::getColor(cT::fG::green).c_str(),
				   client_moduleStartAddress,
				   cT::getStyle(cT::sT::bold).c_str(),
				   cT::getColor(cT::fG::green).c_str(),
				   client_moduleStartAddress +
				   client_moduleLength,
				   cT::getStyle(cT::sT::bold).c_str());
		} else {
			printf("%s\n", cT::print("Error: Failed to get the client.dylib address", cT::fG::red, cT::sT::bold).c_str());
			deinit();
			exit(0);
		}
	} else {
		printf("%s\n", cT::print("Error: Failed to find main task", cT::fG::red, cT::sT::bold).c_str());
		deinit();
		exit(0);
	}
	
	engineScanner = new Scanner(engine_moduleStartAddress, engine_moduleLength, mem);
	clientScanner = new Scanner(client_moduleStartAddress, client_moduleLength, mem);
	
	getEnginePointers();
	
	if (off->engine.m_dwCEngineClient != 0x0)
		printf("Engine Client\t\t\t= %s0x%llx%s\n", cT::getColor(cT::fG::green).c_str(), off->engine.m_dwCEngineClient, cT::getStyle(cT::sT::bold).c_str());
	
	getClientPointers();
	
	if (off->client.m_dwLocalPlayer != 0x0) {
		printf("Local Player Base\t\t= %s0x%llx%s\n", cT::getColor(cT::fG::green).c_str(), off->client.m_dwLocalPlayer, cT::getStyle(cT::sT::bold).c_str());
	}
	
	if (off->client.m_dwEntityList != 0x0) {
		printf("Entity List\t\t\t= %s0x%llx%s\n", cT::getColor(cT::fG::green).c_str(), off->client.m_dwEntityList, cT::getStyle(cT::sT::bold).c_str());
	}
	
	if (off->client.m_dwGlowManager != 0x0) {
		printf("Glow Manager\t\t\t= %s0x%llx%s\n", cT::getColor(cT::fG::green).c_str(), off->client.m_dwGlowManager, cT::getStyle(cT::sT::bold).c_str());
	}
}

Wall::~Wall() {
	deinit();
}

void Wall::deinit() {
	stop.store(true);
	if (g_cProc)
		delete g_cProc;
	if (mem)
		delete mem;
	if (off)
		delete off;
	if (engineScanner)
		delete engineScanner;
	if (clientScanner)
		delete clientScanner;
}

void Wall::run(bool getOff) {
	std::thread s_thread(&Wall::stopThread, this);
	s_thread.detach();
	
	off->engine.m_dwCEngineClientBase = mem->read<uint64_t>(off->engine.m_dwCEngineClient);
	
	int i_teamNum = 0;
	
	while (g_cProc->mainPid() != -1 && !stop.load()) {
		if (g_cProc->mainTask() != 0 && off->engine.m_dwCEngineClientBase != 0x0) {
			if (mem->read<int>(off->engine.m_dwCEngineClientBase + off->engine.m_dwIsInGame) == 6) {
				off->client.m_dwLocalPlayerBase = mem->read<uint64_t>(off->client.m_dwLocalPlayer);
				if (off->client.m_dwLocalPlayerBase != 0x0 && off->client.m_dwGlowObjectLoopStartBase != 0x0 ) {
					i_teamNum = mem->read<int>(off->client.m_dwLocalPlayerBase + off->client.m_iTeam);
					if (getOff) {
						getOffsets();
						stop.store(true);
					}
					applyEntityGlow(i_teamNum);
				} else {
					getClientPointers();
					off->client.m_dwGlowObjectLoopStartBase = mem->read<uint64_t>(off->client.m_dwGlowManager);
				}
			}
		} else {
			getEnginePointers();
		}
		g_cProc->mainPid() = g_cProc->get("csgo_osx64");
		//g_cProc->mainTask() = g_cProc->task(g_cProc->mainPid());
		usleep(refreshRate); // 800
	}
	
	stop.store(true);
}

void Wall::stopThread() {
	std::string str;
	while (!stop.load()) {
		std::cin >> str;
		if (str == "q" || str == "quit" || str == "stop" || str == "exit") {
			stop.store(true);
		}
		usleep(refreshRate);
	}
}

void Wall::applyEntityGlow(int iTeamNum) {
	sGlowEntity glow;
	uint64_t glowPointer;
	uint64_t entityPointer;
	int health;
	
	for (int i = 0; i < mem->read<int>(off->engine.m_dwCEngineClientBase + off->engine.m_iGetMaxClients); ++i){
		
		entityPointer = mem->read<uint64_t>(off->client.m_dwEntityList + (off->client.m_dwEntityStructSize * i));
		
		if (entityPointer <= 0x0)
			continue;
		
		// Anti flash
		if (off->client.m_dwLocalPlayerBase == entityPointer) {
			if (maxFlash != -1) {
				if (mem->read<double>(entityPointer + off->client.m_dFlashAlpha) > maxFlash) {
					mem->write<double>(entityPointer + off->client.m_dFlashAlpha, maxFlash);
				}
				continue;
			}
			continue;
		}
		
		if (!mem->read<bool>(entityPointer + off->client.m_bDormant) && !mem->read<bool>(entityPointer + off->client.m_bLifeState)) {
			
			int team = mem->read<int>(entityPointer + off->client.m_iTeam);
			
			if (noTeammates && entityPointer != off->client.m_dwLocalPlayerBase && team == iTeamNum)
				continue;
			
			health = mem->read<int>(entityPointer + off->client.m_iHealth);
			
			if (health == 0)
				health = 100;
			
			glowPointer = off->client.m_dwGlowObjectLoopStartBase + (off->client.m_dwGlowStructSize * mem->read<int>(entityPointer + off->client.m_iGlowIndex));
			
			if (glowPointer != 0x0) {
				glow = mem->read<sGlowEntity>(glowPointer);
				
				if (glow.isValidGlowEntity(entityPointer)) {
					
					if (team != iTeamNum) {
						// Enemy glow colors
						glow.r = float((100 - health)/100.0);
						glow.g = float((health)/100.0);
						glow.b = 0.0f;
						glow.a = 0.6f;
					} else {
						// Teammates glow colors
						glow.r = float((100 - health)/100.0);
						glow.g = 0.0f;
						glow.b = float((health)/100.0);
						glow.a = 0.6f;
					}
					
					// Enables Glow
					glow.RenderWhenOccluded = true;
					//glow.RenderWhenUnoccluded = false;
					mem->write<sGlowEntity>(glowPointer, glow);
				}
			}
		}
	}
}

void Wall::getOffsets() {
	//
	// ** SHOULD NOT BE RUN ON VAC SERVERS **
	//
	// attempts to get the new glow index offset.
	//
	// - launch csgo with flag: -insecure
	// - launch game with bots.
	// - in terminal:
	// - sudo -s
	// - ./Wall -o
	//
	
	uint64_t entityPointer;
	
	bool isValid = false;
	std::vector<int> glowIndexes;
	
	for (int i = 0; i < 64; ++i) {
		for (int j = 0; j < 64; ++j){
			entityPointer = mem->read<uint64_t>(off->client.m_dwEntityList + (off->client.m_dwEntityStructSize * j));
			
			if (entityPointer <= 0x0 || entityPointer == 0)
				continue;
			
			if (!mem->read<bool>(entityPointer + off->client.m_bDormant) && !mem->read<bool>(entityPointer + off->client.m_bLifeState))
				glowIndexes.emplace_back(mem->read<int>(entityPointer + off->client.m_iGlowIndex + i));
		}
		
		for (int j = 1; j < glowIndexes.size(); ++j) {
			
			if (glowIndexes[j] > 1 && glowIndexes[j] < 60)
				isValid = true;
			else
				isValid = false;
		}
		glowIndexes.clear();
		
		if (isValid) {
			printf("\nuint64_t %s\t\t= %s0x%llx%s\n", cT::print("m_iGlowIndex", cT::fG::yellow).c_str(), cT::getColor(cT::fG::green).c_str(), off->client.m_iGlowIndex + i, cT::getStyle(cT::sT::bold).c_str());
			printf("uint64_t %s\t\t= %s0x%llx%s\n", cT::print("m_dFlashAlpha", cT::fG::yellow).c_str(), cT::getColor(cT::fG::green).c_str(), off->client.m_dFlashAlpha + i, cT::getStyle(cT::sT::bold).c_str());
			printf("uint64_t %s\t= %s0x%llx%s\n\n", cT::print("m_fFlashDuration", cT::fG::yellow).c_str(), cT::getColor(cT::fG::green).c_str(), off->client.m_fFlashDuration + i, cT::getStyle(cT::sT::bold).c_str());
			printf("uint64_t %s\t= %s0x%llx%s\n\n", cT::print("m_iShotFired", cT::fG::yellow).c_str(), cT::getColor(cT::fG::green).c_str(), off->client.m_iShotFired + i, cT::getStyle(cT::sT::bold).c_str());
			return;
		}
		usleep(1000);
	}
	
	if (!isValid) {
		printf("%s\n", cT::print("Error: Could not find new Offsets\nRetry using the last known Offsets", cT::fG::red, cT::sT::bold).c_str());
		return;
	}
}

void Wall::getEnginePointers() {
	off->engine.m_dwCEngineClient = engine_moduleStartAddress + engineScanner->getPointer(
																						  (Byte*)"\x55\x48\x89\xE5\x48\x8B\x00\x00\x00\x00\x00\x48\x83\x00\x00\x5D\xC3\x66\x66\x66\x66\x66\x66\x2E\x0F\x1F\x84\x00\x00\x00\x00\x00",
																						  "xxxxxx?????xx??xxxxxxxxxxxxxxxxx",
																						  0x7
																						  ) + 0x4;
}

void Wall::getClientPointers() {
	off->client.m_dwLocalPlayer = client_moduleStartAddress + clientScanner->getPointer(
																						(Byte*)"\x89\xD6\x41\x89\x00\x49\x89\x00\x48\x8B\x1D\x00\x00\x00\x00\x48\x85\xDB\x74\x00",
																						"xxxx?xx?xxx????xxxx?",
																						0xB
																						) + 0x4;
	
	off->client.m_dwEntityList = client_moduleStartAddress + clientScanner->getPointer(
																					   (Byte*)"\x48\x8D\x1D\x00\x00\x00\x00\x48\x89\xDF\xE8\x00\x00\x00\x00\x48\x8D\x3D\x00\x00\x00\x00\x48\x8D\x15\x00\x00\x00\x00\x48\x89\xDE",
																					   "xxx????xxxx????xxx????xxx????xxx",
																					   0x3
																					   ) + 0x2C;
	
	off->client.m_dwGlowManager = client_moduleStartAddress + clientScanner->getPointer(
																						(Byte*)"\x48\x8D\x3D\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x85\xC0\x0F\x84\x00\x00\x00\x00\x48\xC7\x05\x00\x00\x00\x00\x00\x00\x00\x00\x48\x8D\x35\x00\x00\x00\x00",
																						"xxx????x????xxxx????xxx????xxxxxxx????",
																						0x22
																						) + 0x4;
	/*
	 off->client. m_dwRadarBase = client_moduleStartAddress + clientScanner->getPointer(
	 (Byte*)"\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x50\x49\x89\xFD\x49\x8B\x7D\x00\x48\x85\xFF\x74\x56\x41\x8B\x75\x00\xE8\x6D\x00\x00\x00\x41\x88\xC6\x45\x84\xF6\x75\x45\x41\x80\xF6\x00\x45\x8B\x65\x00\x45\x85\xE4\x7E\x3B\x31\xDB\x4C\x8D\x3D\x00\x00\x00\x00\x66\x66\x66\x66\x66\x66\x2E\x0F\x1F\x84\x00\x00\x00\x00\x00",
	 "xxxxxxxxxxxxxxxxxxxx?xxxxxxxx?xxxxxxxxxxxxxxxx?xxx?xxxxxxxxxx????xxxxxxxxxxxxxxx",
	 0x3D
	 ) + 0x3C;
	 
	 printf("Radar Base: 0x%llx\n", off->client.m_dwRadarBase);
	 */
	/*
	 off->client. m_dwPlayerResource = client_moduleStartAddress + clientScanner->getPointer(
	 (Byte*)"\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x83\xEC\x00\x48\x89\xF3\x48\x8D\x7D\xB0\xBE\x00\x00\x00\x00\xE8\xAE\x2F\xDC\xFF\x4C\x8B\x75\xD0\x48\x8B\x03\x48\x8B\x80\x00\x00\x00\x00\x48\x89\xDF\xFF\xD0\x41\x80\x4E\x00\x00\x49\x89\x46\x00\x48\x8D\x1D\x00\x00\x00\x00\x48\x8B\x3B\x48\x8B\x07\x48\x8B\x80\x00\x00\x00\x00\xFF\xD0\x4C\x8D\x25\x00\x00\x00\x00",
	 "xxxxxxxxxxxxxxxx?xxxxxxxx????xxxxxxxxxxxxxxx????xxxxxxxx??xxx?xxx????xxxxxxxxx????xxxxx????",
	 0x57
	 ) + 0x4;
	 
	 printf("Player Resource: 0x%llx\n", off->client.m_dwPlayerResource);
	 */
}

std::atomic<bool> Wall::stop{false};