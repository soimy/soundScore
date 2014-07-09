/*
 soundScore -- Sound Spectogram anaylize and scoring tool
 Copyright (C) 2014 copyright Shen Yiming <sym@shader.cn>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 */

// Standard include files
#include <iostream>
#include <getopt.h>
#include <string.h>
#include <math.h>
#include <assert.h>

// libsndfile addon include file
#include <sndfile.hh>

// Opencv addon include file
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>

// Portaudio 2.0 include file
#include <portaudio.h>

// kissFFT addon include file
#include "kiss_fft.h"
#include "kiss_fftr.h"

// Define buffer length to hold the sound data
#define BUFFER_LEN 512



enum SNDV_PARAM {
    USE_MIC = 0,
    USE_FILE = 1
};

class soundView
{
public:
    struct Params{
        Params();
        SNDV_PARAM inputDevice;
        PaDeviceIndex outputDevice;
        char* inputFilename;
        double sampleRate;
    };
    
    soundView(const soundView::Params &parameters = soundView::Params());
    ~soundView();
    bool close();
    bool start();
    bool stop();

private:
    bool init_file();
    bool init_mic();
    // callback in Portaudio stream
    static int RecordCallback(	const void *input,
        void *output,
        unsigned long framePerBuffer,
        const PaStreamCallbackTimeInfo *timeInfo,
        PaStreamCallbackFlags statusFlags,
        void *userData);
    
    int RecordCallbackImpl(	const void *input,
        void *output,
        unsigned long framePerBuffer,
        const PaStreamCallbackTimeInfo *timeInfo,
        PaStreamCallbackFlags statusFlags,
        void *userData);

   
    // callback in Portaudio stream
    static int playCallback(	const void *input,
        void *output,
        unsigned long framePerBuffer,
        const PaStreamCallbackTimeInfo *timeInfo,
        PaStreamCallbackFlags statusFlags,
        void *userData);

    static void paStreamFinished(void *userData);
    
    sf_count_t sfx_mix_mono_read_double (SNDFILE * file, double * data, sf_count_t datalen) ;
    void paExitWithError(PaError err);
    
    PaStream *stream;
    cv::Mat spectogram;		// opencv Mat storing spectogram image

    // kiss_fft data
    kiss_fft_scalar in[BUFFER_LEN*2];
    kiss_fft_cpx out[BUFFER_LEN*2];
    kiss_fftr_cfg fftcfg;

    // libsndfile data
    SndfileHandle sndHandle;

    // portaudio params
    PaStreamParameters inputParameters, outputParameters;
    
    // sound core data
    float *inputData;
    float volume, floor_db, max_db;

    Params params;
};
