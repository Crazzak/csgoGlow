//
//  Wall.cpp
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

Wall::Wall(double refreshRate, double maxFlash, double glowAlpha, bool noTeammates, bool noUtils, bool spotted)
{
	this->refreshRate 	= refreshRate;
	this->maxFlash 		= maxFlash;
	this->glowAlpha 	= glowAlpha;
	this->noTeammates 	= noTeammates;
	this->noUtils		= noUtils;
	this->spotted 		= spotted;
	
	stop.store(false);
	
	g_cProc->get("csgo_osx64");
	
	if (g_cProc->mainPid() == -1) {
		printf("%s\n", cT::print("Error: Can't find CSGO's PID", cT::fG::red, cT::sT::bold).c_str());
		Deinit();
		exit(0);
	}
    
    printf("Found CSGO's PID\t\t= %s\n", cT::print(std::to_string(g_cProc->mainPid()).c_str(), cT::fG::green, cT::sT::bold).c_str());
	
	g_cProc->task(g_cProc->mainPid());
	
	if (!g_cProc->mainTask()) {
		printf("%s\n", cT::print("Error: Failed to find main task", cT::fG::red, cT::sT::bold).c_str());
		Deinit();
		exit(0);
	}
    
    g_cProc->getModule(g_cProc->mainTask(), &engine_moduleStartAddress, &engine_moduleLength, "/engine.dylib");
    
    if (engine_moduleStartAddress == 0x0) {
        printf("%s\n", cT::print("Error: Failed to get the engine.dylib address", cT::fG::red, cT::sT::bold).c_str());
        Deinit();
        exit(0);
    }
    
    printf("Found the %s address\t= %s0x%llx%s ... %s0x%llx%s\n", cT::print("engine.dylib", cT::fG::yellow).c_str(),  cT::getColor(cT::fG::green).c_str(),
           engine_moduleStartAddress,
           cT::getStyle(cT::sT::bold).c_str(),
           cT::getColor(cT::fG::green).c_str(),
           engine_moduleStartAddress +
           engine_moduleLength,
           cT::getStyle(cT::sT::bold).c_str());
    
    g_cProc->getModule(g_cProc->mainTask(), &client_moduleStartAddress, &client_moduleLength, "/client.dylib");
    
    if (client_moduleStartAddress == 0x0) {
       printf("%s\n", cT::print("Error: Failed to get the client.dylib address", cT::fG::red, cT::sT::bold).c_str());
        Deinit();
        exit(0);
    }
    
    printf("Found the %s address\t= %s0x%llx%s ... %s0x%llx%s\n", cT::print("client.dylib", cT::fG::yellow).c_str(),
           cT::getColor(cT::fG::green).c_str(),
           client_moduleStartAddress,
           cT::getStyle(cT::sT::bold).c_str(),
           cT::getColor(cT::fG::green).c_str(),
           client_moduleStartAddress +
           client_moduleLength,
           cT::getStyle(cT::sT::bold).c_str());
	
	engineScanner		= new Scanner(engine_moduleStartAddress, engine_moduleLength, mem);
	clientScanner 		= new Scanner(client_moduleStartAddress, client_moduleLength, mem);
	
	localPlayer 		= new sBasePlayer_t;
	entityList 			= new sEntityList_t;
	glowManager 		= new sGlowManager_t;
	radarManager 		= new C_RadarManager;
	playerResource 		= new sPlayerResource_t;
	
	glow 				= new sGlowDefinitionObject_t;
}

Wall::~Wall()
{
	Deinit();
}

void Wall::Deinit()
{
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
	if (localPlayer)
		delete localPlayer;
	if (entityList)
		delete entityList;
	if (glowManager)
		delete glowManager;
	if (radarManager)
		delete radarManager;
	if (playerResource)
		delete playerResource;
	if (glow)
		delete glow;
}

void Wall::Run()
{
	std::thread s_thread(&Wall::StopThread, this);
	s_thread.detach();
	
	while (g_cProc->get("csgo_osx64") != -1 && g_cProc->task(g_cProc->mainPid()) != -1 && !stop.load()) {
        if (!EngineCheck()) { printf("EngineCheck Failed\n"); goto procCheck; }
		if (mem->read<int>(off->engine.m_dwCEngineClientBase + off->engine.m_dwIsInGame) != 6) { goto procCheck; }
		if (!ClientCheck()) { printf("ClientCheck Failed\n"); goto procCheck; }
        ApplyGlow();
        procCheck:
            usleep(refreshRate); // 800
	}
	stop.store(true);
	std::system("clear");
}

