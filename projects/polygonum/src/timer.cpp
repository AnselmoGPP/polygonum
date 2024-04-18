
#include "timer.hpp"

#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>


void sleep(int milliseconds) { std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); }


TimerSet::TimerSet(int maximumFPS)
    : currentTime(std::chrono::system_clock::duration::zero()), maxFPS(maximumFPS), time(0), deltaTime(0), FPS(0), frameCounter(0)
{
    startTimer();
}

void TimerSet::startTimer()
{
    startTime = std::chrono::high_resolution_clock::now();
    prevTime = startTime;
    //std::this_thread::sleep_for(std::chrono::microseconds(1000));   // Avoids deltaTime == 0 (i.e. currentTime == lastTime)
}

void TimerSet::computeDeltaTime()
{
    // Get deltaTime
    currentTime = std::chrono::high_resolution_clock::now();
    deltaTime = std::chrono::duration<long double, std::chrono::seconds::period>(currentTime - prevTime).count();
    //   time = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - startTime).count() / 1000000.l;
    //   time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    maxPossibleFPS = std::round(1 / deltaTime);

    // Add some time to deltaTime to adjust the FPS (if FPS control is enabled)
    if (maxFPS > 0)
    {
        int waitTime = (1.l / maxFPS - deltaTime) * 1000000;    // microseconds (for the sleep)
        if (waitTime > 0)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(waitTime));
            currentTime = std::chrono::high_resolution_clock::now();
            deltaTime = std::chrono::duration<long double, std::chrono::seconds::period>(currentTime - prevTime).count();
        }
    }

    prevTime = currentTime;

    // Get FPS
    FPS = std::round(1 / deltaTime);

    // Get time
    time = std::chrono::duration<long double, std::chrono::seconds::period>(currentTime - startTime).count();

    // Increment the frame count
    ++frameCounter;
}

long double TimerSet::getDeltaTime() { return deltaTime; }

long double TimerSet::getTime() { return time; }

long double TimerSet::getTimeNow()
{
    std::chrono::high_resolution_clock::time_point timeNow = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<long double, std::chrono::seconds::period>(timeNow - startTime).count();
}

std::string TimerSet::getDate()
{
    std::chrono::system_clock::time_point timePoint = std::chrono::system_clock::now();
    std::time_t date = std::chrono::system_clock::to_time_t(timePoint);
    return std::ctime(&date);
}

int TimerSet::getFPS() { return FPS; }

int TimerSet::getMaxPossibleFPS() { return maxPossibleFPS; }

void TimerSet::setMaxFPS(int newFPS) { maxFPS = newFPS; }

size_t TimerSet::getFrameCounter() { return frameCounter; };