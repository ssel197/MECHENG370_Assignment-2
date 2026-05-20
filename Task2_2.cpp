
/* Implement audio passthrough on multiple threads */

#include <smbPitchShift.h>
#include <portaudio.h>
#include <iostream>
#include <thread>
#include <atomic>

using namespace std;

/*-- Audio parameters --*/
constexpr int kSampleRate      = 44100; // Sample rate in Hz
constexpr int kFramesPerBuffer = 512;   // Number of frames per buffer
constexpr int kNumChannels     = 1;     // Mono channel (Justine recommends using this)
constexpr int kNumSeconds      = 10;    // Run for 10 seconds
constexpr PaSampleFormat kSampleFormat = paFloat32; // 32-bit floating point

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
    PaStream* stream;

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

    err = Pa_OpenStream(
        &stream,
        &inputParams,
        &outputParams,
        kSampleRate,
        kFramesPerBuffer,
        paNoFlag,
        nullptr,
        nullptr
    );
    if (!checkErr(err, "Pa_OpenStream")) return 1;

    err = Pa_StartStream(stream);
    if (!checkErr(err, "Pa_StartStream")) return 1;

    RingBuffer ring;
    std::atomic<bool> done(false);

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
            PaError read_err = Pa_ReadStream(stream, buf, kFramesPerBuffer);
            if (!checkErr(read_err, "Pa_ReadStream")) {
                done = true;
                return;
            }
            ring.write(buf); // drops frame silently if ring buffer is full
        }
    });

    cout << "Beginning the audio process..." << endl;

    /*
     * Main thread — processor
     * Pulls chunks from the ring buffer, pitch shifts them, and writes the
     * result to the output stream. Pa_WriteStream paces this thread to the
     * same hardware rate as the reader, so the ring buffer stays shallow
     * under normal conditions.
     */
    float process_buf[kFramesPerBuffer];
    const int iterations = (kNumSeconds * kSampleRate) / kFramesPerBuffer;
    for (int i = 0; i < iterations && !done; i++) {
        // Wait until the ring buffer has a chunk available
        while (!ring.read(process_buf) && !done) {}
        if (done) break;

        // Pitch shift by a factor of 2
        smbPitchShift(2.0f, kFramesPerBuffer, 1024, 4, kSampleRate, process_buf, process_buf);

        err = Pa_WriteStream(stream, process_buf, kFramesPerBuffer);
        if (!checkErr(err, "Pa_WriteStream")) break;
    }

    done = true;
    reader.join();

    cout << "The program has ended" << endl;

    err = Pa_StopStream(stream);
    if (!checkErr(err, "Pa_StopStream")) return 1;

    err = Pa_CloseStream(stream);
    if (!checkErr(err, "Pa_CloseStream")) return 1;

    Pa_Terminate();
}
