#include "StdAfx.h"
#include "CWaveManager.h"
#include "CWave.h"
#include "CFlock.h"
#include "CTRexAI.h"
#include "../Gameplay/ADino.h"
#include "../Gameplay/CRaptor.h"
#include "../Managers/CObjectFactory.h"
#include "../Game States/CGameWorldState.h"
#include <fstream>
#include <ctime>
#include "..\tinyxml\tinyxml.h"

CWaveManager* CWaveManager::instance = NULL;

CWaveManager::CWaveManager()
{
	m_Wave1 = NULL;
	m_Wave2 = NULL;
	m_nextwave = 0;
	m_latestwave = 0;
	m_totaltime = 0.0f;
	m_nextwavereachtime = 0.0f;

	for(int i=0; i<NUM_WAVES; ++i)
	{
		m_avgtimes[i] = 0.0f;
	}

	m_prevtrexInterrupt = false;
	m_trexInterrupt = false;
}

CWaveManager::~CWaveManager()
{

}

CWaveManager* CWaveManager::GetInstance()
{
	if(!instance)
	{
		instance = new CWaveManager();
	}

	return instance;
}

void CWaveManager::Enter()
{
	m_Wave1 = NULL;
	m_Wave2 = NULL;
	m_nextwave = 0;
	m_latestwave = 0;
	m_totaltime = 0.0f;
	m_nextwavereachtime = 0.0f;

	for(int i=0; i<NUM_WAVES; ++i)
	{
		m_avgtimes[i] = 0.0f;
	}

	m_prevtrexInterrupt = false;
	m_trexInterrupt = false;

	CTRexAI::GetInstance()->Enter();
}

void CWaveManager::Shutdown()
{
	//Shutsdown and free all AI and gameplay memory within the collections
	if( m_Wave1 != NULL )
	{
		m_Wave1->Shutdown();
		delete m_Wave1;
		m_Wave1 = NULL;
	}

	if( m_Wave2 != NULL )
	{
		m_Wave2->Shutdown();
		delete m_Wave2;
		m_Wave2 = NULL;
	}

	m_dinos.clear();

	m_waypoints.clear();
	m_stage1waypoints.clear();
	m_stage2waypoints.clear();
	m_stage3waypoints.clear();

	CTRexAI::GetInstance()->Shutdown();

	if( instance != NULL )
	{
		delete instance;
		instance = NULL;
	}
}

void CWaveManager::ReadWaypointFile(const char* _filename)
{
	TWaypoint wp;

	std::fstream reader(_filename, std::ios_base::binary | std::ios_base::in);

	if(reader.good())
	{
		unsigned int num;
		reader.read((char*)&num, sizeof(num));
		for(unsigned int i=0; i<num; ++i)
		{
			reader.read((char*)&wp, sizeof(TWaypoint));

			for(unsigned int i=0; i<4; ++i)
			{
				wp.m_children[i] = NULL;
			}

			switch(wp.m_stage)
			{
			case 1:
				AddStage1Waypoint(wp);
				break;
			case 2:
				AddStage2Waypoint(wp);
				break;
			case 3:
				AddStage3Waypoint(wp);
				break;
			default:
				AddWaypoint(wp);
				break;
			}
		}
	}
	reader.close();
}

void CWaveManager::ReadWallAvoidanceFile(const char* _filename)
{
	TWallAvoidance wa;

	std::fstream reader(_filename, std::ios_base::binary | std::ios_base::in);

	if(reader.good())
	{
		unsigned int num;
		D3DXVECTOR3 xaxis, yaxis;
		yaxis = D3DXVECTOR3(0.0f, 1.0f, 0.0f);

		reader.read((char*)&num, sizeof(num));
		for(unsigned int i=0; i<num; ++i)
		{
			reader.read((char*)&wa, sizeof(TWallAvoidance));
			xaxis = wa.m_rightPos - wa.m_leftPos;
			D3DXVec3Normalize(&xaxis, &xaxis);
			D3DXVec3Cross(&wa.m_normal, &xaxis, &yaxis);
			AddWallAvoidances(wa);
		}
	}

	reader.close();
}