void Wall::ParseEntityList()
{
	entities.clear();
	sEntityList_t* tmp = &entities.emplace_back(*entityList);
	uint64_t next_ptr = tmp->m_pNext;
	while (next_ptr) {
		tmp = &entities.emplace_back(mem->read<sEntityList_t>(next_ptr));
		next_ptr = tmp->m_pNext;
	}
}

void Wall::ApplyGlow()
{
	int health;
	int team;
	int i_teamNum = localPlayer->Team();
	
	bool cmp = false;

	for (int i = 0; i < glowManager->Capacity(); ++i) {
		
		*glow = glowManager->Get(i);
		
		if (!glow->IsValid()) { continue; }
		if (glow->IsDormant()) { continue; }
			
        switch (glow->Type()) {
            case sOffsets::player:
                
                player = reinterpret_cast<sBasePlayer_t*>(glow);
                
                if (player->LifeState()) {
                    break;
                }
                
                if (*player == *localPlayer) {
                    if (maxFlash != -1 && player->FlashMaxAlpha() > maxFlash) {
                        player->SetFlashMaxAlpha(maxFlash);
                    }
                    break;
                }
                
                if (spotted) {
                    player->SetSpotted(spotted);
                }
                
                team = glow->Team();
				
				cmp = team != i_teamNum;

				if (noTeammates && !cmp) {
                    break;
                }

                health = player->Health();
                health += (health == 0 ? 100 : health);
                
                // Glow Colors
                glow->m_vGlowColor = {
                    float((100 - health)/100.0),
                    cmp ? float((health)/100.0) : 0.0f,
                    cmp ? 0.0f : float((health)/100.0)
                };
                glow->m_flGlowAlpha = glowAlpha;

                // Enables Glow
                glow->m_bRenderWhenOccluded = true;
                glow->m_bRenderWhenUnoccluded = false;

                // Write to Memory
                glowManager->Write(glow, i);
                break;
            case sOffsets::hostage:
                break;
            case sOffsets::chicken:
                
                if (noUtils || glow->LifeState()) {
                    break;
                }

                // Glow Colors
                glow->m_vGlowColor = {0.0f, 1.0f, 1.0f};
                glow->m_flGlowAlpha = 1.0f;

                // Enables Glow
                glow->m_bRenderWhenOccluded = !noUtils;
                glow->m_bRenderWhenUnoccluded = false;

                // Write to Memory
                glowManager->Write(glow, i);
                break;
            case sOffsets::C4:
                
                 weapon = reinterpret_cast<sBaseCombatWeapon_t*>(glow);
                
                if (noUtils || weapon->State()) {
                    break;
                }
                
                // Glow Colors
                glow->m_vGlowColor = {0.0f, 1.0f, 0.0f};
                glow->m_flGlowAlpha = 1.0f;

                // Enables Glow
                glow->m_bRenderWhenOccluded = !noUtils;
                glow->m_bRenderWhenUnoccluded = false;

                // Write to Memory
                glowManager->Write(glow, i);
                break;
            case sOffsets::plantedC4:
                
                if (noUtils) {
                    break;
                }
                
                // Glow Colors
                glow->m_vGlowColor = {1.0f, 0.0f, 0.0f};
                glow->m_flGlowAlpha = 1.0f;

                // Enables Glow
                glow->m_bRenderWhenOccluded = (!noUtils | (reinterpret_cast<sBasePlantedC4_t*>(glow)->State() == 1));
                glow->m_bRenderWhenUnoccluded = false;

                // Write to Memory
                glowManager->Write(glow, i);
                break;
            case sOffsets::weapon:
                
                weapon = reinterpret_cast<sBaseCombatWeapon_t*>(glow);
                
                if (noUtils || weapon->State()) {
                    break;
                }
                
                // Glow Colors
                glow->m_vGlowColor = {1.0f, 1.0f, 1.0f};
                glow->m_flGlowAlpha = 1.0f;

                // Enables Glow
                glow->m_bRenderWhenOccluded = !noUtils;
                glow->m_bRenderWhenUnoccluded = false;

                // Write to Memory
                glowManager->Write(glow, i);
                break;
            case sOffsets::kit:
                
				if (noUtils) {
                    break;
                }
                
				// Glow Colors
                glow->m_vGlowColor = {1.0f, 0.0f, 1.0f};
                glow->m_flGlowAlpha = 0.5f;
                
                // Enables Glow
                glow->m_bRenderWhenOccluded = !noUtils;
                glow->m_bRenderWhenUnoccluded = false;
                
                // Write to Memory
                glowManager->Write(glow, i);
                break;
            case sOffsets::grenade:
                
                weapon = reinterpret_cast<sBaseCombatWeapon_t*>(glow);

                if (noUtils || weapon->State()) {
                    break;
                }

                // Glow Colors
                glow->m_vGlowColor = {1.0f, 1.0f, 1.0f};
                glow->m_flGlowAlpha = 1.0f;

                // Enables Glow
                glow->m_bRenderWhenOccluded = !noUtils;
                glow->m_bRenderWhenUnoccluded = false;

                // Write to Memory
                glowManager->Write(glow, i);
                break;
            case sOffsets::projectile:
                
                proj = reinterpret_cast<sBaseCSGrenadeProjectile_t*>(glow);

                if (noUtils || proj->State()) {
                    break;
                }

                // Glow Colors
                glow->m_vGlowColor = {1.0f, 1.0f, 1.0f};
                glow->m_flGlowAlpha = 1.0f;

                // Enables Glow
                glow->m_bRenderWhenOccluded = !noUtils;
                glow->m_bRenderWhenUnoccluded = false;

                // Write to Memory
                glowManager->Write(glow, i);
                
                break;
            case sOffsets::props:
                break;
            case sOffsets::resource:
                break;
            case sOffsets::team:
                break;
            case sOffsets::other:
                break;
        }
    }
	
	if (revealRank.load()) {
		C_RadarBase radarBase = radarManager->GetRadarStruct().GetRadarBase();
		
		ParseEntityList();
		
		int index = 1;
		printf("######### RANKS #########\n");
		for (int i = 0; i < entities.size(); ++i) {
			if (!entities[i].IsPlayer()) {
				continue;
			}
			
			index = entities[i].ID();
			printf("name: %s\n", radarBase.GetRadarObject(index).GetRadarName().c_str());
			printf("team: %s\n", off->TeamNames[playerResource->Team2(index)]);
			
			printf("rank: %s\n", off->ranks[playerResource->CompetitiveRanking(index)]);
			printf("wins: %i\n", playerResource->CompetitiveWins(index));
			printf("\n");
		}
		
		entities.clear();
		
		printf("######### DONE #########\n");
		
		revealRank.store(false);
	}
	
//	ParseEntityList();
//	for (auto& entity: entities) {
//		printf("%s -> 0x%llx\n", entity.EntityClass().c_str(), entity.m_hBase);
////		entity.Print();
//	}
//
//	stop.store(true);

//	entities.clear();
}

