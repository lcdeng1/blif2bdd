/*
 * \brief A timer class.
 *
 * Timer is started when a timer object is initialized.
 * note_time() is used to note the current time.
 * get_last_interval() is the time between the last two instances when the
 * time was noted (in microseconds)
 *
 * */

#ifndef TIMERS_H
#define TIMERS_H

#include "defines.h"
#include <sys/time.h>
#include <time.h>

class timer 
{
	struct timeval curr_time, prev_time;
	struct timezone time_zone;
	long last_interval; 

public:
	timer()
	{
		gettimeofday(&curr_time, &time_zone);
		prev_time = curr_time;
	}

	inline void note_time()
	{
		gettimeofday(&curr_time, &time_zone);
		last_interval = (curr_time.tv_sec - prev_time.tv_sec) * 1000000;
		last_interval += curr_time.tv_usec - prev_time.tv_usec;
		prev_time = curr_time;
	}

	inline long get_last_interval()
	{
		return last_interval;
	}
  inline double get_last_seconds()
  {
    return last_interval / 1000000.0;
  }
};

#endif