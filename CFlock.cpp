#include "StdAfx.h"
#include "CFlock.h"
#include "CWaveManager.h"
#include "..\Gameplay\ADino.h"
#include "..\Gameplay\CBaseCore.h"
#include "..\Managers\CObjectFactory.h"
#include "..\Gameplay\CPlayer.h"
#include "..\Gameplay\CBaseCore.h"
#include "..\Managers\CObjectManager.h"
#include "..\Managers\CHudManager.h"
#include "..\Gameplay\CRaptor.h"
#include "../Gameplay/CTriceratop.h"
#include "../Gameplay/CPteranodon.h"
#include "..\Managers\CAudioManager.h"
#include "..\Game States\CUpgradeState.h"
#include "..\Game States\CHighScoreState.h"
#include "..\Game States\CGameWorldState.h"
#include "../Renderer/CRaptorDeathState.h"
#include "../Renderer/CTriceratopsDeathState.h"
#include "../Renderer/CPteranadonDeathState.h"
#include <ctime>

DWORD WINAPI AIThreadFunction(LPVOID lpParam);

CFlock::CFlock(std::vector<ADino*> _dinos)
{
	m_dinos = _dinos;


	m_active = false;
}

CFlock::~CFlock()
{

}

void CFlock::Launch(const TWaveData* _wd)
{
	//Play SFX:
	CAudioManager::GetInstance()->PlaySFX(SFX_NEWWAVE);

	const TLaunchSite* ls = CWaveManager::GetInstance()->GetLaunchSiteData(_wd->m_launchSite);

	m_player = CGameWorldState::GetInstance()->GetPlayer();
	m_core = CGameWorldState::GetInstance()->GetBaseCore();

	D3DXVECTOR3 corepos = CGameWorldState::GetInstance()->GetBaseCore()->GetFrame().GetWorldPosition();
	D3DXMATRIX newmat;
	D3DXMATRIX tempmat;
	D3DXMATRIX rotmat;

	D3DXVECTOR3 xaxis, yaxis;
	D3DXVECTOR3 oripos = ls->m_position, pos;
	D3DXVECTOR3 velocity = corepos - ls->m_position;
	D3DXVec3Normalize(&velocity, &velocity);

	yaxis = D3DXVECTOR3(0.0f, 1.0, 0.0f);
	D3DXVec3Cross(&xaxis, &velocity, &yaxis);
	D3DXVec3Normalize(&xaxis, &xaxis);
	D3DXVec3Cross(&yaxis, &xaxis, &velocity);
	D3DXVec3Normalize(&yaxis, &yaxis);
	pos = oripos;

	newmat._11 = xaxis.x; newmat._12 = xaxis.y; newmat._13 = xaxis.z; newmat._14 = 0.0f;
	newmat._21 = yaxis.x; newmat._22 = yaxis.y; newmat._23 = yaxis.z; newmat._24 = 0.0f;
	newmat._31 = velocity.x; newmat._32 = velocity.y; newmat._33 = velocity.z; newmat._34 = 0.0f;
	newmat._41 = pos.x; newmat._42 = pos.y; newmat._43 = pos.z; newmat._44 = 1.0f;

	std::vector<ADino*>::iterator iter;
	unsigned int count = 0;
	unsigned int modifier = 9;

	float radius;
	for(iter=m_dinos.begin(); iter!=m_dinos.end(); ++iter)
	{
		(*iter)->ResetStats();
		(*iter)->SetMaxWhiskerAngle(30.0f);
		(*iter)->SetCurrWhiskerAngle(0.0f);
		(*iter)->SetCurrWaypoint(&CWaveManager::GetInstance()->m_launchWaypoints[_wd->m_launchSite]);
		(*iter)->SetWhiskerActive(false);
		(*iter)->UpdateCollisionFrames();
		(*iter)->SetVelocity(velocity * (*iter)->GetRoamingSpeed());
		(*iter)->SetAcceleration(D3DXVECTOR3(0.0f, 0.0f, 0.0f));
		radius = ((CCollisionMesh*)((*iter)->GetFastIndexedObj(0)->GetMesh()))->GetRadius();

		if(count%modifier==0)
		{
			oripos -= velocity * radius * 8.0f;
			pos = oripos;
			tempmat = (*iter)->GetFrame().GetWorldMatrix();
		}
		else
		{
			float angle = (float)((count%modifier)-1) * 45.0f;
			D3DXMatrixRotationY(&rotmat, D3DXToRadian(angle));
			D3DXMatrixMultiply(&rotmat, &tempmat, &rotmat);
			pos = oripos + D3DXVECTOR3(rotmat._11, rotmat._12, rotmat._13) * radius * 3.0f;
		}

		if((*iter)->GetType() == ADino::DINO_PTERANADON)
		{
			pos.y = 120.0f;
		}
		newmat._41 = pos.x; newmat._42 = pos.y; newmat._43 = pos.z; 
		(*iter)->GetFrame().GetWorldMatrix()=newmat;
		CObjectManager::GetInstance()->AddObject((*iter));
		++count;
	}

	m_active = true;
	m_waveIndex = _wd->m_index;
	m_separationStrength = _wd->m_separationStrength;
	m_playerAttraction = _wd->m_playerAttraction;
	m_playerRepulsion = _wd->m_playerRepulsion;
	m_coreAttraction = _wd->m_coreAttraction;
	m_coreRepulsion = _wd->m_coreRepulsion;
}