bool Wall::EngineCheck()
{
	if (off->engine.m_dwCEngineClient == 0x0) {
		GetEnginePointers();
		printf("Engine Client\t\t\t= %s0x%llx%s\n", cT::getColor(cT::fG::green).c_str(), off->engine.m_dwCEngineClient, cT::getStyle(cT::sT::bold).c_str());
	}
	
	if (off->engine.m_dwCEngineClient != 0x0 && off->engine.m_dwCEngineClientBase == 0x0) {
		off->engine.m_dwCEngineClientBase = mem->read<uint64_t>(off->engine.m_dwCEngineClient);
	}
	
	return off->engine.m_dwCEngineClientBase != 0x0;
}

bool Wall::ClientCheck()
{
	bool cmp = off->client.m_dwLocalPlayer == 0x0 || off->client.m_dwEntityList == 0x0 || off->client.m_dwGlowManager == 0x0 || off->client.m_dwRadarBase == 0x0; // || off->client.m_dwPlayerResource == 0x0;
		
	GetClientPointers();

	if (cmp) {
		printf("Local Player\t\t\t= %s0x%llx%s\n", cT::getColor(cT::fG::green).c_str(), off->client.m_dwLocalPlayer, cT::getStyle(cT::sT::bold).c_str());
		printf("Entity List\t\t\t= %s0x%llx%s\n", cT::getColor(cT::fG::green).c_str(), off->client.m_dwEntityList, cT::getStyle(cT::sT::bold).c_str());
		printf("Glow Manager\t\t\t= %s0x%llx%s\n", cT::getColor(cT::fG::green).c_str(), off->client.m_dwGlowManager, cT::getStyle(cT::sT::bold).c_str());
		printf("Radar Base\t\t\t= %s0x%llx%s\n", cT::getColor(cT::fG::green).c_str(), off->client.m_dwRadarBase, cT::getStyle(cT::sT::bold).c_str());
		// printf("PlayerResource\t\t\t= %s0x%llx%s\n", cT::getColor(cT::fG::green).c_str(), off->client.m_dwPlayerResource, cT::getStyle(cT::sT::bold).c_str());
	}
	
	*localPlayer = mem->read<sBasePlayer_t>(off->client.m_dwLocalPlayer);
	*entityList = mem->read<sEntityList_t>(off->client.m_dwEntityList);
	*glowManager = mem->read<sGlowManager_t>(off->client.m_dwGlowManager);
	*radarManager = mem->read<C_RadarManager>(off->client.m_dwRadarBase);
	// *playerResource = mem->read<sPlayerResource_t>(off->client.m_dwPlayerResource);
	
	if (!localPlayer->IsValid()) {
		printf("localPlayer: 0x%llx failed\n", localPlayer->m_hBase);
	}
	if (!entityList->IsValid()) {
		printf("entityList: 0x%llx failed\n", entityList->m_hBase);
	}
	if (!glowManager->IsValid()) {
		printf("glowManager: 0x%llx failed\n", glowManager->m_hBase);
	}
	if (!radarManager->IsValid()) {
		printf("radarManager: 0x%llx failed\n", radarManager->m_hBase);
	}
	// if (!playerResource->IsValid()) {
	// 	printf("playerResource: 0x%llx failed\n", playerResource->m_hBase);
	// }
	
	return localPlayer->IsValid() && entityList->IsValid() && glowManager->IsValid() && radarManager->IsValid(); // && playerResource->IsValid();
}
void Wall::GetEnginePointers()
{
	off->engine.m_dwCEngineClient = engine_moduleStartAddress + engineScanner->getPointer(
																						  (Byte*)"\x55\x48\x89\xE5\x48\x8B\x00\x00\x00\x00\x00\x48\x83\x00\x00\x5D\xC3\x66\x66\x66\x66\x66\x66\x2E\x0F\x1F\x84\x00\x00\x00\x00\x00",
																						  "xxxxxx?????xx??xxxxxxxxxxxxxxxxx",
																						  0x7
																						  ) + 0x4;
}

