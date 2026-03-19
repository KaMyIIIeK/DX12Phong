#include "game_timer.h"

GameTimer::GameTimer() : m_delta_time_(-1.0f), m_seconds_per_count_(0.0f), m_base_time_(0.0f),
m_paused_time_(0.0f), m_stop_time_(0.0f), m_prev_time_(0.0f), m_curr_time_(0.0f),
m_stopped_(false)
{
	__int64 countsPerSec;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
	m_seconds_per_count_ = 1.0f / (double)countsPerSec;
}

void GameTimer::Tick() {
	if (m_stopped_) {
		m_delta_time_ = 0.0;
		return;
	}
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	m_curr_time_ = currTime;
	m_delta_time_ = (m_curr_time_ - m_prev_time_) * m_seconds_per_count_;
	m_prev_time_ = m_curr_time_;

	if (m_delta_time_ < 0.0) {
		m_delta_time_ = 0.0;
	}

	if (m_delta_time_ > 0.2) {
		m_delta_time_ = 0.2;
	}
}

float GameTimer::DeltaTime() const {
	return (float)m_delta_time_;
}

void GameTimer::Stop() {
	if (!m_stopped_) {
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
		m_stop_time_ = currTime;
		m_stopped_ = true;
	}
}

void GameTimer::Start() {
	__int64 startTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&startTime);
	if (m_stopped_) {
		m_paused_time_ += (startTime - m_stop_time_);
		m_prev_time_ = startTime;
		m_stop_time_ = 0;
		m_stopped_ = false;
	}
}

float GameTimer::TotalTime() const {
	if (m_stopped_) {
		return (float)(((m_stop_time_ - m_paused_time_) - m_base_time_) * m_seconds_per_count_);
	}
	else {
		return (float)(((m_curr_time_ - m_paused_time_) - m_base_time_) * m_seconds_per_count_);
	}
}

void GameTimer::Reset() {
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	m_base_time_ = currTime;
	m_prev_time_ = currTime;
	m_stop_time_ = 0;
	m_stopped_ = false;
}


