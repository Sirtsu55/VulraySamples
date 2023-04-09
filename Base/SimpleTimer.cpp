#include "SimpleTimer.h"


auto ProgramStartTime = std::chrono::high_resolution_clock::now();

SimpleTimer::SimpleTimer()
{
}

SimpleTimer::~SimpleTimer()
{

}

void SimpleTimer::Start()
{
	StartTime = std::chrono::high_resolution_clock::now();
}

// Gives Time in Seconds
double SimpleTimer::Endd()
{
	EndTime = std::chrono::high_resolution_clock::now();

	std::chrono::duration<double> Duration = (EndTime - StartTime);
	return Duration.count();

}
float SimpleTimer::Endf()
{
	EndTime = std::chrono::high_resolution_clock::now();

	std::chrono::duration<float> Duration = (EndTime - StartTime);
	return Duration.count();

}
/// Gives Time in Specified accuracy

double SimpleTimer::Endd(TimerAccuracy acc)
{

	EndTime = std::chrono::high_resolution_clock::now();

	switch (acc)
	{
	case TimerAccuracy::MilliSec:
		{
			std::chrono::duration<double, std::milli> Duration = EndTime - StartTime;
			return Duration.count();
			break;
		}
		case TimerAccuracy::MicroSec:
		{
			std::chrono::duration<double, std::micro> Duration = EndTime - StartTime;
			return Duration.count();
			break;
		}
		case TimerAccuracy::NanoSec:
		{
			std::chrono::duration<double, std::nano> Duration = EndTime - StartTime;
			return Duration.count();
			break;
		}
		case TimerAccuracy::Seconds:
		{
			std::chrono::duration<double> Duration = EndTime - StartTime;
			return Duration.count();
			break;
		}

	}
	return -1;
}
float SimpleTimer::Endf(TimerAccuracy acc)
{

	EndTime = std::chrono::high_resolution_clock::now();

	switch (acc)
	{
	case TimerAccuracy::MilliSec:
		{
			std::chrono::duration<float, std::milli> Duration = EndTime - StartTime;
			return Duration.count();
			break;
		}
		case TimerAccuracy::MicroSec:
		{
			std::chrono::duration<float, std::micro> Duration = EndTime - StartTime;
			return Duration.count();
			break;
		}
		case TimerAccuracy::NanoSec:
		{
			std::chrono::duration<float, std::nano> Duration = EndTime - StartTime;
			return Duration.count();
			break;
		}
		case TimerAccuracy::Seconds:
		{
			std::chrono::duration<float> Duration = EndTime - StartTime;
			return Duration.count();
			break;
		}

	}
	return -1;
}




double GetElapsedSeconds()
{
	std::chrono::duration<double, std::ratio<1, 1>> Duration = std::chrono::high_resolution_clock::now() - ProgramStartTime;

	return Duration.count();
}