void Wall::GetClientPointers()
{
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
	
	 off->client.m_dwRadarBase = client_moduleStartAddress + clientScanner->getPointer(
	 (Byte*)"\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x50\x49\x89\xFD\x49\x8B\x7D\x00\x48\x85\xFF\x74\x56\x41\x8B\x75\x00\xE8\x6D\x00\x00\x00\x41\x88\xC6\x45\x84\xF6\x75\x45\x41\x80\xF6\x00\x45\x8B\x65\x00\x45\x85\xE4\x7E\x3B\x31\xDB\x4C\x8D\x3D\x00\x00\x00\x00\x66\x66\x66\x66\x66\x66\x2E\x0F\x1F\x84\x00\x00\x00\x00\x00",
	 "xxxxxxxxxxxxxxxxxxxx?xxxxxxxx?xxxxxxxxxxxxxxxx?xxx?xxxxxxxxxx????xxxxxxxxxxxxxxx",
	 0x3D
	 ) + 0x3C;

//	 printf("Radar Base: 0x%llx\n", off->client.m_dwRadarBase);
	
	off->client.m_dwPlayerResource = client_moduleStartAddress + 0x1FD1018; //0x1FD6768; // 0x1FD5728; //0x1FD24B8;
//	printf("Player Resource: 0x%llx\n", off->client.m_dwPlayerResource);
	
	
	/*
	 off->client. m_dwPlayerResource = client_moduleStartAddress + clientScanner->getPointer(
	 (Byte*)"\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x83\xEC\x00\x48\x89\xF3\x48\x8D\x7D\xB0\xBE\x00\x00\x00\x00\xE8\xAE\x2F\xDC\xFF\x4C\x8B\x75\xD0\x48\x8B\x03\x48\x8B\x80\x00\x00\x00\x00\x48\x89\xDF\xFF\xD0\x41\x80\x4E\x00\x00\x49\x89\x46\x00\x48\x8D\x1D\x00\x00\x00\x00\x48\x8B\x3B\x48\x8B\x07\x48\x8B\x80\x00\x00\x00\x00\xFF\xD0\x4C\x8D\x25\x00\x00\x00\x00",
	 "xxxxxxxxxxxxxxxx?xxxxxxxx????xxxxxxxxxxxxxxx????xxxxxxxx??xxx?xxx????xxxxxxxxx????xxxxx????",
	 0x57
	 ) + 0x4;
	 
	 printf("Player Resource: 0x%llx\n", off->client.m_dwPlayerResource);
	 */
}

