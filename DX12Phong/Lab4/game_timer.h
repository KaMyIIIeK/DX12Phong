#ifndef GAME_TIMER_
#define GAME_TIMER_
#include <Windows.h>

class GameTimer {
public:
	GameTimer();

	float TotalTime() const;
	float DeltaTime() const;

	void Reset();
	void Start();
	void Stop();
	void Tick();
private:
	double m_seconds_per_count_;
	double m_delta_time_;

	__int64 m_base_time_;
	__int64 m_paused_time_;
	__int64 m_stop_time_;
	__int64 m_prev_time_;
	__int64 m_curr_time_;

	bool m_stopped_;
};

#endif //GAME_TIMER_