void CWaveManager::ReadWaveFile(const char* _filename)
{
	TiXmlDocument doc;

	if(doc.LoadFile(_filename) == false)
	{
		return;
	}

	TiXmlElement* xRoot = doc.RootElement();

	if(xRoot == NULL)
	{
		return;
	}

	unsigned int count = 0;

	TiXmlElement* pSite = xRoot->FirstChildElement("Launch"); 
	int tempIndex;
	int tempValue;

	while(pSite)
	{
		pSite->Attribute("Site", &tempIndex);
		if(tempIndex < NUM_LAUNCH_SITES)
		{
			pSite->Attribute("X", &tempValue);
			m_launchSites[tempIndex].m_position.x = tempValue/100.0f;

			pSite->Attribute("Y", &tempValue);
			m_launchSites[tempIndex].m_position.y = tempValue/100.0f;

			pSite->Attribute("Z", &tempValue);
			m_launchSites[tempIndex].m_position.z = tempValue/100.0f;
		}

		pSite = pSite->NextSiblingElement("Launch");
	}

	TiXmlElement* pWave = xRoot->FirstChildElement("Wave");

	while(pWave)
	{
		pWave->Attribute("Index", &tempIndex);
		if(tempIndex < NUM_WAVES)
		{
			m_waveData[tempIndex].m_index = tempIndex;

			pWave->Attribute("Site", &tempValue);
			m_waveData[tempIndex].m_launchSite = tempValue;

			pWave->Attribute("NumRaptors", &tempValue);
			m_waveData[tempIndex].m_numRaptors = tempValue;

			pWave->Attribute("NumPteranodons", &tempValue);
			m_waveData[tempIndex].m_numPteranodons = tempValue;

			pWave->Attribute("NumTriceratops", &tempValue);
			m_waveData[tempIndex].m_numTriceratops = tempValue;

			m_waveData[tempIndex].m_playerAttraction = 50.0f;
			m_waveData[tempIndex].m_playerRepulsion = 0.0f;
			m_waveData[tempIndex].m_coreAttraction = 50.0f;
			m_waveData[tempIndex].m_coreRepulsion = 0.0f;
			m_waveData[tempIndex].m_separationStrength = 1.0f;
		}

		pWave = pWave->NextSiblingElement("Wave");
	}

	doc.Clear();
}

void CWaveManager::SetLaunchPosition(unsigned int _index, D3DXVECTOR3 _position)
{
	if(_index < NUM_LAUNCH_SITES)
	{
		m_launchSites[_index].m_position = _position;
	}
}

unsigned int CWaveManager::GetNumRaptors(unsigned int _index)
{
	if(_index < NUM_WAVES)
	{
		return m_waveData[_index].m_numRaptors;
	}
	return UINT_MAX;
}

unsigned int CWaveManager::GetNumTriceratops(unsigned int _index)
{
	if(_index < NUM_WAVES)
	{
		return m_waveData[_index].m_numTriceratops;
	}
	return UINT_MAX;
}

unsigned int CWaveManager::GetNumPteranodon(unsigned int _index)
{
	if(_index < NUM_WAVES)
	{
		return m_waveData[_index].m_numPteranodons;
	}
	return UINT_MAX;
}

unsigned int CWaveManager::GetLaunchSite(unsigned int _index)
{
	if(_index < NUM_WAVES)
	{
		return m_waveData[_index].m_launchSite;
	}
	return UINT_MAX;
}

float CWaveManager::GetSeparationStrength(unsigned int _index)
{
	if(_index < NUM_WAVES)
	{
		return m_waveData[_index].m_separationStrength;
	}
	return FLT_MAX;
}

float CWaveManager::GetPlayerAttraction(unsigned int _index)
{
	if(_index < NUM_WAVES)
	{
		return m_waveData[_index].m_playerAttraction;
	}
	return FLT_MAX;
}

float CWaveManager::GetPlayerRepulsion(unsigned int _index)
{
	if(_index < NUM_WAVES)
	{
		return m_waveData[_index].m_playerRepulsion;
	}
	return FLT_MAX;
}

float CWaveManager::GetCoreAttraction(unsigned int _index)
{
	if(_index < NUM_WAVES)
	{
		return m_waveData[_index].m_coreAttraction;
	}
	return FLT_MAX;
}

float CWaveManager::GetCoreRepulsion(unsigned int _index)
{
	if(_index < NUM_WAVES)
	{
		return m_waveData[_index].m_coreRepulsion;
	}
	return FLT_MAX;
}

void CWaveManager::AddWaypoint(TWaypoint _wp)
{
	m_waypoints.push_back(_wp);
}

