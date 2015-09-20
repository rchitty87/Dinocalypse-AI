#include "StdAfx.h"

#include "CTRexAI.h"
#include "../Gameplay/CTRex.h"
#include "CWaveManager.h"
#include "../Game States/CGameWorldState.h"
#include "../Managers/CObjectManager.h"
#include "../Managers/CHudManager.h"
#include "../Renderer/CTRexDeathState.h"
#include "../Renderer/CTRexRunState.h"
#include "../Managers/CAudioManager.h"
#include "../Game States/CUpgradeState.h"
#include "../Game States/CHighScoreState.h"

CTRexAI* CTRexAI::m_instance = NULL;

CTRexAI::CTRexAI()
{
	m_leaving = false;
	m_trex = NULL;
	m_phase = 0;
}

CTRexAI::~CTRexAI()
{
}

CTRexAI::CTRexAI(const CTRexAI&)
{
}

CTRexAI* CTRexAI::GetInstance()
{
	if(!m_instance)
	{
		m_instance = new CTRexAI();
	}
	return m_instance;
}

void CTRexAI::Enter()
{
	m_trex = new CTRex();
	m_trex->GetFrame().GetWorldMatrix()._43 = 10000.0f;
	m_phase = 0;
	m_trex->ResetStats();
	CObjectManager::GetInstance()->AddObject(m_trex);
	m_leaving = false;
	m_timeractive = true;
	m_timer = 5.0f;
	m_targettype = ENTITY_CORE;
	m_targettimer = 0.0f;
	m_targetmaxtime = 3.0f;
}

void CTRexAI::Shutdown()
{
	if(m_trex)
	{
		CObjectManager::GetInstance()->RemoveObject(m_trex);
		m_trex->Shutdown();
		delete m_trex;
		m_trex = NULL;
	}

	if(m_instance)
	{
		delete m_instance;
		m_instance = NULL;
	}
}

