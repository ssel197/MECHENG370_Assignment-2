
/* Implement audio passthrough on multiple threads */

#include "smbPitchShift.h"
#include "portaudio.h"
#include <iostream>
#include <string>
#include <thread>
#include <atomic>

using namespace std;

/*-- Audio parameters --*/
constexpr int kSampleRate      = 44100; // Sample rate in Hz
constexpr int kFramesPerBuffer = 512;   // Number of frames per buffer
constexpr int kNumChannels     = 1;     // Mono channel (Justine recommends using this)
constexpr PaSampleFormat kSampleFormat = paFloat32; // 32-bit floating point]

/*
 * Lock-free ring buffer (single-producer, single-consumer).
 *
 * The reader thread writes chunks in; the processor thread reads them out.
 * Because only one thread writes and only one reads, atomic indices are
 * enough to keep accesses safe — no mutex needed, so neither thread ever
 * blocks the other.
 *
 * If the processor falls behind and the buffer fills up, the reader drops
 * the incoming frame rather than waiting. This is acceptable for real-time
 * audio: a brief gap is better than the microphone falling behind.
 */
constexpr int NUM_CHUNKS = 8;

struct RingBuffer {
    float data[NUM_CHUNKS][kFramesPerBuffer];
    std::atomic<int> write_pos{0};
    std::atomic<int> read_pos{0};

    // Called by reader thread. Returns false and drops the frame if full.
    bool write(const float* src) {
        int w      = write_pos.load(std::memory_order_relaxed);
        int next_w = (w + 1) % NUM_CHUNKS;
        if (next_w == read_pos.load(std::memory_order_acquire))
            return false; // full — drop frame rather than block
        std::copy(src, src + kFramesPerBuffer, data[w]);
        write_pos.store(next_w, std::memory_order_release);
        return true;
    }