void CWaveManager::DeleteWaypoint(TWaypoint _wp)
{
	std::vector<TWaypoint>::iterator iter;
	for(iter=m_waypoints.begin(); iter!=m_waypoints.end();)
	{
		if((*iter).m_position ==_wp.m_position)
		{
			iter = m_waypoints.erase(iter);
		}
		else
		{
			++iter;
		}
	}
}

void CWaveManager::AddStage1Waypoint(TWaypoint _wp)
{
	m_stage1waypoints.push_back(_wp);
}

void CWaveManager::DeleteStage1Waypoint(TWaypoint _wp)
{
	std::vector<TWaypoint>::iterator iter;
	for(iter=m_stage1waypoints.begin(); iter!=m_stage1waypoints.end();)
	{
		if((*iter).m_position == _wp.m_position)
		{
			iter = m_stage1waypoints.erase(iter);
		}
		else
		{
			++iter;
		}
	}
}

void CWaveManager::AddStage2Waypoint(TWaypoint _wp)
{
	m_stage2waypoints.push_back(_wp);
}

void CWaveManager::DeleteStage2Waypoint(TWaypoint _wp)
{
	std::vector<TWaypoint>::iterator iter;
	for(iter=m_stage2waypoints.begin(); iter!=m_stage2waypoints.end();)
	{
		if((*iter).m_position == _wp.m_position)
		{
			iter = m_stage2waypoints.erase(iter);
		}
		else
		{
			++iter;
		}
	}
}

void CWaveManager::AddStage3Waypoint(TWaypoint _wp)
{
	m_stage3waypoints.push_back(_wp);
}

void CWaveManager::DeleteStage3Waypoint(TWaypoint _wp)
{
	std::vector<TWaypoint>::iterator iter;
	for(iter=m_stage3waypoints.begin(); iter!=m_stage3waypoints.end();)
	{
		if((*iter).m_position == _wp.m_position)
		{
			iter = m_stage3waypoints.erase(iter);
		}
		else
		{
			++iter;
		}
	}
}

void CWaveManager::BuildWaypointSystem()
{
	m_finalWaypoint.m_position = CGameWorldState::GetInstance()->GetBaseCore()->GetFrame().GetLocalPosition();
	m_finalWaypoint.m_attraction = 100.0f;
	m_finalWaypoint.m_repulsion = 0.0f;
	m_finalWaypoint.m_stage = 4;

	for(unsigned int i=0; i<NUM_LAUNCH_SITES; ++i)
	{
		m_launchWaypoints[i].m_position = m_launchSites[i].m_position;
		m_launchWaypoints[i].m_attraction = 100.0f;
		m_launchWaypoints[i].m_repulsion = 0.0f;
		m_launchWaypoints[i].m_stage = 0;
		FindChildren(&m_launchWaypoints[i], i, i);
	}
}

void CWaveManager::FindChildren(TWaypoint* _wp, unsigned int _gate, unsigned int _index)
{
	if(_wp->m_children[0])
		return;

	unsigned int tempIndex;

	switch(_wp->m_stage)
	{
	case 0:
		{
			//link to all stage 1 (2 exclusive children each)
			for(unsigned int i=0; i<2; ++i)
			{
				tempIndex = _index * 2 +i;
				_wp->m_children[i] = &m_stage1waypoints.at(tempIndex);
				FindChildren(_wp->m_children[i], _gate, tempIndex);
			}
		}
		break;
	case 1:
		{
			//link to all stage 2 (one-to-one)
			for(unsigned int i=0; i< 4; ++i)
			{
				tempIndex = _index;
				_wp->m_children[i] = &m_stage2waypoints.at(tempIndex);

				switch(tempIndex)
				{
				case 0:
					{
						FindChildren(_wp->m_children[i], 0, tempIndex);
					}
					break;
				case 1:
				case 2:
					{
						FindChildren(_wp->m_children[i], 1, tempIndex);
					}
					break;
				case 3:
				case 4:
				case 5:
				case 6:
					{
						FindChildren(_wp->m_children[i], 2, tempIndex);
					}
					break;
				case 7:
				case 8:
					{
						FindChildren(_wp->m_children[i], 3, tempIndex);
					}
					break;
				case 9:
					{
						FindChildren(_wp->m_children[i], 4, tempIndex);
					}
					break;
				}
			}
		}
		break;
	case 2:
		{
			//link to all stage 3
			_wp->m_children[0] = &m_stage3waypoints.at(_gate);
			FindChildren(_wp->m_children[0], _gate, 0);
		}
		break;
	case 3:
		{
			//point to root
			_wp->m_children[0] = &m_finalWaypoint;
		}
		break;
	}
}

