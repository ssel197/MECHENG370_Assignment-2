
/* Implement audio passthrough on a single thread */

#include <portaudio.h>
#include <iostream>
#include <vector>

/*-- Audio parameters --*/
constexpr int kSampleRate      = 44100; // Sample rate in Hz
constexpr int kFramesPerBuffer = 512;   // Number of frames per buffer
constexpr int kNumChannels     = 2;     // Stereo
constexpr int kNumSeconds      = 10;    // Run for 10 seconds
constexpr PaSampleFormat kSampleFormat = paFloat32; // 32-bit floating point

/*-- Error checking function --*/
static bool checkErr(PaError err, const char* context) {
    if (err != paNoError) {
        std::cerr << "PortAudio error in " << context
                  << ": " << Pa_GetErrorText(err) << std::endl;
        return false;
    }
    return true;
}

int main() {
    // Initialize PortAudio
    PaError err = Pa_Initialize();
    if (!checkErr(err, "Pa_Initialize")) return 1; //  Exit on error

    // Set up input stream parameters
    PaStreamParameters inputParams;
    inputParams.device                    = Pa_GetDefaultInputDevice();
    inputParams.channelCount              = kNumChannels;
    inputParams.sampleFormat              = kSampleFormat;
    inputParams.suggestedLatency          =
        Pa_GetDeviceInfo(inputParams.device)->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = nullptr;