void CTRexAI::Update(float _dt)
{
	if(m_trex->m_Dead)
	{
		m_trex->Update(_dt);
		m_trex->AddToRenderList();
		return;
	}

	if(m_timeractive)
	{
		m_timer -= _dt;

		if(m_timer <= 0.0f)
		{
			m_timer = 0.0f;
			m_timeractive = false;
			Launch();
		}
		else
		{
			return;
		}
	}

	m_targettimer += _dt;

	D3DXVECTOR3 localPos = m_trex->GetFrame().GetWorldPosition();
	
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

	CHudManager::GetInstance()->SetDinoVecs(drawVec, m_trex->GetType());

	std::vector<ATower*>* towerList = &CObjectManager::GetInstance()->m_ActiveTowers;
	std::vector<ATower*>::iterator towerIter;
	std::vector<CDecoy*>* decoyList = &CObjectManager::GetInstance()->m_ActiveDecoys;
	std::vector<CDecoy*>::iterator decoyIter;
	std::vector<TWaypoint> tempWaypoints = CWaveManager::GetInstance()->GetAllWaypoints();
	std::vector<TWaypoint>::iterator wpIter;

	D3DXVECTOR3 acceleration = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
	D3DXVECTOR3 velocity = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
	D3DXVECTOR3 tempPos = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
	D3DXVECTOR3 r, u;
	float d, U;
	D3DXVECTOR3 vec;

	bool wasattacking = m_trex->GetIsAttacking();
	bool hadtarget = m_trex->GetHasTarget();
	m_trex->ClearAttackList();
	m_trex->SetIsAttacking(false);
	m_trex->SetHasTarget(false);
	
	CEntity* target = NULL;

	if(!m_leaving)
	{
		if(m_trex->GetHealth() <=0)
		{
			if(m_phase==NUM_TREX_PHASES-1)
			{
				m_trex->m_AnimStateMachine->ChangeState(CTRexDeathState::GetState());
				m_trex->m_Dead = true;
				CObjectManager::GetInstance()->RemoveObject(m_trex);
				++m_phase;
				CAudioManager::GetInstance()->StopBossFightAndPlayGameplay();

			}
			else
			{
				m_trex->SetMaxHealth((short)(m_trex->GetMaxHealth()*1.5f));
				m_trex->SetHealth(m_trex->GetMaxHealth());
				m_leaving = true;
				CAudioManager::GetInstance()->StopBossFightAndPlayGameplay();
				m_trex->m_AnimStateMachine->ChangeState(CTRexRunState::GetState());
				m_trex->m_AttackAnimComplete=true;
				float distance, closestDistance = FLT_MAX;

				if(m_trex->GetCurrWaypoint())
				{
					distance = D3DXVec3LengthSq(&(localPos - CWaveManager::GetInstance()->m_launchWaypoints[0].m_position));

					if(distance < closestDistance)
					{
						closestDistance = distance;
						m_trex->SetCurrWaypoint(&CWaveManager::GetInstance()->m_launchWaypoints[0]);
					}

					distance = D3DXVec3LengthSq(&(localPos - CWaveManager::GetInstance()->m_launchWaypoints[4].m_position));

					if(distance < closestDistance)
					{
						closestDistance = distance;
						m_trex->SetCurrWaypoint(&CWaveManager::GetInstance()->m_launchWaypoints[4]);
					}
				}
				else
				{
					distance = D3DXVec3LengthSq(&(localPos - CWaveManager::GetInstance()->GetAllStage3Waypoints().at(1).m_position));

					if(distance < closestDistance)
					{
						closestDistance = distance;
						m_trex->SetCurrWaypoint(const_cast<TWaypoint*>(&CWaveManager::GetInstance()->GetAllStage3Waypoints().at(1)));
					}

					distance = D3DXVec3LengthSq(&(localPos - CWaveManager::GetInstance()->GetAllStage3Waypoints().at(4).m_position));

					if(distance < closestDistance)
					{
						closestDistance = distance;
						m_trex->SetCurrWaypoint(const_cast<TWaypoint*>(&CWaveManager::GetInstance()->GetAllStage3Waypoints().at(4)));
					}
				}
			}
		}
		
		if(m_targettype == ENTITY_PLAYER)
		{
			if(CGameWorldState::GetInstance()->GetPlayer()->GetHealth() <= 0)
			{
				SetTargetType(ENTITY_CORE);
				target = CGameWorldState::GetInstance()->GetBaseCore();
			}
			else
			{
				target = CGameWorldState::GetInstance()->GetPlayer();
				tempPos = target->GetFrame().GetWorldPosition();

				vec = tempPos - CGameWorldState::GetInstance()->GetBaseCore()->GetFrame().GetWorldPosition();
				if(D3DXVec3Length(&vec) > m_trex->GetAttackRange() * 2.0f)
				{
					SetTargetType(ENTITY_CORE);
					target = CGameWorldState::GetInstance()->GetBaseCore();
				}
			}
		}
		else if(m_targettype == ENTITY_TOWER)
		{
			float closestDistance = FLT_MAX;
			unsigned int numTowers = 0;

			for(towerIter=towerList->begin(); towerIter!=towerList->end(); ++towerIter)
			{
				vec = (*towerIter)->GetFrame().GetWorldPosition() - CGameWorldState::GetInstance()->GetBaseCore()->GetFrame().GetWorldPosition();
				if(D3DXVec3Length(&vec) > m_trex->GetAttackRange() * 2.0f)
					continue;

				++numTowers;

				float distance = D3DXVec3LengthSq(&((*towerIter)->GetFrame().GetWorldPosition() - m_trex->GetFrame().GetWorldPosition()));

				if(distance < closestDistance)
				{
					closestDistance = distance; 
					target = (*towerIter);
				}
			}

			if(numTowers==0)
			{
				SetTargetType(ENTITY_CORE);
				target = CGameWorldState::GetInstance()->GetBaseCore();
			}
		}
		else if(m_targettype == ENTITY_CORE)
		{
			target = CGameWorldState::GetInstance()->GetBaseCore();
		}
		
		localPos.y = 0.0f;
		tempPos = target->GetFrame().GetWorldPosition();
		
		r = localPos - tempPos;
		D3DXVec3Normalize(&u, &r);
		d = D3DXVec3LengthSq(&r);
		U = (-1000.0f/(d));
		acceleration += u*U;
		
		if(m_trex->GetCurrWaypoint())
		{
			tempPos = m_trex->GetCurrWaypoint()->m_position;
			tempPos.y = 0.0f;
			r = localPos - tempPos;
			D3DXVec3Normalize(&u, &r);
			d = D3DXVec3Length(&r);
			U = -(m_trex->GetCurrWaypoint()->m_attraction * m_trex->GetAttackRange())/(d);
			acceleration += u * U;
				
			if(d < m_trex->GetAttackRange())
			{
				if(m_trex->GetCurrWaypoint()->m_stage == 0)
				{
					m_trex->SetCurrWaypoint(const_cast<TWaypoint*>(&CWaveManager::GetInstance()->GetAllStage3Waypoints().at(2)));
				}
				else
				{
					m_trex->SetCurrWaypoint(NULL);
				}
			}
		}
	}
	else
	{
		bool flag = false;

		if(m_trex->GetCurrWaypoint())
		{
			tempPos = m_trex->GetCurrWaypoint()->m_position;
			localPos.y = tempPos.y = 0.0f;
		
			r = localPos - tempPos;
			D3DXVec3Normalize(&u, &r);
			d = D3DXVec3LengthSq(&r);
			U = -(1000.0f/(d));
			acceleration += u*U;

			if(d < m_trex->GetAttackRange() * m_trex->GetAttackRange())
			{
				float closestDistance = FLT_MAX;

				if(m_trex->GetCurrWaypoint()->m_stage == 3)
				{
					distance = D3DXVec3LengthSq(&(localPos - CWaveManager::GetInstance()->m_launchWaypoints[0].m_position));

					if(distance < closestDistance)
					{
						closestDistance = distance;
						m_trex->SetCurrWaypoint(&CWaveManager::GetInstance()->m_launchWaypoints[0]);
					}

					distance = D3DXVec3LengthSq(&(localPos - CWaveManager::GetInstance()->m_launchWaypoints[4].m_position));

					if(distance < closestDistance)
					{
						closestDistance = distance;
						m_trex->SetCurrWaypoint(&CWaveManager::GetInstance()->m_launchWaypoints[4]);
					}
				}
				else
				{
					flag = true;
				}
			}
		}
		else
		{
			float closestDistance = FLT_MAX;
			distance = D3DXVec3LengthSq(&(localPos - CWaveManager::GetInstance()->m_launchWaypoints[0].m_position));

			if(distance < closestDistance)
			{
				closestDistance = distance;
				m_trex->SetCurrWaypoint(&CWaveManager::GetInstance()->m_launchWaypoints[0]);
			}

			distance = D3DXVec3LengthSq(&(localPos - CWaveManager::GetInstance()->m_launchWaypoints[4].m_position));

			if(distance < closestDistance)
			{
				closestDistance = distance;
				m_trex->SetCurrWaypoint(&CWaveManager::GetInstance()->m_launchWaypoints[4]);
			}
		}

		if(flag || D3DXVec3LengthSq(&(localPos - CGameWorldState::GetInstance()->GetBaseCore()->GetFrame().GetWorldPosition())) > 4000000.0f)
		{
			m_leaving = false;
			++m_phase;
			m_timeractive = true;
			m_timer = 5.0f;
			CUpgradeState::GetInstance()->SetTopHats(CUpgradeState::GetInstance()->GetTopHats() + m_trex->GetResourceValue());
			CHighScoreState::GetInstance()->GetCurrPlayer()->_totalHats += m_trex->GetResourceValue();
			CUpgradeState::GetInstance()->SetScore(CUpgradeState::GetInstance()->GetScore() + m_trex->GetPointValue());
			CWaveManager::GetInstance()->SetTrexInterrupt(false);
		}
	}

	if(target)
	{
		float distance = D3DXVec3LengthSq(&(localPos - target->GetFrame().GetWorldPosition()));
		m_trex->SetHasTarget(true);
		if(distance < m_trex->GetAttackRange()*m_trex->GetAttackRange())
		{
			m_trex->SetIsAttacking(true);
			m_trex->AddToAttackList(target);
		}
	}

	if(m_trex->GetIsAttacking())
	{
		m_trex->SetSpeed(0.1f);
		m_trex->SetAccelerationMode(ADino::NO_ACCELERATION);
		m_trex->SetAccelerationTimer(0.0f);
	}
	else if(wasattacking && !m_trex->GetIsAttacking())
	{
		m_trex->SetAccelerationMode(ADino::POSITIVE_ACCELERATION);
		m_trex->SetAccelerationTimer(0.0f);
	}
	else if(!target)
	{
		m_trex->SetAccelerationMode(ADino::NEGATIVE_ACCELERATION);
		m_trex->SetAccelerationTimer(0.0f);
	}

	if(m_trex->GetAccelerationMode() != ADino::NO_ACCELERATION)
	{
		m_trex->SetAccelerationTimer(m_trex->GetAccelerationTimer() + _dt);
		if(m_trex->GetAccelerationTimer() > m_trex->GetMaxAcceleration()*2.0f)
		{
			m_trex->SetAccelerationTimer(0.0f);
		}
	}

	float x = m_trex->GetAccelerationTimer();
	float c = m_trex->GetMaxAcceleration();
	float a = -((x-c)*(x-c))/c +c;
	float tempspeed = m_trex->GetSpeed();

	if(m_trex->GetAccelerationMode() == ADino::POSITIVE_ACCELERATION)
	{
		tempspeed += a;

		if(m_trex->GetHasTarget() && tempspeed > m_trex->GetMaxSpeed())
		{
			tempspeed = m_trex->GetMaxSpeed();
			m_trex->SetAccelerationMode(ADino::NO_ACCELERATION);
			m_trex->SetAccelerationTimer(0.0f);
		}
		else if(!m_trex->GetHasTarget() && tempspeed > m_trex->GetRoamingSpeed())
		{
			tempspeed = m_trex->GetRoamingSpeed();
			m_trex->SetAccelerationMode(ADino::NO_ACCELERATION);
			m_trex->SetAccelerationTimer(0.0f);
		}
	}
	else if(m_trex->GetAccelerationMode() == ADino::NEGATIVE_ACCELERATION)
	{
		tempspeed -= a;

		if(!m_trex->GetHasTarget() && tempspeed < m_trex->GetRoamingSpeed())
		{
			tempspeed = m_trex->GetRoamingSpeed();
			m_trex->SetAccelerationMode(ADino::NO_ACCELERATION);
			m_trex->SetAccelerationTimer(0.0f);
		}
		else if(tempspeed < 0.0f)
		{
			tempspeed = 0.0f;
			m_trex->SetAccelerationMode(ADino::POSITIVE_ACCELERATION);
			m_trex->SetAccelerationTimer(0.0f);
		}
	}

	D3DXVec3Normalize(&acceleration, &acceleration);
	acceleration *= tempspeed;

	velocity += m_trex->GetVelocity();
	D3DXVec3Normalize(&velocity, &velocity);
	velocity += acceleration;
	D3DXVec3Normalize(&velocity, &velocity);

	float angle, ratio;
	angle = D3DXVec3Dot(&m_trex->GetFrame().GetWorldZAxis(), &velocity);
	if(angle < cos(D3DXToRadian(15.0f)))
	{
		ratio = 15.0f/D3DXToDegree(acos(angle));
		D3DXVec3Lerp(&velocity, &m_trex->GetFrame().GetWorldZAxis(), &velocity, ratio);
		D3DXVec3Normalize(&velocity, &velocity);
	}

	velocity *= tempspeed;

	m_trex->SetAcceleration(acceleration);
	m_trex->SetSpeed(tempspeed);
	m_trex->SetVelocity(velocity);
}

