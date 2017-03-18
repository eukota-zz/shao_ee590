#include <iostream>
#include "Stopwatch.h"
#include "profileapi.h"



Stopwatch::Stopwatch()
{
}

LARGE_INTEGER Stopwatch::GetFrequency()
{
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	return freq;
}

float Stopwatch::GetTimeDiffInMS(LARGE_INTEGER start, LARGE_INTEGER stop)
{
	return 1000.0f*(float)(stop.QuadPart - start.QuadPart) / (float)GetFrequency().QuadPart;
}


void Stopwatch::Start()
{
	QueryPerformanceCounter(&StartTime);
	LapTime = StartTime;
	std::cout << "Started Timer" << std::endl;
}

void Stopwatch::Lap(const std::string& annotation)
{
	LARGE_INTEGER prevLapTime = LapTime;
	QueryPerformanceCounter(&LapTime);
	std::cout << "Lap Time: " << annotation.c_str() << ": " << GetTimeDiffInMS(prevLapTime, LapTime) << " ms" << std::endl;
}

void Stopwatch::Stop(const std::string& annotation)
{
	QueryPerformanceCounter(&StopTime);
	std::cout << "Total Time: " << annotation.c_str() << GetTimeDiffInMS(StartTime, StopTime) << " ms" << std::endl;
}
