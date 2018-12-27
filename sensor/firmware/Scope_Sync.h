#pragma once

#	include <avr/interrupt.h>

class Scope_Sync
{
public: 
	Scope_Sync()
	{
		m_old_sreg = SREG;
		cli();
	}
	~Scope_Sync()
	{
		SREG = m_old_sreg;
	}
private:
	uint8_t m_old_sreg;
};



