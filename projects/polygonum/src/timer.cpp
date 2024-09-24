
#include "timer.hpp"

#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>


void sleep(int milliseconds) { std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); }

// Add some time to deltaTime to adjust the FPS (if FPS control is enabled)
void waitForFPS(Timer& timer, int maxFPS)
{
    if (maxFPS == 0) return;
    
    int waitTime = (1.l / maxFPS - timer.getDeltaTime()) * 1000000;    // microseconds (for the sleep)
    if (waitTime > 0)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(waitTime));
        timer.reUpdateTime();
    }
}

Timer::Timer()
    : deltaTime(0), totalDeltaTime(0)
{
    startTimer();
    currentTime = startTime;
    //currentTime = std::chrono::system_clock::duration::zero();
}

void Timer::startTimer()
{
    startTime = std::chrono::high_resolution_clock::now();
    prevTime = startTime;
    //std::this_thread::sleep_for(std::chrono::microseconds(1000));   // Avoids deltaTime == 0 (i.e. currentTime == lastTime)
}

void Timer::updateTime()
{
    prevTime = currentTime;
    currentTime = std::chrono::high_resolution_clock::now();
    deltaTime = std::chrono::duration<long double, std::chrono::seconds::period>(currentTime - prevTime).count();
    totalDeltaTime = std::chrono::duration<long double, std::chrono::seconds::period>(currentTime - startTime).count();
    //   time = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - startTime).count() / 1000000.l;
    //   time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
}

void Timer::reUpdateTime()
{
    currentTime = std::chrono::high_resolution_clock::now();
    deltaTime = std::chrono::duration<long double, std::chrono::seconds::period>(currentTime - prevTime).count();
    totalDeltaTime = std::chrono::duration<long double, std::chrono::seconds::period>(currentTime - startTime).count();
}

long double Timer::getDeltaTime() const { return deltaTime; }

long double Timer::getTotalDeltaTime() const { return totalDeltaTime; }

std::string Timer::getDate()
{
    std::chrono::system_clock::time_point timePoint = std::chrono::system_clock::now();
    std::time_t date = std::chrono::system_clock::to_time_t(timePoint);
    return std::ctime(&date);
}