TWaypoint* CWaveManager::FindNewWaypoint(D3DXVECTOR3 _pos, float _radius, TWaypoint* _ignore)
{
	TWaypoint* temp = NULL;
	float distance, closestdistance = FLT_MAX;
	float range = _radius * _radius;
	D3DXVECTOR3 vec;
	D3DXVECTOR3 wall;

	const std::vector<TWaypoint>& waypoints2 = GetAllStage2Waypoints();
	for(unsigned int i = 0; i < waypoints2.size(); ++i)
	{
		vec = waypoints2.at(i).m_position - _pos;
		distance = D3DXVec3LengthSq(&vec);

		if(distance < closestdistance && distance > range && (&waypoints2.at(i)) != _ignore)
		{
			closestdistance = distance;
			temp = const_cast<TWaypoint*>(&waypoints2.at(i));
		}
	}

	const std::vector<TWaypoint>& waypoints3 = GetAllStage3Waypoints();
	for(unsigned int i = 0; i < waypoints3.size(); ++i)
	{
		vec = waypoints3.at(i).m_position - _pos;
		distance = D3DXVec3LengthSq(&vec);

		if(distance < closestdistance && distance > range && (&waypoints3.at(i)) != _ignore)
		{
			closestdistance = distance;
			temp = const_cast<TWaypoint*>(&waypoints3.at(i));
		}
	}

	vec = m_finalWaypoint.m_position - _pos;
	distance = D3DXVec3LengthSq(&vec);

	if(distance < closestdistance && distance > range && &m_finalWaypoint != _ignore)
	{
		closestdistance = distance;
		temp = &m_finalWaypoint;
	}

	bool flag = false;
	std::vector<TWallAvoidance>& tempWA = const_cast<std::vector<TWallAvoidance>&>(GetAllWallAvoidances());
	std::vector<TWallAvoidance>::iterator iter;

	D3DXVECTOR3 intersectPt;
	float a, b;

	for(iter = tempWA.begin(); iter != tempWA.end(); ++iter)
	{
		vec.y = _pos.y = (*iter).m_leftPos.y = (*iter).m_rightPos.y = 0.0f;
		wall = (*iter).m_leftPos - (*iter).m_rightPos;

		if(FindIntersectionPt(_pos, (*iter).m_rightPos, vec, wall, &intersectPt, &a, &b))
		{
			flag = true;
			break;
		}
	}

	if(!flag)
	{
		temp = &m_finalWaypoint;
	}

	return temp;
}

void CWaveManager::AddWallAvoidances(TWallAvoidance _wa)
{
	m_wallAvoidances.push_back(_wa);
}

void CWaveManager::DeleteWallAvoidances(TWallAvoidance _wa)
{
	std::vector<TWallAvoidance>::iterator iter;
	for(iter=m_wallAvoidances.begin(); iter!=m_wallAvoidances.end();)
	{
		if((*iter).m_leftPos == _wa.m_leftPos && (*iter).m_rightPos == _wa.m_rightPos)
		{
			iter = m_wallAvoidances.erase(iter);
		}
		else
		{
			++iter;
		}
	}
}

bool CWaveManager::FindIntersectionPt(const D3DXVECTOR3& _p1, const D3DXVECTOR3& _p2, const D3DXVECTOR3& _v1, const D3DXVECTOR3& _v2, D3DXVECTOR3* _out, float* _u, float* _t)
{
	*_out = D3DXVECTOR3(0.0f, 0.0f, 0.0f);

	float temp1, temp2;
	float t, u;
	D3DXVECTOR3 vec; 

	D3DXVec3Cross(&vec, &_v1, &_v2);
	temp1 = D3DXVec3Length(&vec);

	if(temp1 == 0.0f)
		return false;

	D3DXVec3Cross(&vec, &(_p2 - _p1), &_v1);
	temp2 = D3DXVec3Length(&vec);
	u = temp2/temp1;

	if(u < 0.0f || u > 1.0f)
		return false;

	vec = _p2 + (_v2 * u) - _p1;
	t = D3DXVec3Length(&vec);

	if(t < 0.0f || t > 1.0f)
		return false;

	*_out = _p1 + _v1 * u;
	*_u = u;
	*_t = t;
	return true;
}

