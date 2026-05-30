
/* Implement audio passthrough on a single thread */

#include "smbPitchShift.h"
#include "portaudio.h"
#include <iostream>
#include <vector>

using namespace std;

/*-- Audio parameters --*/
constexpr int kSampleRate      = 44100; // Sample rate in Hz
constexpr int kFramesPerBuffer = 512;   // Number of frames per buffer
constexpr int kNumChannels     = 1;     // Mono channel (Justine recommends using this)
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
    PaStream* stream; // stream for audio

    PaError err = Pa_Initialize();
    if (!checkErr(err, "Pa_Initialize")) return 1; //  Exit on error

    // Set up input stream parameters
    PaStreamParameters inputParams;
    inputParams.device = Pa_GetDefaultInputDevice();
    if (inputParams.device == paNoDevice) {
        std::cerr << "No default input device found." << std::endl;
        Pa_Terminate();
        return 1;
    }
    inputParams.channelCount              = kNumChannels;
    inputParams.sampleFormat              = kSampleFormat;
    inputParams.suggestedLatency          =
        Pa_GetDeviceInfo(inputParams.device)->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = nullptr;

    // Set up output stream parameters
    PaStreamParameters outputParams;
    outputParams.device = Pa_GetDefaultOutputDevice();
    if (outputParams.device == paNoDevice) {
        std::cerr << "No default output device found." << std::endl;
        Pa_Terminate();
        return 1;
    }
    outputParams.channelCount              = kNumChannels;
    outputParams.sampleFormat              = kSampleFormat;
    outputParams.suggestedLatency          =
        Pa_GetDeviceInfo(outputParams.device)->defaultHighOutputLatency;
    outputParams.hostApiSpecificStreamInfo = nullptr;

    // Open the audio stream
    err = Pa_OpenStream( 
        &stream, 
        &inputParams,       // Changed these to use the input & output parameters
        &outputParams, 
        kSampleRate, 
        kFramesPerBuffer, 
        paNoFlag, 
        nullptr, 
        nullptr
    );

    // Check if the stream opened successfully
    if (!checkErr(err, "Pa_OpenStream")) return 1; //  Exit on error(err);

    // start the stream
    err = Pa_StartStream(stream);
    if (!checkErr(err, "Pa_StartStream")) return 1; //  Exit on error(err);

    // reading in values from the stream and passing them through
    float input_buffer[kFramesPerBuffer];
    float output_buffer[kFramesPerBuffer];

    cout << "Beginning the audio process..." << endl;

    // perform a passthrough for 10 seconds
    for (int i = 0; i < ((kNumSeconds*kSampleRate)/kFramesPerBuffer); i++) {

        // read 1 block of the stream
        err = Pa_ReadStream( stream, input_buffer, kFramesPerBuffer);
        if (!checkErr(err, "Pa_ReadStream")) return 1; //  Exit on error(err);

        // write that block to the output buffer
        for (int j = 0; j < kFramesPerBuffer; j++) {
            output_buffer[j] = input_buffer[j];
        }

        // Pitch shift the output buffer by a factor of 2 (just to test the functionality of the pitch shifting code)
        smbPitchShift(2.0, kFramesPerBuffer, 1024, 4, kSampleRate, output_buffer, output_buffer);

        // write the output buffer to the output stream
        err = Pa_WriteStream( stream, output_buffer, kFramesPerBuffer);

        if (!checkErr(err, "Pa_WriteStream")) return 1; //  Exit on error(err);
    }

    // close off everything at the end of the process
    cout << "The program has ended" << endl;

    err = Pa_StopStream(stream);
    if (!checkErr(err, "Pa_StopStream")) return 1; //  Exit on error(err);

    err = Pa_CloseStream(stream);
    if (!checkErr(err, "Pa_CloseStream")) return 1; //  Exit on error(err);

    Pa_Terminate();
}