void Wall::StopThread()
{
	std::string str;
	while (!stop.load()) {
		std::cin >> str;
		if (str == "q" || str == "quit" || str == "stop" || str == "exit") {
			stop.store(true);
		}
		if (str == "ranks") {
			revealRank.store(true);
		}
		usleep(100000);
	}
}

std::atomic<bool> Wall::stop{false};

std::atomic<bool> Wall::revealRank{false};

Process* Wall::g_cProc = new Process;

MemMngr* Wall::mem = new MemMngr(g_cProc);

sOffsets* Wall::off = new sOffsets;


std::string Wall::sBaseEntity_t::EntityClass()
{
	uint64_t vtable = mem->read<uint64_t>(m_hBase + 0x8);
	uint64_t fn = mem->read<uint64_t>(vtable - 0x8);
	uint64_t cls = mem->read<uint64_t>(fn + 0x8);
	std::string clsName = mem->readString(cls);
	auto delim = clsName.find_first_of("C");
	if (delim != std::string::npos) {
		return std::string(clsName.c_str(), delim, clsName.length() - delim);
	}
	return clsName;
}

sOffsets::EntityType Wall::sBaseEntity_t::Type()
{
	return off->getEntityType(EntityClass());
}

Byte Wall::sBaseEntity_t::EFlags()
{
	return mem->read<Byte>(m_hBase + off->client.m_iEFlags);
}

int Wall::sBaseEntity_t::ID()
{
	return mem->read<int>(m_hBase + off->client.m_iID);
}

int Wall::sBaseEntity_t::Team()
{
	return mem->read<int>(m_hBase + off->client.m_iTeam);
}

int Wall::sBaseEntity_t::SpottedBy()
{
	return mem->read<int>(m_hBase + off->client.m_bSpottedBy);
}

bool Wall::sBaseEntity_t::Spotted()
{
	return mem->read<bool>(m_hBase + off->client.m_bSpotted);
}

bool Wall::sBaseEntity_t::IsDormant()
{
	return mem->read<bool>(m_hBase + off->client.m_bDormant);
}

bool Wall::sBaseEntity_t::LifeState()
{
	return mem->read<bool>(m_hBase + off->client.m_bLifeState);
}

bool Wall::sBaseEntity_t::IsWeapon()
{
	return Type() == sOffsets::weapon;
}

bool Wall::sBaseEntity_t::IsBomb()
{
	return Type() == sOffsets::C4 || Type() == sOffsets::plantedC4;
}

bool Wall::sBaseEntity_t::IsChicken()
{
	return Type() == sOffsets::chicken;
}

bool Wall::sBaseEntity_t::IsPlayer()
{
	return Type() == sOffsets::player;
}

bool Wall::sBaseEntity_t::IsValid()
{
	return m_hBase > 0x0;
}

bool Wall::sBaseEntity_t::IsValid(uint64_t ptr)
{
	return m_hBase > 0x0 && m_hBase == ptr;
}

bool Wall::sBaseEntity_t::operator == (const sBaseEntity_t& rhs)
{
	return m_hBase == rhs.m_hBase;
}

void Wall::sBaseEntity_t::SetSpotted(bool x)
{
	mem->write<bool>(m_hBase + off->client.m_bSpotted, x);
}

void Wall::sBaseEntity_t::Print()
{
	printf("m_hBase = 0x%llx\n", m_hBase);
	printf("%s\n", EntityClass().c_str());
	printf("m_bSpotted = %i\n", Spotted());
	printf("m_bSpottedBy = %i\n", SpottedBy());
	printf("Entity Type = %i\n", Type());
	printf("m_iTeam = %i\n", Team());
	printf("m_bDormant = %i\n", IsDormant());
	printf("m_bLifeState = %i\n", LifeState());
	printf("Is Weapon = %i\n", IsWeapon());
	printf("Is Bomb = %i\n", IsBomb());
	printf("Is Chicken = %i\n", IsChicken());
	printf("Is Player = %i\n", IsPlayer());
	printf("\n");
}



double Wall::sBasePlayer_t::FlashMaxAlpha()
{
	return mem->read<double>(m_hBase + off->client.m_flFlashMaxAlpha);
}

float Wall::sBasePlayer_t::FlashDuration()
{
	return mem->read<float>(m_hBase + off->client.m_flFlashDuration);
}

int Wall::sBasePlayer_t::Health()
{
	return mem->read<int>(m_hBase + off->client.m_iHealth);
}

int Wall::sBasePlayer_t::GlowIndex()
{
	return mem->read<int>(m_hBase + off->client.m_iGlowIndex);
}

int Wall::sBasePlayer_t::ShotsFired()
{
	return mem->read<int>(m_hBase + off->client.m_iShotFired);
}

