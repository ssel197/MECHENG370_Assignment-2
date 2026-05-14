/****************************************************************************
*
* NAME: smbPitchShift.h
* VERSION: 1.2
* HOME URL: http://blogs.zynaptiq.com/bernsee
*
* SYNOPSIS: Header file for the pitch shifting routine using the Short Time
* Fourier Transform.
*
* COPYRIGHT 1999-2015 Stephan M. Bernsee <s.bernsee [AT] zynaptiq [DOT] com>
*
*                       The Wide Open License (WOL)
*
* Permission to use, copy, modify, distribute and sell this software and its
* documentation for any purpose is hereby granted without fee, provided that
* the above copyright notice and this license appear in all source copies.
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF
* ANY KIND. See http://www.dspguru.com/wol.htm for more information.
*
*****************************************************************************/

#ifndef SMB_PITCH_SHIFT_H
#define SMB_PITCH_SHIFT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/* Constants                                                                 */
/* ------------------------------------------------------------------------- */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_FRAME_LENGTH 8192

/* ------------------------------------------------------------------------- */
/* Function declarations                                                     */
/* ------------------------------------------------------------------------- */

/*
 * smbPitchShift
 * -------------
 * Performs pitch shifting on a block of audio samples while preserving
 * duration, using the Short Time Fourier Transform (STFT).
 *
 * Parameters:
 *   pitchShift        - Pitch shift factor in the range [0.5, 2.0].
 *                       0.5 = one octave down, 1.0 = no change, 2.0 = one octave up.
 *   numSampsToProcess - Number of samples to process from indata into outdata.
 *   fftFrameSize      - FFT frame size. Must be a power of 2 and
 *                       <= MAX_FRAME_LENGTH. Typical values: 1024, 2048, 4096.
 *   osamp             - STFT oversampling factor (overlap between frames).
 *                       Use at least 4; 32 is recommended for best quality.
 *   sampleRate        - Sample rate of the audio in Hz (e.g. 44100.0f).
 *   indata            - Input sample buffer, values in range [-1.0, 1.0).
 *   outdata           - Output sample buffer. May be the same buffer as indata
 *                       for in-place processing.
 */
void smbPitchShift(float pitchShift,
                   long numSampsToProcess,
                   long fftFrameSize,
                   long osamp,
                   float sampleRate,
                   float *indata,
                   float *outdata);

/*
 * smbFft
 * ------
 * In-place FFT / inverse FFT routine.
 *
 * Parameters:
 *   fftBuffer    - Interleaved complex buffer of length 2*fftFrameSize,
 *                  laid out as [re0, im0, re1, im1, ...].
 *   fftFrameSize - Number of complex samples. Must be a power of 2.
 *   sign         - -1 for forward FFT, +1 for inverse FFT.
 */
void smbFft(float *fftBuffer, long fftFrameSize, long sign);

/*
 * smbAtan2
 * --------
 * Replacement for the standard atan2() that avoids domain errors on some
 * platforms. Returns 0 when x == 0, and +/- PI/2 when y == 0.
 */
double smbAtan2(double x, double y);

#ifdef __cplusplus
}
#endif

#endif /* SMB_PITCH_SHIFT_H */
