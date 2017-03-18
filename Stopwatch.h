#pragma once
#include "Windows.h"
#include <vector>

class Stopwatch
{
	LARGE_INTEGER StartTime;
	LARGE_INTEGER StopTime;
	LARGE_INTEGER LapTime;

	LARGE_INTEGER GetFrequency();
	float GetTimeDiffInMS(LARGE_INTEGER start, LARGE_INTEGER stop);

public:
	Stopwatch();

	void Start();
	float Lap(const std::string& annotation);
	float Stop(const std::string& annotation);
};