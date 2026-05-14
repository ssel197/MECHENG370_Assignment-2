// Task 1: Getting PortAudios set up and working on the device

#include "portaudio.h"
#include "smbPitchShift.h"

#include <iostream>

using namespace std;

const int SAMPLING_FREQ = 44100;
const int BUFFER_SIZE = 512;
const int INPUT_CHANNEL_NO = 1;
const int OUTPUT_CHANNEL_NO = 1;
const PaSampleFormat SAMPLE_FORMAT = paFloat32;
void printDeviceInfo(const PaDeviceInfo* deviceInfo, const char* deviceType) {
  std::cout << "Default " << deviceType << " Device: " << deviceInfo->name
            << std::endl;
  std::cout << " Input Channels: " << deviceInfo->maxInputChannels << std::endl;
  std::cout << " Output Channels: " << deviceInfo->maxOutputChannels
            << std::endl;
  std::cout << " Default Sample Rate: " << deviceInfo->defaultSampleRate
            << std::endl;
  // You can print more information about the device if needed
  std::cout << std::endl;
}
static void checkErr(PaError err) {
  if (err != paNoError) {
    printf("PortAudio error: %s\n", Pa_GetErrorText(err));
    exit(EXIT_FAILURE);
  }
}

int main() {
  PaError err;
  // initialising portAudio, checking input and output audio devices
  err = Pa_Initialize();
  checkErr(err);
  int defaultInputDevice = Pa_GetDefaultInputDevice();
  int defaultOutputDevice = Pa_GetDefaultOutputDevice();
  if (defaultInputDevice == paNoDevice || defaultOutputDevice == paNoDevice) {
    std::cerr << "No default input or output device found." << std::endl;
    Pa_Terminate();
    return -1;
  }
  std::cout << "Available audio devices:" << std::endl;
  for (int i = 0; i < Pa_GetDeviceCount(); ++i) {
    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
    if (i == defaultInputDevice) {
      printDeviceInfo(deviceInfo, "Input");
    }
    if (i == defaultOutputDevice) {
      printDeviceInfo(deviceInfo, "Output");
    }
  }
}
