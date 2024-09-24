#ifndef TIMER_HPP
#define TIMER_HPP

#include <chrono>
#include <thread>
#include <string>


class Timer;

void sleep(int milliseconds);

void waitForFPS(Timer& timer, int maxFPS);

/**
*   Class for getting delta time and counting. Useful for getting frame's delta time and counting frames.
*       <ul>
*           <li>Delta time: Delta time between the last 2 consecutive calls to updateTime.</li>
*           <li>Total delta time: Delta time between startTime and the last call to updateTime.</li>
*       </ul>
*/
class Timer
{
    std::chrono::high_resolution_clock::time_point startTime;   //!< Time at startTimer call
    std::chrono::high_resolution_clock::time_point currentTime;   //!< Time at last updateTime call
    std::chrono::high_resolution_clock::time_point prevTime;   //!< Time at second last updateTime call
    long double deltaTime;   //!< Delta time between two consecutive calls to updateTime.
    long double totalDeltaTime;   //!< Delta time between startTimer and updateTime.

public:
    Timer();   //!< Starts chronometer (calls startTimer())

    void startTimer();   //!< Restart chronometer (startTime)
    void updateTime();   //!< Update time parameters with respect to currentTime.
    void reUpdateTime();   //!< Re-update time parameters as if updateTime was not called before.

    long double getDeltaTime() const;
    long double getTotalDeltaTime() const;
    std::string getDate();   //!< Get a string with current date and time (example: Mon Jan 31 02:28:35 2022)
};

#endif