bool Wall::sBasePlayer_t::HasMovedSinceSpawn()
{
	return mem->read<bool>(m_hBase + off->client.m_bHasMovedSinceSpawn);
}

bool Wall::sBasePlayer_t::IsJumping()
{
	return !(EFlags() & FL_ONGROUND);
}

bool Wall::sBasePlayer_t::IsCrouching()
{
	return EFlags() & FL_DUCKING;
}

void Wall::sBasePlayer_t::GetAllWeapons(uint64_t* weaponArray)
{
	for (int i = 0; i < 16; ++i) {
		uint64_t weaponHandle = mem->read<uint32_t>(m_hBase + off->client.m_hMyWeapons + (sizeof(uint32_t) * i)) & 0xFFF;
		if((int)weaponHandle != -1) {
			weaponArray[i] = mem->read<uint64_t>(
												 off->client.m_dwEntityList +
												 ((weaponHandle - 1) * sizeof(sEntityList_t))
												 );
		} else {
			weaponArray[i] = 0x0;
		}
	}
}

uint64_t Wall::sBasePlayer_t::GetActiveWeapon()
{
	uint64_t dwBaseCombatWeaponIndex =  mem->read<uint32_t>(m_hBase + off->client.m_hActiveWeapon) & 0xFFF;
	
	return mem->read<uint64_t>(
										 off->client.m_dwEntityList +
										 ((dwBaseCombatWeaponIndex - 1) * sizeof(sEntityList_t))
										 );
}

void Wall::sBasePlayer_t::SetFlashMaxAlpha(double x)
{
	if (this->IsValid()) {
		mem->write<double>(m_hBase + off->client.m_flFlashMaxAlpha, x);
	}
}

void Wall::sBasePlayer_t::Print()
{
	reinterpret_cast<sBaseEntity_t*>(this)->Print();
	printf("m_flFlashMaxAlpha = %f\n", FlashMaxAlpha());
	printf("m_flFlashDuration = %f\n", FlashDuration());
	printf("m_iHealth = %i\n", Health());
	printf("m_iGlowIndex = %i\n", GlowIndex());
	printf("m_iShotFired = %i\n", ShotsFired());
	printf("m_bHasMovedSinceSpawn = %i\n", HasMovedSinceSpawn());
	printf("Is Jumping = %i\n", IsJumping());
	printf("Is Crouching = %i\n", IsCrouching());
	printf("\n\n");
}

int Wall::sBaseCombatWeapon_t::State()
{
	return mem->read<int>(m_hBase + 0x3ad8);
}

int Wall::sBaseCSGrenadeProjectile_t::ExplodeEffectIndex()
{
	return mem->read<int>(m_hBase + 0x3078);
}

int Wall::sBaseCSGrenadeProjectile_t::ExplodeEffectTickBegin()
{
	return mem->read<int>(m_hBase + 0x307c);
}

void Wall::sBaseCombatWeapon_t::Print()
{
	reinterpret_cast<sBaseEntity_t*>(this)->Print();
	printf("m_iState = %i\n", State());
	printf("\n\n");
}

int Wall::sBasePlantedC4_t::State()
{
	return mem->read<int>(m_hBase + off->client.m_iC4State);
}


void Wall::sEntityList_t::Print()
{
	reinterpret_cast<sBaseEntity_t*>(this)->Print();
	printf("m_SerialNum = %s\n", m_SerialNum);
	printf("m_pPrevious = 0x%llx\n", m_pPrevious);
	printf("m_pNext = 0x%llx\n", m_pNext);
	printf("\n\n");
}




void Wall::sGlowDefinitionObject_t::Print()
{
	reinterpret_cast<sBaseEntity_t*>(this)->Print();
	printf("red = %f\n", m_vGlowColor.r);
	printf("green = %f\n", m_vGlowColor.g);
	printf("blue = %f\n", m_vGlowColor.b);
	printf("alpha = %f\n", m_flGlowAlpha);
	printf("unk1 = %s\n", unk1);
	printf("m_bRenderWhenOccluded = %i\n", m_bRenderWhenOccluded);
	printf("m_bRenderWhenUnoccluded = %i\n", m_bRenderWhenUnoccluded);
	printf("FullBloom = %i\n", FullBloom);
	printf("unk2 = %s\n", unk2);
	printf("\n\n");
}



Wall::sGlowDefinitionObject_t Wall::sGlowManager_t::Get(int index)
{
	return mem->read<sGlowDefinitionObject_t>(m_hBase + 0x8 + (sizeof(sGlowDefinitionObject_t) * index));
}

