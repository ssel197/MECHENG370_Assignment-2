
/* Implement audio passthrough on a single thread */

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
    PaStream* inputStream; // stream for reading audio
    PaStream* outputStream; // stream for writing audio

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

    // Open input-only stream
    err = Pa_OpenStream(
        &inputStream,
        &inputParams,
        nullptr,        // no output
        kSampleRate,
        kFramesPerBuffer,
        paNoFlag,
        nullptr,
        nullptr
    );
    if (!checkErr(err, "Pa_OpenStream (input)")) { Pa_Terminate(); return 1; }

    // Open output-only stream
    err = Pa_OpenStream(
        &outputStream,
        nullptr,        // no input
        &outputParams,
        kSampleRate,
        kFramesPerBuffer,
        paNoFlag,
        nullptr,
        nullptr
    );
    if (!checkErr(err, "Pa_OpenStream (output)")) { Pa_Terminate(); return 1; }

    err = Pa_StartStream(inputStream);
    if (!checkErr(err, "Pa_StartStream (input)")) { Pa_Terminate(); return 1; }

    err = Pa_StartStream(outputStream);
    if (!checkErr(err, "Pa_StartStream (output)")) { Pa_Terminate(); return 1; }

    // reading in values from the stream and passing them through
    float input_buffer[kFramesPerBuffer];

    cout << "Beginning the audio process..." << endl;

    // perform a passthrough for 10 seconds
    for (int i = 0; i < ((kNumSeconds*kSampleRate)/kFramesPerBuffer); i++) {

        // read 1 block of the stream
        err = Pa_ReadStream( inputStream, input_buffer, kFramesPerBuffer);
        if (!checkErr(err, "Pa_ReadStream")) return 1; //  Exit on error(err);

        // write that block to the output buffer immediately

        err = Pa_WriteStream( outputStream, input_buffer, kFramesPerBuffer);

        if (!checkErr(err, "Pa_WriteStream")) return 1; //  Exit on error(err);
    }

    // close off everything at the end of the process
    cout << "The program has ended" << endl;

    err = Pa_StopStream(outputStream);
    if (!checkErr(err, "Pa_StopStream")) return 1; //  Exit on error(err);

    err = Pa_StopStream(inputStream);
    if (!checkErr(err, "Pa_StopStream")) return 1; //  Exit on error(err);

    err = Pa_CloseStream(outputStream);
    if (!checkErr(err, "Pa_CloseStream")) return 1; //  Exit on error(err);

    err = Pa_CloseStream(inputStream);
    if (!checkErr(err, "Pa_CloseStream")) return 1; //  Exit on error(err);

    Pa_Terminate();
    return 0;
}