///////////////////////////////////////////////////////////////////////////////////
//		File			:	CFlock.h
//
//		Author			:	Riduan Chitty
//
//		Sources			:	None
//
//		Creation		:	12/5/2011
//
//		Purpose:		:	The class that stores flock data and does the AI updating of the individual data.
//						:	
///////////////////////////////////////////////////////////////////////////////////

#ifndef _CFLOCK_H_
#define _CFLOCK_H_

#include <vector>
#include "d3dx9math.h"
#include "../Gameplay/ADino.h"
#include "../Managers/CObjectManager.h"

#define MAX_AI_THREADS 32


class CPlayer;
class CBaseCore;
struct TWaveData;
class CEntity;
class CFlock
{
private:
	std::vector<ADino*> m_dinos;
	unsigned int m_waveIndex;
	float m_separationStrength;
	CPlayer* m_player;
	float m_playerAttraction;
	float m_playerRepulsion;
	CBaseCore* m_core;
	float m_coreAttraction;
	float m_coreRepulsion;
	bool m_active;
	float m_deltaTime;

	CFlock(const CFlock&) {};

public:
	CFlock() {};
	CFlock(std::vector<ADino*> _dinos);
	~CFlock();
	
	void Launch(const TWaveData* _wd);
	void Update(float _dt);
	
	 std::vector<ADino*>* GetDinos();

	void AddDino(ADino* _dino)
	{
		m_dinos.push_back(_dino);
	}

	//Accessors
	unsigned int GetWaveIndex() { return m_waveIndex; } 
	float GetSeparationStrength() { return m_separationStrength; }
	CPlayer* GetPlayer() { return m_player; }
	float GetPlayerAttraction() { return m_playerAttraction; }
	float GetPlayerRepulsion() { return m_playerRepulsion; }
	CBaseCore* GetBaseCore() { return m_core; }
	float GetCoreAttraction() { return m_coreAttraction; }
	float GetCoreRepulsion() { return m_coreRepulsion; }
	bool IsActive() { return m_active; }
	float GetDeltaTime() { return m_deltaTime; }

	//Mutators
	void SetWaveIndex(unsigned int _value) { m_waveIndex = _value; }
	void SetSeparationStrength(float _value) { m_separationStrength = _value; }
	void SetPlayer(CPlayer* _value) { m_player = _value; }
	void SetPlayerAttraction(float _value) { m_playerAttraction = _value; }
	void SetPlayerRepulsion(float _value) { m_playerRepulsion = _value; }
	void SetBaseCore(CBaseCore* _value) { m_core = _value; }
	void SetCoreAttraction(float _value) { m_coreAttraction = _value; }
	void SetCoreRepulsion(float _value) { m_coreRepulsion = _value; }


	void Shutdown()
	{
		for( unsigned int i = 0; i < m_dinos.size(); ++i )
		{
			CObjectManager::GetInstance()->RemoveObject(m_dinos[i]);
			m_dinos[i]->Shutdown();
			delete m_dinos[i];
		}

		m_dinos.clear();

		m_player = NULL;
		m_core = NULL;
	}

	struct TAIData
	{
		//misc temp and final information
		CFlock*	flock;
		D3DXVECTOR3 localPos;
		D3DXVECTOR3 tempPos;
		D3DXVECTOR3 vec;
		D3DXVECTOR3 acceleration;
		D3DXVECTOR3 velocity;
		D3DXVECTOR3 offset;

		//for targeting
		float distance;
		float closestDistance;
		CEntity* target;
		bool hadTarget;
		bool wasAttacking;

		//for potential field
		TWaypoint* currwp;
		D3DXVECTOR3 r;
		D3DXVECTOR3 u;
		float d;
		float U;

		//common vectors
		std::vector<ATower*>* towerList;
		std::vector<CDecoy*>* decoysList;
		std::vector<TWaypoint>* waypointsList;
		std::vector<TWallAvoidance>* wallAvoidanceList;

		//for switching
		unsigned int numChildren;
		unsigned int tempChildIndex;
		bool flag;

		//for acceleration and velocity
		float x;
		float c;
		float a;
		float b;
		float tempSpeed;

		//for wall avoidance
		D3DXVECTOR3 whisker;
		D3DXVECTOR3 wall;
		D3DXVECTOR3 intersectPt;
		D3DXVECTOR3 nearestIntersectPt;
		TWallAvoidance* nearestWall;
	};

	struct TAIPackage
	{
		ADino* m_dino;
		TAIData* m_data;
	};

private:
	TAIData m_dinoData[MAX_AI_THREADS];
	TAIPackage m_dinoPackages[MAX_AI_THREADS];
	DWORD   m_threadIdArray[MAX_AI_THREADS];
    HANDLE  m_threadHandleArray[MAX_AI_THREADS]; 

};

#endif