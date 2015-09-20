///////////////////////////////////////////////////////////////////////////////////
//		File			:	CWaveManager.h
//
//		Author			:	Riduan Chitty
//
//		Sources			:	None
//
//		Creation		:	12/5/2011
//
//		Purpose:		:	The class that stores AI data like waypoints and controls all AI states like T-Rex coming in and player progression.
//						:	It also handles creation and destruction of waves and gettting and returning dinos from and to object factory
///////////////////////////////////////////////////////////////////////////////////

#ifndef _CWAVEMANAGER_H_
#define _CWAVEMANAGER_H_

#include "d3dx9math.h"
#include "CWave.h"
#include <vector>
#include "../Gameplay/ADino.h"
#include "CTRexAI.h"

#define NUM_LAUNCH_SITES 5
#define NUM_WAVES 15
#define SPAWN_DELAY 5.0f


struct TLaunchSite
{
	D3DXVECTOR3 m_position;
};

struct TWaveData
{
	unsigned int m_index;
	unsigned int m_numRaptors;
	unsigned int m_numPteranodons;
	unsigned int m_numTriceratops;

	unsigned int m_launchSite;

	float m_separationStrength;
	float m_playerAttraction;
	float m_playerRepulsion;
	float m_coreAttraction;
	float m_coreRepulsion;
};

class CWaveManager
{
private:
	static CWaveManager* instance;

	std::vector<ADino*> m_dinos;
	std::vector<TWaypoint> m_waypoints;
	std::vector<TWaypoint> m_stage1waypoints;
	std::vector<TWaypoint> m_stage2waypoints;
	std::vector<TWaypoint> m_stage3waypoints;
	std::vector<TWallAvoidance> m_wallAvoidances;

	CWave* m_Wave1;
	CWave* m_Wave2;

	unsigned int m_nextwave;
	unsigned int m_latestwave;
	float m_totaltime;
	float m_nextwavereachtime;
	float m_avgtimes[NUM_WAVES];
	
	bool m_prevtrexInterrupt;
	bool m_trexInterrupt;

	CWaveManager();
	CWaveManager(const CWaveManager&);
	~CWaveManager();

public:
	TLaunchSite m_launchSites[NUM_LAUNCH_SITES];
	TWaveData m_waveData[NUM_WAVES];
	TWaypoint m_launchWaypoints[NUM_LAUNCH_SITES];
	TWaypoint m_finalWaypoint;

	static CWaveManager* GetInstance();

	void Enter();
	void Shutdown();

	//file io functions
	void ReadWaypointFile(const char* _filename);
	void ReadWallAvoidanceFile(const char* _filename);

	void ReadWaveFile(const char* _filename);
	
	//waypoint functions
	void AddWaypoint(TWaypoint p);
	void DeleteWaypoint(TWaypoint _wp);

	void AddStage1Waypoint(TWaypoint _wp);
	void DeleteStage1Waypoint(TWaypoint _wp);

	void AddStage2Waypoint(TWaypoint _wp);
	void DeleteStage2Waypoint(TWaypoint _wp);

	void AddStage3Waypoint(TWaypoint _wp);
	void DeleteStage3Waypoint(TWaypoint _wp);

	void BuildWaypointSystem();
	void FindChildren(TWaypoint* _wp, unsigned int _gate, unsigned int _index);

	TWaypoint* FindNewWaypoint(D3DXVECTOR3 _pos, float _radius, TWaypoint* _ignore);
	
	void ClearWaypoints() { m_waypoints.clear(); }
	
	//wall avoidance functions
	void AddWallAvoidances(TWallAvoidance _wa);
	void DeleteWallAvoidances(TWallAvoidance _wa);

	static bool FindIntersectionPt(const D3DXVECTOR3& _p1, const D3DXVECTOR3& _p2, const D3DXVECTOR3& _v1, const D3DXVECTOR3& _v2, D3DXVECTOR3* _out, float* _u, float* _t);

	//wave data functions
	TWaveData* GetLaunchData(unsigned int _index); 
	TLaunchSite* GetLaunchSiteData(unsigned int _index);

	void CreateWave(unsigned int _index, int _raptors, int _triceratops, int _pteranodons);
	void LaunchWave(unsigned int _index);
	CWave* GetWave(unsigned int _index);

	const std::vector<ADino*> GetNearestDinos(D3DXVECTOR3 _position, float _radius);
	const std::vector<TWaypoint> GetNearestWaypoints(D3DXVECTOR3 _position, float _radius);

	void SetLaunchPosition(unsigned int _index, D3DXVECTOR3 _position);

	void Update(float _dt);	

	//Accessors
	unsigned int GetNumRaptors(unsigned int _index);
	unsigned int GetNumTriceratops(unsigned int _index);
	unsigned int GetNumPteranodon(unsigned int _index);
	unsigned int GetLaunchSite(unsigned int _index);
	float GetSeparationStrength(unsigned int _index);
	float GetPlayerAttraction(unsigned int _index);
	float GetPlayerRepulsion(unsigned int _index);
	float GetCoreAttraction(unsigned int _index);
	float GetCoreRepulsion(unsigned int _index);

	bool GetTrexInterrupt() { return m_trexInterrupt; }
	
	const std::vector<ADino*>& GetAllDinos() { return m_dinos; }
	const std::vector<TWaypoint>& GetAllWaypoints() { return m_waypoints; }
	const std::vector<TWaypoint>& GetAllStage1Waypoints() { return m_stage1waypoints; }
	const std::vector<TWaypoint>& GetAllStage2Waypoints() { return m_stage2waypoints; }
	const std::vector<TWaypoint>& GetAllStage3Waypoints() { return m_stage3waypoints; }
	
	const std::vector<TWallAvoidance>& GetAllWallAvoidances() { return m_wallAvoidances; }

	bool IsWavesDone() { return (m_latestwave == NUM_WAVES && CTRexAI::GetInstance()->IsDone()); } 
	int GetCurrentWave() { return (int)m_latestwave - 1; }

	//Modifiers
	void SetTrexInterrupt(bool _value) { m_trexInterrupt = _value; }	
};

#endif