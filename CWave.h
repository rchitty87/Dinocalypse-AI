#ifndef _CWAVE_H_
#define _CWAVE_H_

#include <vector>
#include "CFlock.h"

class CWave
{
private:
	std::vector<CFlock*> m_flocks;
	unsigned int m_waveindex;
	bool m_dead;
	bool m_launched;

	CWave(const CWave&);

public:
	CWave(unsigned int _index);	
	~CWave();

	std::vector<CFlock*>* GetFlocks();

	void AddFlock(CFlock* _flock)
	{
		m_flocks.push_back(_flock);
	}
	
	void Launch();
	void Update(float _dt);

	//Accessors
	unsigned int GetWaveIndex() { return m_waveindex; }
	bool IsActive() { return (!m_dead && m_launched); }
	bool HasLaunched() { return m_launched; }

	void Shutdown()
	{
		if( m_flocks.size() )
		{
			for( unsigned int i = 0; i < m_flocks.size(); ++i )
			{
				m_flocks[i]->Shutdown();
				delete m_flocks[i];
			}

			m_flocks.clear();
		}
		
	}
};

#endif