TWaveData* CWaveManager::GetLaunchData(unsigned int _index)
{
	if(_index < NUM_WAVES)
	{
		return &m_waveData[_index];
	}

	return NULL;
}

TLaunchSite* CWaveManager::GetLaunchSiteData(unsigned int _index)
{
	if(_index < NUM_LAUNCH_SITES)
	{
		return &m_launchSites[_index];
	}

	return NULL;
}

void CWaveManager::CreateWave(unsigned int _index, int _raptors, int _triceratops, int _pteranodons)
{

	if(_index >= NUM_WAVES)
		return;

	CWave* tempWave = NULL;

	if(m_Wave1 == NULL)
	{
		m_Wave1 = new CWave(_index);
		tempWave = m_Wave1;
	}
	else if(m_Wave1 != NULL && !m_Wave1->IsActive() && m_Wave1->HasLaunched())
	{
		m_Wave1->Shutdown();
		delete m_Wave1;
		m_Wave1 = new CWave(_index);
		tempWave = m_Wave1;
	}
	else if(m_Wave2 == NULL)
	{
		m_Wave2 = new CWave(_index);
		tempWave = m_Wave2;
	}
	else if(m_Wave2 != NULL && !m_Wave2->IsActive() && m_Wave2->HasLaunched())
	{
		m_Wave2->Shutdown();
		delete m_Wave2;
		m_Wave2 = new CWave(_index);
		tempWave = m_Wave2;
	}

	if(tempWave == NULL)
	{
		return;
	}


	float distfromlaunch = D3DXVec3Length(&(CGameWorldState::GetInstance()->GetBaseCore()->GetFrame().GetWorldPosition() - m_launchSites[m_waveData[_index].m_launchSite].m_position));
	float totalspeed = 0.0f;

	if(_raptors>0)
	{
		CFlock* newflock = new CFlock();
		newflock->GetDinos()->reserve(60);
		for(int i=0; i<_raptors; ++i)
		{
			newflock->AddDino((ADino*)CObjectFactory::GetInstance()->MakeRaptor());
			totalspeed += 20.0f;
		}

		tempWave->AddFlock(newflock);
	}

	if(_triceratops>0)
	{
		CFlock* newflock = new CFlock();
		newflock->GetDinos()->reserve(8);
		for(int i=0; i<_triceratops; ++i)
		{
			newflock->AddDino((ADino*)CObjectFactory::GetInstance()->MakeTriceratop());
			totalspeed += 10.0f;
		}

		tempWave->AddFlock(newflock);
	}

	if(_pteranodons>0)
	{
		CFlock* newflock = new CFlock();
		newflock->GetDinos()->reserve(10);
		for(int i=0; i<_pteranodons; ++i)
		{
			newflock->AddDino((ADino*)CObjectFactory::GetInstance()->MakePteranodon());
		}

		tempWave->AddFlock(newflock);
	}

	m_nextwavereachtime = distfromlaunch/(totalspeed/(_raptors + _triceratops + _pteranodons));
}

void CWaveManager::LaunchWave(unsigned int _index)
{
	if(m_Wave1 != NULL && m_Wave1->GetWaveIndex() == _index)
	{	
		m_Wave1->Launch();
	}
	else if(m_Wave2 != NULL && m_Wave2->GetWaveIndex() == _index)
	{
		m_Wave2->Launch();
	}
}

CWave* CWaveManager::GetWave(unsigned int _index)
{
	if(m_Wave1 != NULL && m_Wave1->GetWaveIndex() == _index)
	{
		return m_Wave1;
	}
	else if(m_Wave2 != NULL && m_Wave2->GetWaveIndex() == _index)
	{
		return m_Wave2;
	}

	return NULL;
}