void CFlock::Update(float _dt)
{
	//do ai stuff and check for activeness
	if(m_dinos.size()==0 &&CGameWorldState::GetInstance()->m_DeadPteranodons.size()==0&&CGameWorldState::GetInstance()->m_DeadTriceratops.size()==0
		&&CGameWorldState::GetInstance()->m_DeadRaptors.size()==0)
	{
		m_active = false;
	}

	if(!m_active)
	{
		return;
	}

	std::vector<ADino*>::iterator iter;
	D3DXMATRIX localMat;
	D3DXVECTOR3 localPos;
	D3DXMATRIX tempMat;
	D3DXVECTOR3 tempPos;

	for(iter=m_dinos.begin(); iter!=m_dinos.end();)
	{
		if((*iter)->GetHealth()<=0)
		{
			//Set score based on dino
			CUpgradeState::GetInstance()->SetScore((*iter)->GetPointValue() + 
				CUpgradeState::GetInstance()->GetScore());
			//Set hats based on dino
			CUpgradeState::GetInstance()->SetTopHats((*iter)->GetResourceValue() + 
				CUpgradeState::GetInstance()->GetTopHats());

			CHighScoreState::GetInstance()->GetCurrPlayer()->_totalHats += (*iter)->GetResourceValue();

			CAudioManager::GetInstance()->PlaySFX(SFX_TOPHAT);


			switch((*iter)->GetType())
			{
			case ADino::DINO_RAPTOR:
				{
					CObjectFactory::GetInstance()->ReturnRaptor((CRaptor*)(*iter));
					((CRaptor*)(*iter))->m_AnimStateMachine->ChangeState(CRaptorDeathState::GetState());
					CGameWorldState::GetInstance()->m_DeadRaptors.push_back((CRaptor*)(*iter));
				}
				break;
			case ADino::DINO_TRICERATOPS:
				{
					CObjectFactory::GetInstance()->ReturnTriceratop((CTriceratop*)(*iter));
					CAudioManager::GetInstance()->PlaySFX(SFX_TRICERATOPS_DEATH);
					((CTriceratop*)(*iter))->m_AnimStateMachine->ChangeState(CTriceratopsDeathState::GetState());
					CGameWorldState::GetInstance()->m_DeadTriceratops.push_back((CTriceratop*)(*iter));
				}
				break;
			case ADino::DINO_PTERANADON:
				{
					CObjectFactory::GetInstance()->ReturnPteranodon((CPteranodon*)(*iter));
					CAudioManager::GetInstance()->PlaySFX(SFX_PTERANODON_DEATH);
					((CPteranodon*)(*iter))->m_AnimStateMachine->ChangeState(CPteranadonDeathState::GetState(), 1.25f);
					CGameWorldState::GetInstance()->m_DeadPteranodons.push_back((CPteranodon*)(*iter));
				}
				break;
			default:
				{
					delete (*iter);
				}
				break;
			}
			iter = m_dinos.erase(iter);

		}
		else
		{
			localPos = (*iter)->GetFrame().GetWorldPosition();
			// NEW MINIMAP STUFF
			D3DXVECTOR3 playerPos = CGameWorldState::GetInstance()->GetPlayer()->GetFrame().GetWorldPosition();
			D3DXVECTOR3 playerForward = CGameWorldState::GetInstance()->GetPlayer()->GetFrame().GetWorldZAxis();
			D3DXVECTOR3 playerRight = CGameWorldState::GetInstance()->GetPlayer()->GetFrame().GetWorldXAxis();


			D3DXVec3Normalize(&playerForward, &playerForward);
			D3DXVec3Normalize(&playerRight, &playerRight);
			D3DXVECTOR3 vecToDino;
			vecToDino = localPos - playerPos;
			vecToDino.y = 0.0f;  // we don't care about Y movement


			float distance;
			distance = D3DXVec3Length(&vecToDino);

			D3DXVec3Normalize(&vecToDino, &vecToDino);
			float finalAngle = 0.0f;
			float result1 = D3DXVec3Dot(&vecToDino, &playerForward);
			float result2 = D3DXVec3Dot(&vecToDino, &playerRight);

			if( result1 > 0.0f )
			{
				if( result2 > 0.0f )
				{
					// Upper right quad
					finalAngle += result1 * 3.14f / 2.0f;
				}
				else
				{
					finalAngle += 3.14f / 2.0f - (result2 * 3.14f / 2.0f);
				}
			}
			else
			{
				if( result2 > 0.0f )
				{
					// Upper right quad
					finalAngle += 3.14f + 3.14f / 2.0f + result2 * 3.14f / 2.0f;
				}
				else
				{
					finalAngle += 3.14f - result1 * 3.14f / 2.0f;

				}
			}

			D3DXVECTOR4 result3;
			D3DXMATRIX rotation;
			D3DXMatrixRotationY(&rotation, finalAngle);

			D3DXVECTOR3 right(1.0f, 0.0f, 0.0f);

			D3DXVec3Transform(&result3, &right,&rotation );

			D3DXVECTOR3 drawVec;
			drawVec.x = result3.x;
			drawVec.y = result3.y;
			drawVec.z = result3.z;

			// Calculate distance
			float distanceScale = distance / 500.0f;
			if( distanceScale > 1.0f )
				distanceScale = 1.0f;

			if( distanceScale < 0.15f )
				distanceScale = 0.15f;

			drawVec = drawVec * 90.0f * distanceScale;	// the 90.0f is the radius of the minimap

			CHudManager::GetInstance()->SetDinoVecs(drawVec,(*iter)->GetType());
			++iter;
		}
	}

	m_deltaTime = _dt;
	memset(m_dinoData, 0, sizeof(TAIData) * MAX_AI_THREADS);
	for(unsigned int i = 0; i < MAX_AI_THREADS; ++i)
	{
		m_dinoData[i].flock = this;
		m_dinoData[i].towerList = &CObjectManager::GetInstance()->m_ActiveTowers;
		m_dinoData[i].decoysList = &CObjectManager::GetInstance()->m_ActiveDecoys;
		m_dinoData[i].waypointsList = const_cast<vector<TWaypoint>*>(&CWaveManager::GetInstance()->GetAllWaypoints());
		m_dinoData[i].wallAvoidanceList = const_cast<vector<TWallAvoidance>*>(&CWaveManager::GetInstance()->GetAllWallAvoidances());

		m_dinoPackages[i].m_data = &m_dinoData[i];
	}

	bool flag = false;
	unsigned int count;
	std::vector<ADino*>::iterator dinoIter = m_dinos.begin();

	if(dinoIter == m_dinos.end())
		flag = true;

	while(!flag)
	{
		count = 0;

		while(true)
		{
			m_dinoPackages[count].m_dino = (*dinoIter);
			++dinoIter;

			if(dinoIter == m_dinos.end())
			{
				flag = true;
				break;
			}
			else if(count+1 == MAX_AI_THREADS)
			{
				break;
			}

			++count;
		}

		//Non-Threaded Implementation
		for(unsigned int i = 0; i <= count; ++i)
		{
			AIThreadFunction(&m_dinoPackages[i]);	
		}

	}
}

