#pragma once
#include <chrono>



enum class TimerAccuracy
{
	MilliSec,
	MicroSec,
	NanoSec,
	Seconds

};

class SimpleTimer
{
public:
	SimpleTimer();
	~SimpleTimer();


	void Start();
	double Endd(TimerAccuracy acc);
	double Endd();

	float Endf(TimerAccuracy acc);
	float Endf();


private:

	// msvc and gcc return different types for high_resolution_clock::now()
#ifdef _WIN32
	std::chrono::steady_clock::time_point StartTime;
	std::chrono::steady_clock::time_point EndTime;
#else
	std::chrono::system_clock::time_point StartTime;
	std::chrono::system_clock::time_point EndTime;
#endif

};
double GetElapsedSeconds();