int Wall::sGlowManager_t::Capacity()
{
	return m_iGlowListMaxSize;
}

int Wall::sGlowManager_t::Size()
{
	return m_iGlowListSize;
}

void Wall::sGlowManager_t::Write(Wall::sGlowDefinitionObject_t* glowObject, int index)
{
	if (!this->IsValid() || !glowObject->IsValid()) {
		printf("GlowManager 0x%llx Failed To Write 0x%llx At Index %i\n", this->m_hBase, glowObject->m_hBase, index);
		return;
	}
	
	mem->write<sGlowDefinitionObject_t>(m_hBase + 0x8 + (sizeof(sGlowDefinitionObject_t) * index), *glowObject);
}

void Wall::sGlowManager_t::Print()
{
	reinterpret_cast<sBaseEntity_t*>(this)->Print();
	printf("m_iGlowListMaxSize = %i\n", m_iGlowListMaxSize);
	printf("unk1 = %s\n", unk1);
	printf("m_iGlowListSize = %i\n", m_iGlowListSize);
	printf("unk2 = %s\n", unk2);
	printf("\n\n");
}



std::string Wall::sPlayerResource_t::Clan(int index)
{
	return mem->readString(m_hBase + off->client.m_szClan + (index * 0xF));
}

int Wall::sPlayerResource_t::Kills(int index)
{
	return mem->read<int>(m_hBase + off->client.m_iKills + (index * sizeof(int)));
}

int Wall::sPlayerResource_t::Assists(int index)
{
	return mem->read<int>(m_hBase + off->client.m_iAssists + (index * sizeof(int)));
}

int Wall::sPlayerResource_t::Deaths(int index)
{
	return mem->read<int>(m_hBase + off->client.m_iDeaths + (index * sizeof(int)));
}

int Wall::sPlayerResource_t::Team2(int index)
{
	return mem->read<int>(m_hBase + off->client.m_iTeam2 + (index * sizeof(int)));
}

int Wall::sPlayerResource_t::Health2(int index)
{
	return mem->read<int>(m_hBase + off->client.m_iHealth2 + (index * sizeof(int)));
}

int Wall::sPlayerResource_t::MVPs(int index)
{
	return mem->read<int>(m_hBase + off->client.m_iMVPs + (index * sizeof(int)));
}

int Wall::sPlayerResource_t::Armor(int index)
{
	return mem->read<int>(m_hBase + off->client.m_iArmor + (index * sizeof(int)));
}

int Wall::sPlayerResource_t::Score(int index)
{
	return mem->read<int>(m_hBase + off->client.m_iScore + (index * sizeof(int)));
}

int Wall::sPlayerResource_t::CompetitiveRanking(int index)
{
	return mem->read<int>(m_hBase + off->client.m_iCompetitiveRanking + (index * sizeof(int)));
}

int Wall::sPlayerResource_t::CompetitiveWins(int index)
{
	return mem->read<int>(m_hBase + off->client.m_iCompetitiveWins + (index * sizeof(int)));
}

int Wall::sPlayerResource_t::TotalCashSpent(int index)
{
	return mem->read<int>(m_hBase + off->client.m_iTotalCashSpent + (index * sizeof(int)));
}

int Wall::sPlayerResource_t::CashSpentThisRound(int index)
{
	return mem->read<int>(m_hBase + off->client.m_iCashSpentThisRound + (index * sizeof(int)));
}

bool Wall::sPlayerResource_t::Connected(int index)
{
	return mem->read<bool>(m_hBase + off->client.m_bConnected + (index * sizeof(bool)));
}

bool Wall::sPlayerResource_t::HasDefuser(int index)
{
	return mem->read<bool>(m_hBase + off->client.m_bHasDefuser + (index * sizeof(bool)));
}

bool Wall::sPlayerResource_t::HasHelmet(int index)
{
	return mem->read<bool>(m_hBase + off->client.m_bHasHelmet + (index * sizeof(bool)));
}

