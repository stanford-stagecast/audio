#ifndef CONSTANTS_HH
#define CONSTANTS_HH

constexpr int SAMPLE_RATE = 48000;
constexpr float MAX_FREQUENCY = SAMPLE_RATE / 2.0;
constexpr int NUM_CHANNELS = 1;
constexpr unsigned int MIN_LATENCY = 500000; /* microseconds */

#endif /* CONSTANTS_HH */