const std::vector<ADino*> CWaveManager::GetNearestDinos(D3DXVECTOR3 _position, float _radius)
{
	std::vector<ADino*> temp;

	if(m_Wave1 != NULL && m_Wave1->IsActive() && m_Wave1->HasLaunched())
	{
		std::vector<CFlock*>* tempFlocks = m_Wave1->GetFlocks();

		for(unsigned int flockIter = 0; flockIter < tempFlocks->size(); ++flockIter)
		{
			const std::vector<ADino*>* tempDinos = tempFlocks->at(flockIter)->GetDinos();

			for(unsigned int dinoIter = 0; dinoIter < tempDinos->size(); ++dinoIter)
			{
				D3DXMATRIX tempMat = tempDinos->at(dinoIter)->GetFrame().GetWorldMatrix();
				if(D3DXVec3LengthSq(&(D3DXVECTOR3(tempMat._41, tempMat._42, tempMat._43) - _position)) < _radius*_radius)
				{
					temp.push_back(tempDinos->at(dinoIter));
				}
			}			
		}		
	}

	if(m_Wave2 != NULL && m_Wave2->IsActive() && m_Wave2->HasLaunched())
	{
		std::vector<CFlock*>* tempFlocks = m_Wave2->GetFlocks();

		for(unsigned int flockIter = 0; flockIter < tempFlocks->size(); ++flockIter)
		{
			const std::vector<ADino*>* tempDinos = tempFlocks->at(flockIter)->GetDinos();

			for(unsigned int  dinoIter = 0; dinoIter < tempDinos->size(); ++dinoIter)
			{
				D3DXMATRIX tempMat = tempDinos->at(dinoIter)->GetFrame().GetWorldMatrix();
				if(D3DXVec3Length(&(D3DXVECTOR3(tempMat._41, tempMat._42, tempMat._43) - _position)) < _radius)
				{
					temp.push_back(tempDinos->at(dinoIter));
				}
			}			
		}		
	}

	return temp;
}

const std::vector<TWaypoint> CWaveManager::GetNearestWaypoints(D3DXVECTOR3 _position, float _radius)
{
	std::vector<TWaypoint>::iterator iter;
	std::vector<TWaypoint> temp;

	for(iter = m_waypoints.begin(); iter != m_waypoints.end(); ++iter)
	{
		if(D3DXVec3LengthSq(&((*iter).m_position - _position)) < _radius*_radius)
		{
			temp.push_back((*iter));
		}
	}

	return temp;
}

