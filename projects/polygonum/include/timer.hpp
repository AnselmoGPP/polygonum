#ifndef TIMER_HPP
#define TIMER_HPP

#include <chrono>
#include <thread>
#include <string>


void sleep(int milliseconds);

/**
*   Object used for getting the time between events.Two ways :
*       <ul>
*           <li>get_delta_time(): Get the time increment (deltaTime) between two consecutive calls to this function.</li>
*           <li>start() & end(): Get the time increment (deltaTime) between a call to start() and end(). <<<<<<<<<<<<<<<<<<<<<<</li>
*       </ul>
*/    
class timer
{
    // Get date and time (all in numbers)

    // Get date and time (months and week days are strings)
};

// Class used in the render loop (OpenGL, Vulkan, etc.) for different time-related purposes (frame counting, delta time, current time, fps...)
class TimerSet
{
    std::chrono::high_resolution_clock::time_point startTime;       ///< From origin of time
    std::chrono::high_resolution_clock::time_point prevTime;        ///< From origin of time
    std::chrono::high_resolution_clock::time_point currentTime;     ///< From origin of time

    long double deltaTime;              ///< From prevTime
    long double time;                   ///< From startTime() call

    int FPS;                            ///< FPS computed in computeDeltaTime()
    int maxFPS;                         ///< Maximum allowed FPS. A sleep is activated in computeDeltaTime() if FPS is too high.
    int maxPossibleFPS;                 ///< Maximum possible FPS you can get without setting a maxFPS (i.e. if maxFPS were equal to 0)

    size_t frameCounter;                ///< Number of calls made to computeDeltaTime()

public:
    TimerSet(int maxFPS = 30);               ///< Constructor. maxFPS sets a maximum FPS (0 for setting no maximum FPS)

    // Chrono methods
    void        startTimer();           ///< Start time counting for the chronometer (startTime)
    void        computeDeltaTime();     ///< Compute frame's duration (time between two calls to this) and other time values
    long double getDeltaTime();         ///< Returns time (seconds) increment between frames (deltaTime)
    long double getTime();              ///< Returns time (seconds) since startTime when computeDeltaTime() was called

    // FPS control
    int         getFPS();               ///< Get FPS (updated in computeDeltaTime())
    void        setMaxFPS(int newFPS);  ///< Modify the current maximum FPS. Set it to 0 to deactivate FPS control.
    int         getMaxPossibleFPS();    ///< Get the maximum possible FPS you can get (if we haven't set max FPS, maxPossibleFPS == FPS)

    // Frame counting
    size_t      getFrameCounter();      ///< Get number of calls made to computeDeltaTime()

    // Bonus methods (less used)
    long double getTimeNow();           ///< Returns time (seconds) since startTime, at the moment of calling GetTimeNow()
    std::string getDate();              ///< Get a string with date and time (example: Mon Jan 31 02:28:35 2022)
};

#endif