void Wall::sPlayerResource_t::Print()
{
	int num_alive = mem->read<int>(off->client.m_dwEntityList - off->client.m_iNumPlayersAlive);
	for (int i = 0; i < num_alive; ++i) {
		printf("index = %i\n", i);
		printf("m_hBase = 0x%llx\n", m_hBase);
		printf("m_iKills = %i\n", Kills(i));
		printf("m_iAssists = %i\n", Assists(i));
		printf("m_iDeaths = %i\n", Deaths(i));
		printf("m_iTeam2 = %i\n", Team2(i));
		printf("m_bConnected = %i\n", Connected(i));
		printf("m_iHealth2 = %i\n", Health2(i));
		printf("m_iMVPs = %i\n", MVPs(i));
		printf("m_bHasDefuser = %i\n", HasDefuser(i));
		printf("m_bHasHelmet = %i\n", HasHelmet(i));
		printf("m_iArmor = %i\n", Armor(i));
		printf("m_iScore = %i\n", Score(i));
		printf("m_iCompetitiveRanking = %i\n", CompetitiveRanking(i));
		printf("m_iCompetitiveWins = %i\n", CompetitiveWins(i));
		printf("\n");
	}
}

Wall::C_RadarBase::C_RadarBase() {}

Wall::C_RadarObject Wall::C_RadarBase::GetRadarObject(int index)
{
	return m_dwRadarArray[index-1];
}

Wall::C_RadarManager::C_RadarManager() {}

Wall::C_RadarStruct Wall::C_RadarManager::GetRadarStruct()
{
	return mem->read<C_RadarStruct>(m_hBase);
}

Wall::C_RadarObject::C_RadarObject()
{
	printf("unk: %s\n", unk);
	printf("health: %i\n", m_iRadarHealth);
	printf("name: %s\n", m_szRadarName);
}

int Wall::C_RadarObject::GetRadarHealth()
{
	return m_iRadarHealth;
}

std::string Wall::C_RadarObject::GetRadarName()
{
	return m_szRadarName;
}

Wall::C_RadarStruct::C_RadarStruct() {}

Wall::C_RadarBase Wall::C_RadarStruct::GetRadarBase()
{
	return mem->read<C_RadarBase>(m_dwRadarBase);
}


//void Wall::GetOffsets()
//{
//	//
//	// ** SHOULD NOT BE RUN ON VAC SERVERS **
//	//
//	// attempts to get the new glow index offset.
//	//
//	// - launch csgo with flag: -insecure
//	// - launch game with bots.
//	// - in terminal:
//	// - sudo ./Wall -o
//	//
//
//	uint64_t entityPointer;
//
//	bool isValid = false;
//	std::vector<int> glowIndexes;
//
//	for (int i = 0; i < 64; ++i) {
//		for (int j = 0; j < mem->read<int>(off->engine.m_dwCEngineClientBase + off->engine.m_iGetMaxClients); ++j){
//			entityPointer = mem->read<uint64_t>(off->client.m_dwEntityList + (off->client.m_dwEntityStructSize * j));
//
//			if (entityPointer <= 0x0)
//				continue;
//
//			if (!mem->read<bool>(entityPointer + off->client.m_bDormant) && !mem->read<bool>(entityPointer + off->client.m_bLifeState))
//				glowIndexes.emplace_back(mem->read<int>(entityPointer + off->client.m_iGlowIndex + i));
//		}
//
//		for (int j = 1; j < glowIndexes.size(); ++j)
//		isValid = (glowIndexes[j] > 1 && glowIndexes[j] < 60 ? true : false);
//
//		glowIndexes.clear();
//
//		if (isValid) {
//			printf("\nuint64_t %s\t\t= %s0x%llx%s\n", cT::print("m_iGlowIndex", cT::fG::yellow).c_str(), cT::getColor(cT::fG::green).c_str(), off->client.m_iGlowIndex + i, cT::getStyle(cT::sT::bold).c_str());
//			printf("uint64_t %s\t\t= %s0x%llx%s\n", cT::print("m_flFlashMaxAlpha", cT::fG::yellow).c_str(), cT::getColor(cT::fG::green).c_str(), off->client.m_flFlashMaxAlpha + i, cT::getStyle(cT::sT::bold).c_str());
//			printf("uint64_t %s\t= %s0x%llx%s\n\n", cT::print("m_flFlashDuration", cT::fG::yellow).c_str(), cT::getColor(cT::fG::green).c_str(), off->client.m_flFlashDuration + i, cT::getStyle(cT::sT::bold).c_str());
//			printf("uint64_t %s\t\t= %s0x%llx%s\n\n", cT::print("m_iShotFired", cT::fG::yellow).c_str(), cT::getColor(cT::fG::green).c_str(), off->client.m_iShotFired + i, cT::getStyle(cT::sT::bold).c_str());
//			return;
//		}
//		usleep(refreshRate);
//	}
//
//	if (!isValid) {
//		printf("%s\n", cT::print("Error: Could not find new Offsets\nRetry using the last known Offsets", cT::fG::red, cT::sT::bold).c_str());
//		return;
//	}
//}
