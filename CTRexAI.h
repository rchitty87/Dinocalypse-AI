#ifndef _CTREXAI_H_
#define _CTREXAI_H_

#include "../Gameplay/ADino.h"
#include "../Renderer/CEntity.h"

#define NUM_TREX_PHASES 3

class CTRex;
class CTRexAI
{
private:
	unsigned int			m_phase;
	CTRex*					m_trex;
	bool					m_leaving;
	bool					m_timeractive;
	float					m_timer;
	EntityType				m_targettype;
	float					m_targettimer;
	float					m_targetmaxtime;
	TWaypoint*				m_backupWp;

	static CTRexAI*			m_instance;

	CTRexAI();
	~CTRexAI();
	CTRexAI(const CTRexAI&);

public:
	static CTRexAI* GetInstance();

	void Enter(void);
	void Shutdown(void);
	void Update(float _dt);
	void Launch();

	void SetTargetType(EntityType _value) { m_targettype = _value; m_targettimer =0.0f;}
	void SetTargetTimer(float _value) { m_targettimer = _value;}
	
	unsigned int GetPhase(void) { return m_phase; }
	bool IsDone(void) { return m_phase >= NUM_TREX_PHASES; }
	bool IsLeaving(void) { return m_leaving; }
	CTRex* GetTRex(void) { return m_trex; } 
	EntityType GetTarget(void) { return m_targettype; }
	float GetTargetTimer(void) { return m_targettimer; }
	float GetTargetMaxTime(void) { return m_targetmaxtime; }
};

#endif