void CWaveManager::Update(float _dt)
{
	//update m_dinos array and waves
	m_dinos.clear();

	if(m_Wave1 && m_Wave1->IsActive() && m_Wave1->HasLaunched())
	{
		m_Wave1->Update(_dt);

		std::vector<CFlock*>* tempFlocks = m_Wave1->GetFlocks();

		for(unsigned int flockIter = 0; flockIter < tempFlocks->size(); ++flockIter)
		{
			const std::vector<ADino*>* tempDinos = tempFlocks->at(flockIter)->GetDinos();

			for(unsigned int dinoIter = 0; dinoIter < tempDinos->size(); ++dinoIter)
			{
				m_dinos.push_back(tempDinos->at(dinoIter));
			}
		}		
	}


	if(m_Wave2 && m_Wave2->IsActive() && m_Wave2->HasLaunched())
	{
		m_Wave2->Update(_dt);

		std::vector<CFlock*>* tempFlocks = m_Wave2->GetFlocks();

		for(unsigned int flockIter = 0; flockIter < tempFlocks->size(); ++flockIter)
		{
			const std::vector<ADino*>* tempDinos = tempFlocks->at(flockIter)->GetDinos();

			for(unsigned int dinoIter = 0; dinoIter < tempDinos->size(); ++dinoIter)
			{
				m_dinos.push_back(tempDinos->at(dinoIter));
			}			
		}	
	}

	if(m_trexInterrupt)
	{
		CTRexAI::GetInstance()->Update(_dt);
		m_prevtrexInterrupt = true;
		if(m_latestwave == NUM_WAVES && GetAllDinos().size() == 0)
		{
			TWaveData* wd = GetLaunchData(rand()%NUM_LAUNCH_SITES);
			CFlock* tempFlock = new CFlock();
			CRaptor* tempRaptor;
			for(int i=0; i<5; ++i)
			{
				tempRaptor = CObjectFactory::GetInstance()->MakeRaptor();
				tempRaptor->SetPointValue(0);
				tempRaptor->SetResourceValue(0);
				tempFlock->AddDino((ADino*)tempRaptor);
			}

			if(m_Wave1)
			{
				m_Wave1->Shutdown();
				delete m_Wave1;
				m_Wave1 = NULL;
			}

			m_Wave1 = new CWave(0);
			m_Wave1->AddFlock(tempFlock);
			m_Wave1->Launch();
		}
		return;
	}

	if(CTRexAI::GetInstance()->IsDone() && !IsWavesDone())
		CTRexAI::GetInstance()->Update(_dt);

	if(!m_trexInterrupt)
		m_totaltime += _dt;

	int currwaves[2] = {-1, -1};

	if(m_Wave1 && (m_Wave1->IsActive() || !m_Wave1->HasLaunched()))
	{
		currwaves[0] = (int)m_Wave1->GetWaveIndex();
	}
	else
	{
		if( m_Wave1 != NULL ) 
		{
			m_Wave1->Shutdown();
			delete m_Wave1;
			m_Wave1 = NULL;
		}

		if(m_nextwave < NUM_WAVES)
		{
			TWaveData* wd = GetLaunchData(m_nextwave);
			CreateWave(m_nextwave, wd->m_numRaptors, wd->m_numTriceratops, wd->m_numPteranodons);
			++m_nextwave;
		}
	}

	if(m_Wave2 && (m_Wave2->IsActive() || !m_Wave2->HasLaunched()))
	{
		currwaves[1] = (int)m_Wave2->GetWaveIndex();
	}	
	else
	{
		if( m_Wave2 != NULL )
		{
			m_Wave2->Shutdown();
			delete m_Wave2;
			m_Wave2 = NULL;
		}

		if(m_nextwave < NUM_WAVES)
		{
			TWaveData* wd = GetLaunchData(m_nextwave);
			CreateWave(m_nextwave, wd->m_numRaptors, wd->m_numTriceratops, wd->m_numPteranodons);
			++m_nextwave;
		}
	}

	//calculate dinos killed so far
	unsigned int totalNum = 0;

	for(unsigned int i=0; i<m_latestwave; ++i)
	{
		bool flag = false;
		if(currwaves[0] == i && !m_Wave1->HasLaunched())
		{
			flag = true;
		}
		else if(currwaves[1] == i && !m_Wave2->HasLaunched())
		{
			flag = true;
		}

		if(!flag)
		{
			totalNum += m_waveData[i].m_numRaptors + m_waveData[i].m_numTriceratops + m_waveData[i].m_numPteranodons;
		}
	}

	unsigned int killed = totalNum - GetAllDinos().size();

	//launch new wave
	if(m_latestwave==0 && m_totaltime >= 3.0f)
	{
		LaunchWave(m_latestwave);
		++m_latestwave;
	}
	else
	{
		if(killed > 0)
		{
			float avgtime = m_totaltime/killed;

			if(m_latestwave < NUM_WAVES && GetWave(m_latestwave))
			{
				float estwavefinishtime = GetAllDinos().size()*avgtime;

				if(m_nextwavereachtime >= estwavefinishtime)
				{
					if((m_latestwave+1)%5==0 && !m_trexInterrupt && !m_prevtrexInterrupt)
					{
						LaunchWave(m_latestwave);
						++m_latestwave;
						m_trexInterrupt = true;
						return;
					}

					m_avgtimes[m_latestwave-1] = avgtime;
					float totalavg = 0.0;
					for(unsigned int i = 0; i<m_latestwave-1; ++i)
					{
						totalavg+=m_avgtimes[i];
					}
					totalavg = totalavg/(float)(m_latestwave-1);

					//if performance is good, increase next wave strength
					if(m_avgtimes[m_latestwave-1] < totalavg)
					{
						CWave* tempwave = GetWave(m_latestwave);
						std::vector<CFlock*>* tempFlocks = tempwave->GetFlocks();

						for(unsigned int flockIter = 0; flockIter < tempFlocks->size(); ++flockIter)
						{
							const std::vector<ADino*>* tempDinos = tempFlocks->at(flockIter)->GetDinos();

							for(unsigned int dinoIter = 0; dinoIter < tempDinos->size(); ++dinoIter)
							{
								tempDinos->at(dinoIter)->AddHealth(50);
								tempDinos->at(dinoIter)->SetPointValue(tempDinos->at(dinoIter)->GetPointValue() + 50);
								tempDinos->at(dinoIter)->SetSpeed(tempDinos->at(dinoIter)->GetSpeed()*2);
							}
						}

					}

					if(!m_trexInterrupt)
					{
						LaunchWave(m_latestwave);
						++m_latestwave;
					}
				}
			}
		}
	}

	m_prevtrexInterrupt = false;
}