void CTRexAI::Launch()
{
	D3DXVECTOR3 launchPos;
	CBaseCore* core = CGameWorldState::GetInstance()->GetBaseCore();
	launchPos = CWaveManager::GetInstance()->GetLaunchSiteData(2)->m_position;

	D3DXMATRIX newmat, tempmat;
	D3DXVECTOR3 xaxis, yaxis;
	D3DXVECTOR3 pos = launchPos;
	D3DXVECTOR3 velocity = core->GetFrame().GetWorldPosition() - launchPos;
	D3DXVec3Normalize(&velocity, &velocity);
	yaxis = D3DXVECTOR3(0.0f, 1.0f, 0.0f);
	D3DXVec3Cross(&xaxis, &velocity, &yaxis);
	D3DXVec3Normalize(&xaxis, &xaxis);
	D3DXVec3Cross(&yaxis, &xaxis, &velocity);
	D3DXVec3Normalize(&yaxis, &yaxis);
	pos -= velocity * 200.0f;
	newmat._11 = xaxis.x; newmat._12 = xaxis.y; newmat._13 = xaxis.z; newmat._14 = 0.0f;
	newmat._21 = yaxis.x; newmat._22 = yaxis.y; newmat._23 = yaxis.z; newmat._24 = 0.0f;
	newmat._31 = velocity.x; newmat._32 = velocity.y; newmat._33 = velocity.z; newmat._34 = 0.0f;
	newmat._41 = pos.x; newmat._42 = pos.y; newmat._43 = pos.z; newmat._44 = 1.0f;

	m_trex->SetSpeed(0.0f);
	m_trex->GetFrame().GetWorldMatrix()=newmat;
	m_trex->SetVelocity(velocity * m_trex->GetRoamingSpeed());
	m_trex->SetAcceleration(D3DXVECTOR3(0.0f, 0.0f, 0.0f));
	m_trex->SetAccelerationMode(ADino::POSITIVE_ACCELERATION);
	m_trex->SetAccelerationTimer(0.0f);
	SetTargetType(ENTITY_CORE);
	m_leaving = false;

	m_trex->SetMaxWhiskerAngle(45.0f);
	m_trex->SetCurrWhiskerAngle(0.0f);
	m_trex->SetWhiskerActive(true);
	m_trex->SetCurrWaypoint(&CWaveManager::GetInstance()->m_launchWaypoints[2]);

	CAudioManager::GetInstance()->PlayBossFight();
}