    // Called by processor thread. Returns false if empty.
    bool read(float* dst) {
        int r = read_pos.load(std::memory_order_relaxed);
        if (r == write_pos.load(std::memory_order_acquire))
            return false; // empty
        std::copy(data[r], data[r] + kFramesPerBuffer, dst);
        read_pos.store((r + 1) % NUM_CHUNKS, std::memory_order_release);
        return true;
    }
};

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
    // Declare separate input and output streams for simultaneous reading and writing
    PaStream* inputStream;
    PaStream* outputStream;

    PaError err = Pa_Initialize();
    if (!checkErr(err, "Pa_Initialize")) return 1;

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

    RingBuffer ring;
    std::atomic<bool> done(false);
    std::atomic<float> pitchFactor{2.0f}; // Pitch shift factor (2.0 = one octave up)

    // State: 0 = idle, 1 = pitch shift, 2 = passthrough
    std::atomic<int> state{0};


    /*
     * Thread 1 — reader
     * Continuously reads raw audio from the microphone and pushes each chunk
     * into the ring buffer. Pa_ReadStream blocks until the audio driver has
     * kFramesPerBuffer samples ready, naturally pacing this thread to the
     * hardware rate. It never waits on the processor thread, so it will not
     * miss incoming audio samples even if pitch shifting takes variable time.
     */
    std::thread reader([&]() {
        float buf[kFramesPerBuffer];
        while (!done) {
            PaError read_err = Pa_ReadStream(inputStream, buf, kFramesPerBuffer);
            if (read_err != paNoError && read_err != paInputOverflowed) {
                checkErr(read_err, "Pa_ReadStream");
                done = true;
                return;
            }
            ring.write(buf); // drops frame silently if ring buffer is full
        }
    });

    cout << "Beginning the audio process..." << endl;

    /*
     * Thread 2 — processor
     * Pulls chunks from the ring buffer, pitch shifts them, and writes the
     * result to the output stream. Pa_WriteStream paces this thread to the
     * same hardware rate as the reader, so the ring buffer stays shallow
     * under normal conditions. When finished, sets done = true to signal
     * the reader thread to stop.
     */
    std::thread processor([&]() {
        float process_buf[kFramesPerBuffer];
        while (!done) {
            // Wait until the ring buffer has a chunk available
            while (!ring.read(process_buf) && !done) {}
            if (done) break;

            // Only pitch shift in pitch shift mode; passthrough sends audio unchanged
            if (state == 1) {
                smbPitchShift(pitchFactor.load(), kFramesPerBuffer, 1024, 4, kSampleRate, process_buf, process_buf);
            }

            PaError write_err = Pa_WriteStream(outputStream, process_buf, kFramesPerBuffer);
            if (write_err != paNoError && write_err != paOutputUnderflowed) {
                checkErr(write_err, "Pa_WriteStream");
                break;
            }
        }
        done = true;
    });

    /*
    * Thread 3 - Input thread for adjusting pitch shift factor
    * Quit (q): Allows the user to exit the program
    * Start Pitch Shifter (s): Enables the pitch shifter. Output audio that follows will be
    * altered in pitch.
    * Passthrough (p): Enables passthrough mode. Allows audio output without any alterations.
    * Up (u): Increasing the pitch shift factor by 0.5 units, disabled in Passthrough mode.
    * Down (d): Decreasing the pitch shift factor by 0.5 units, disabled in Passthrough mode
    */

    std::thread controller([&]() {
        
        string unsanitizedInput;
        char command;
        cout << "Commands: s = pitch shift, p = passthrough, u = pitch up, d = pitch down, q = quit" << endl;
        while (!done) {
            
            cin >> unsanitizedInput;
            if (unsanitizedInput.empty() || unsanitizedInput.length() > 1) {
                cout << "Invalid command" << endl;
                continue;
            }// ignore empty input or multiple characters

            command = unsanitizedInput[0]; // the command is the first character of the input
            int currentState = state; // read once for the switch

            switch (currentState) {
                case 0: // IDLE
                    if      (command == 's') { state = 1; cout << "Pitch shifting enabled. Factor: " << (float)pitchFactor << endl; }
                    else if (command == 'p') { state = 2; cout << "Passthrough enabled." << endl; }
                    else if (command == 'q') { done = true; cout << "Quitting..." << endl; }
                    break;

                case 1: // PITCH SHIFT
                    if (command == 'p') {
                        state = 2;
                        cout << "Passthrough enabled." << endl;
                    } else if (command == 'u') {
                        float f = pitchFactor;
                        if (f + 0.5f <= 2.0f) { pitchFactor = f + 0.5f; cout << "Pitch factor: " << (float)pitchFactor << endl; }
                        else                   { cout << "Already at maximum (2.0)." << endl; }
                    } else if (command == 'd') {
                        float f = pitchFactor;
                        if (f - 0.5f >= 0.5f) { pitchFactor = f - 0.5f; cout << "Pitch factor: " << (float)pitchFactor << endl; }
                        else                   { cout << "Already at minimum (0.5)." << endl; }
                    } else if (command == 'q') {
                        done = true;
                        cout << "Quitting..." << endl;
                    } else {
                        cout << "Invalid command in pitch shift mode." << endl;
                    }
                    break;

                case 2: // PASSTHROUGH
                    if (command == 's') {
                        state = 1;
                        cout << "Pitch shifting enabled. Factor: " << (float)pitchFactor << endl;
                    } else if (command == 'u' || command == 'd') {
                        cout << "u/d disabled in passthrough mode." << endl;
                    } else if (command == 'q') {
                        done = true;
                        cout << "Quitting..." << endl;
                    } else {
                        cout << "Invalid command in passthrough mode." << endl;
                    }
                    break;
            }
        }
    });

    reader.join();
    processor.join();
    controller.join();

    cout << "The program has ended" << endl;

    Pa_StopStream(inputStream);
    Pa_StopStream(outputStream);
    Pa_CloseStream(inputStream);
    Pa_CloseStream(outputStream);
    Pa_Terminate();
}