std::vector<ADino*>* CFlock::GetDinos()
{
	return &m_dinos;
}

DWORD WINAPI AIThreadFunction(LPVOID lpParam)
{
	//initialize all thread data
	srand((unsigned int)time(0) + rand()%1000);
	CFlock::TAIPackage* package = (CFlock::TAIPackage*)lpParam;
	ADino* dino = package->m_dino;
	CFlock::TAIData* data = package->m_data;

	std::vector<ATower*>::iterator towerIter;
	std::vector<CDecoy*>::iterator decoyIter;
	std::vector<TWaypoint>::iterator wpIter;
	std::vector<TWallAvoidance>::iterator waIter;

	if(!dino)
		return 0;

	if((dino->GetType()==ADino::DINO_RAPTOR || dino->GetType()==ADino::DINO_PTERANADON) && dino->IsStunned())
		return 0;

	data->localPos = dino->GetFrame().GetWorldPosition();
	data->localPos.y = 0.0f;

	//set attack to be a blank slate
	data->hadTarget = dino->GetHasTarget();
	data->wasAttacking = dino->GetIsAttacking();

	dino->ClearAttackList();
	dino->SetHasTarget(false);
	dino->SetIsAttacking(false);

	data->closestDistance;
	data->target = NULL;

	//target accquisition for dinos
	if(dino->GetType() != ADino::DINO_PTERANADON)
	{
		data->closestDistance = dino->GetSearchRange() * dino->GetSearchRange();

		for(towerIter = data->towerList->begin(); towerIter != data->towerList->end(); ++towerIter)
		{
			data->vec = (*towerIter)->GetFrame().GetWorldPosition() - data->localPos;
			data->vec.y = 0.0f;

			data->distance = D3DXVec3LengthSq(&data->vec);

			if(data->distance < data->closestDistance)
			{
				if((*towerIter)->GetHealth() > 0 && (*towerIter)->GetIsPlaced())
				{
					data->closestDistance = data->distance;
					data->target = (*towerIter);
				}
			}
		}

		data->vec = data->flock->GetPlayer()->GetFrame().GetWorldPosition() - data->localPos;
		data->vec.y = 0.0f;
		data->distance = D3DXVec3LengthSq(&data->vec);
		D3DXVec3Normalize(&data->vec, &data->vec);

		if(D3DXVec3Dot(&data->vec, &data->flock->GetPlayer()->GetFrame().GetWorldZAxis()) >= 0.707f)
		{
			if(data->distance < data->flock->GetPlayer()->GetGunZeroDist() * data->flock->GetPlayer()->GetGunZeroDist() && data->distance > (float)(SHOOT_Z_OFFSET * SHOOT_Z_OFFSET))
			{
				data->flock->GetPlayer()->SetGunZeroDist(sqrt(data->distance));
			}
		}

		if(data->distance < dino->GetSearchRange() * dino->GetSearchRange() && data->flock->GetPlayer()->GetHealth() > 0 && !data->flock->GetPlayer()->m_WonOrLost)
		{
			data->closestDistance = data->distance;
			data->target = data->flock->GetPlayer();
		}

		if(dino->GetType() != ADino::DINO_TRICERATOPS)
		{
			data->closestDistance = dino->GetSearchRange() * dino->GetSearchRange();

			for(decoyIter = data->decoysList->begin(); decoyIter != data->decoysList->end(); ++decoyIter)
			{
				data->vec = (*decoyIter)->GetFrame().GetWorldPosition() - data->localPos;

				data->vec.y = 0.0f;
				data->distance = D3DXVec3LengthSq(&data->vec);

				if(data->distance < data->closestDistance)
				{
					data->closestDistance = data->distance;
					data->target = (*decoyIter);
				}
			}
		}

		if(data->target)
		{
			dino->SetHasTarget(true);
			dino->AddToAttackList(data->target);
		}
	}

	//change acceleration mode based on has target
	if(data->hadTarget && !data->wasAttacking && !dino->GetHasTarget())//slow down if no longer has target or was attacking
	{
		dino->SetAccelerationMode(ADino::NEGATIVE_ACCELERATION);
		dino->SetAccelerationTimer(0.0f);
	}
	else if(!data->hadTarget && dino->GetHasTarget() && !dino->GetIsAttacking())//accelerate if found new target
	{
		dino->SetAccelerationMode(ADino::POSITIVE_ACCELERATION);
		dino->SetAccelerationTimer(0.0f);		
	}
	else if(dino->GetIsAttacking())//stop movement when attacking
	{
		dino->SetSpeed(0.1f);
		dino->SetAccelerationMode(ADino::NO_ACCELERATION);
		dino->SetAccelerationTimer(0.0f);
	}
	else if(data->wasAttacking && !dino->GetIsAttacking())//speed back up to roaming speed if no longer attacking
	{
		dino->SetAccelerationMode(ADino::POSITIVE_ACCELERATION);
		dino->SetAccelerationTimer(0.0f);
	}

	//Lenard-Jones Potential Function and Flocking
	data->acceleration = data->velocity = data->offset = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
	data->currwp = dino->GetCurrWaypoint();

	//seperation
	std::vector<ADino*> tempDinos = CWaveManager::GetInstance()->GetNearestDinos(dino->GetFrame().GetWorldPosition(), dino->GetAttackRange());
	std::vector<ADino*>::iterator tempIter;

	for(tempIter = tempDinos.begin(); tempIter != tempDinos.end(); ++tempIter)
	{
		if((*tempIter)==dino)
			continue;

		data->tempPos = (*tempIter)->GetFrame().GetWorldPosition();
		data->tempPos.y = 0.0f;
		data->d = D3DXVec3Length(&(data->localPos - data->tempPos));

		if(dino->GetType() == ADino::DINO_PTERANADON && (*tempIter)->GetType() == ADino::DINO_PTERANADON)
		{
			data->acceleration += (data->localPos - data->tempPos) * ((data->flock->GetSeparationStrength() * (dino->GetSearchRange() + (*tempIter)->GetSearchRange()))/(data->d * data->d));
		}
		else if(dino->GetType() != ADino::DINO_PTERANADON && (*tempIter)->GetType() != ADino::DINO_PTERANADON)
		{
			data->acceleration += (data->localPos - data->tempPos) * ((data->flock->GetSeparationStrength() * (dino->GetAttackRange() + (*tempIter)->GetAttackRange()))/(data->d * data->d));
		}
	}

	if(dino->GetType() != ADino::DINO_PTERANADON && data->currwp->m_stage > 1)
	{
		if(dino->GetWhiskerActive())
		{
			//wall avoidance
			data->nearestWall = NULL;
			data->closestDistance = 1.01f;

			for(waIter = data->wallAvoidanceList->begin(); waIter != data->wallAvoidanceList->end(); ++waIter)
			{
				//right whisker check
				data->whisker = dino->GetFrame().GetWorldZAxis() + (dino->GetFrame().GetWorldXAxis() - dino->GetFrame().GetWorldZAxis()) * (dino->GetCurrWhiskerAngle()/90.0f);
				D3DXVec3Normalize(&data->whisker, &data->whisker);
				data->whisker *= dino->GetSearchRange();

				(*waIter).m_leftPos.y = (*waIter).m_rightPos.y = 0.0f;
				data->wall = (*waIter).m_leftPos - (*waIter).m_rightPos;

				if(CWaveManager::GetInstance()->FindIntersectionPt(data->localPos, (*waIter).m_rightPos, data->whisker, data->wall, &data->intersectPt, &data->a, &data->b))
				{
					if(data->a < data->closestDistance)
					{
						data->closestDistance = data->a;
						data->nearestWall = &(*waIter);
						data->nearestIntersectPt = (*waIter).m_rightPos + (data->wall * data->b);
					}
				}

				//left whisker check
				data->vec = -dino->GetFrame().GetWorldXAxis();
				data->whisker = dino->GetFrame().GetWorldZAxis() + (data->vec - dino->GetFrame().GetWorldZAxis()) * (dino->GetCurrWhiskerAngle()/90.0f);
				D3DXVec3Normalize(&data->whisker, &data->whisker);
				data->whisker *= dino->GetSearchRange();

				if(CWaveManager::GetInstance()->FindIntersectionPt(data->localPos, (*waIter).m_rightPos, data->whisker, data->wall, &data->intersectPt, &data->a, &data->b))
				{
					if(data->a < data->closestDistance)
					{
						data->closestDistance = data->a;
						data->nearestWall = &(*waIter);
						data->nearestIntersectPt = (*waIter).m_rightPos + (data->wall * data->b);
					}
				}
			}

			if(data->nearestWall)
			{
				data->d = data->closestDistance;
				data->acceleration += data->nearestWall->m_normal/(data->d);		

				dino->SetCurrWhiskerAngle(dino->GetCurrWhiskerAngle() + 20.0f);

				if(dino->GetCurrWhiskerAngle() > dino->GetMaxWhiskerAngle())
				{
					dino->SetCurrWhiskerAngle(dino->GetMaxWhiskerAngle());
				}
			}
			else
			{
				dino->SetCurrWhiskerAngle(dino->GetCurrWhiskerAngle() - 1.0f);

				if(dino->GetCurrWhiskerAngle() < 0.0f)
				{
					dino->SetCurrWhiskerAngle(0.0f);
				}
			}
		}

		//obstacle avoidance with trees
		for(wpIter = data->waypointsList->begin(); wpIter != data->waypointsList->end(); ++wpIter)
		{
			//apply repulsion
			data->tempPos = (*wpIter).m_position;
			data->tempPos.y = 0.0f;
			data->r = data->localPos - data->tempPos;
			D3DXVec3Normalize(&data->u, &data->r);
			data->d = D3DXVec3Length(&data->r);

			if(data->d > dino->GetSearchRange())
				continue;

			data->U = (((*wpIter).m_repulsion * dino->GetAttackRange())/(data->d));
			data->acceleration += data->u * data->U;
		}		
	}

	if(dino->GetHasTarget())
	{
		//set target as a very strong attraction
		data->tempPos = dino->GetCurrentTarget()->GetFrame().GetWorldPosition();
		data->tempPos.y = 0.0f;
		data->r = data->localPos - data->tempPos;
		D3DXVec3Normalize(&data->u, &data->r);
		data->d = D3DXVec3Length(&data->r);
		data->U = -(1000.0f/(data->d));
		data->acceleration += data->u * data->U;

		if(dino->GetCurrentTarget() == data->flock->GetPlayer())
		{
			dino->SetWhiskerActive(true);
		}
	}
	else if(data->hadTarget)
	{
		dino->SetCurrWaypoint(CWaveManager::GetInstance()->FindNewWaypoint(dino->GetFrame().GetWorldPosition(), 0.0f, dino->GetCurrWaypoint()));
	}

	if(!dino->GetHasTarget() && !data->nearestWall)
	{
		data->tempPos = data->currwp->m_position;
		data->tempPos.y = 0.0f;
		data->r = data->localPos - data->tempPos;
		D3DXVec3Normalize(&data->u, &data->r);
		data->d = D3DXVec3Length(&data->r);
		data->U = -((data->currwp->m_attraction * dino->GetAttackRange())/(data->d));
		data->acceleration += data->u * data->U;

		//if reach within range of waypoint for non-pteranodons
		if(data->currwp != &CWaveManager::GetInstance()->m_finalWaypoint && D3DXVec3LengthSq(&(data->localPos - data->tempPos)) < dino->GetAttackRange() * dino->GetAttackRange())
		{			
			data->numChildren = 0;

			for(unsigned int i = 0; i < 4; ++i)
			{
				if(data->currwp->m_children[i])
				{
					++data->numChildren;
				}
				else
				{
					break;
				}
			}

			data->tempChildIndex = rand()%data->numChildren;

			if(data->currwp->m_children[data->tempChildIndex])
			{
				if(data->currwp->m_children[data->tempChildIndex]->m_stage > data->currwp->m_stage)
				{
					data->currwp = data->currwp->m_children[data->tempChildIndex];
					dino->SetCurrWaypoint(data->currwp);
				}
			}			
		}
		//if pteranodons reach final waypoint
		else if(data->currwp == &CWaveManager::GetInstance()->m_finalWaypoint && dino->GetType() == ADino::DINO_PTERANADON  && D3DXVec3LengthSq(&(data->localPos - data->tempPos)) < dino->GetAttackRange() * dino->GetAttackRange())
		{
			if(rand()%10!=0)
			{
				dino->SetCurrWaypoint(CWaveManager::GetInstance()->FindNewWaypoint(data->localPos, dino->GetAttackRange(), dino->GetCurrWaypoint()));
			}
			else
			{
				dino->SetCurrWaypoint(const_cast<TWaypoint*>(&CWaveManager::GetInstance()->GetAllStage2Waypoints().at(rand()%CWaveManager::GetInstance()->GetAllStage2Waypoints().size())));
			}
		}
	}

	//increase acceleration timer if needed
	if(dino->GetAccelerationMode() != ADino::NO_ACCELERATION)
	{
		dino->SetAccelerationTimer(dino->GetAccelerationTimer() + data->flock->GetDeltaTime());
		if(dino->GetAccelerationTimer() > dino->GetMaxAcceleration()*2.0f)
		{
			dino->SetAccelerationTimer(0.0f);
		}
	}

	//(-(x-10)^2)/10 +10
	//acceleration has an increasing acceleration till max acceleration time and 
	//decreasing acceleration for another max acceleration seconds to simulate a sprint burst
	data->x = dino->GetAccelerationTimer();
	data->c = dino->GetMaxAcceleration();
	data->a = -((data->x - data->c) * (data->x - data->c))/data->c + data->c;
	data->tempSpeed = dino->GetSpeed();

	if(dino->GetAccelerationMode() == ADino::POSITIVE_ACCELERATION)
	{
		data->tempSpeed += data->a;

		if(dino->GetHasTarget() && data->tempSpeed > dino->GetRoamingSpeed())
		{
			data->tempSpeed = dino->GetMaxSpeed();
			dino->SetAccelerationMode(ADino::NO_ACCELERATION);
			dino->SetAccelerationTimer(0.0f);
		}
		else if(!dino->GetHasTarget() && data->tempSpeed > dino->GetRoamingSpeed())
		{
			data->tempSpeed = dino->GetRoamingSpeed();
			dino->SetAccelerationMode(ADino::NO_ACCELERATION);
			dino->SetAccelerationTimer(0.0f);
		}
	}
	else if(dino->GetAccelerationMode() == ADino::NEGATIVE_ACCELERATION)
	{
		data->tempSpeed -= data->a;

		if(!dino->GetHasTarget() && data->tempSpeed < dino->GetRoamingSpeed())
		{
			data->tempSpeed = dino->GetRoamingSpeed();
			dino->SetAccelerationMode(ADino::NO_ACCELERATION);
			dino->SetAccelerationTimer(0.0f);
		}
		else if(data->tempSpeed < 0.0f)
		{
			data->tempSpeed = 0.0f;
			dino->SetAccelerationMode(ADino::POSITIVE_ACCELERATION);
			dino->SetAccelerationTimer(0.0f);
		}
	}

	D3DXVec3Normalize(&data->acceleration, &data->acceleration);
	data->acceleration *= data->tempSpeed;
	data->velocity += dino->GetVelocity();
	data->velocity += data->acceleration;
	D3DXVec3Normalize(&data->velocity, &data->velocity);

	if(!data->nearestWall)
	{
		data->a = D3DXVec3Dot(&dino->GetFrame().GetWorldZAxis(), &data->velocity);
		if(data->a < cos(D3DXToRadian(15.0f)))
		{
			data->b = 15.0f/D3DXToDegree(acos(data->a));
			D3DXVec3Lerp(&data->velocity, &dino->GetFrame().GetWorldZAxis(), &data->velocity, data->b);
			D3DXVec3Normalize(&data->velocity, &data->velocity);
		}
	}

	data->velocity *= data->tempSpeed;

	dino->SetAcceleration(data->acceleration);
	dino->SetSpeed(data->tempSpeed);
	dino->SetVelocity(data->velocity);

	data->offset += dino->GetFrame().GetWorldPosition();
	dino->GetFrame().GetWorldMatrix()._41 = data->offset.x;
	dino->GetFrame().GetWorldMatrix()._42 = data->offset.y;
	dino->GetFrame().GetWorldMatrix()._43 = data->offset.z;

	return 0;
}