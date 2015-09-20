#include "StdAfx.h"
#include "CWave.h"
#include "CFlock.h"
#include "CWaveManager.h"
#include "..\Gameplay\ADino.h"
#include "..\managers\CObjectFactory.h"
#include "..\Managers\CHudManager.h"

CWave::CWave(unsigned int _index)
{
	m_waveindex = _index;
	m_dead = false;
	m_launched = false;
}

CWave::~CWave()
{
	
}

std::vector<CFlock*>* CWave::GetFlocks()
{
	return &m_flocks;
}

void CWave::Launch()
{
	m_launched = true;

	const TWaveData* waveData = CWaveManager::GetInstance()->GetLaunchData(m_waveindex);

	for(unsigned int i=0; i<m_flocks.size(); ++i)
	{
		m_flocks.at(i)->Launch(waveData);
	}
}

void CWave::Update(float _dt)
{
	std::vector<CFlock*>::iterator flockIter;

	for(flockIter = m_flocks.begin(); flockIter != m_flocks.end(); ++flockIter)
	{
		
		(*flockIter)->Update(_dt);
	}

	for(flockIter = m_flocks.begin(); flockIter != m_flocks.end();)
	{
		if(!(*flockIter)->IsActive())
		{
			delete (*flockIter);
			flockIter = m_flocks.erase(flockIter);
		}
		else
		{
			++flockIter;
		}
	}

	if(m_flocks.size()==0)
	{
		m_dead = true;
	}
}