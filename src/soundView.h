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

	// Controling functions
    bool start();
    bool stop();

	// Setting function
    void setLevels(const float _volume, const float _max_db, const float _floor_db);

	// Querying functions
	bool isPlayback();
	bool isRecord();
	int isPlaying();

	// Output function
    cv::Mat Spectogram();
    
	// Static functions
    static void init();
    static void close();

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
        PaStreamCallbackFlags statusFlags );

   
    // callback in Portaudio stream
    static int playCallback(	const void *input,
        void *output,
        unsigned long framePerBuffer,
        const PaStreamCallbackTimeInfo *timeInfo,
        PaStreamCallbackFlags statusFlags,
        void *userData);
    
    int playCallbackImpl(	const void *input,
        void *output,
        unsigned long framePerBuffer,
        const PaStreamCallbackTimeInfo *timeInfo,
        PaStreamCallbackFlags statusFlags );
    
    void drawBuffer(const void* input);
    
	// portaudio variables
    PaStream *stream;
	PaStreamParameters inputParams, outputParams;

	// opencv variables
    cv::Mat spectogram;		// opencv Mat storing spectogram image
    unsigned int col;       // current columne in Mat drawing

    // kiss_fft data
    kiss_fft_scalar in[BUFFER_LEN*2];
    kiss_fft_cpx out[BUFFER_LEN*2];
    kiss_fftr_cfg fftcfg;

    // libsndfile data
    SndfileHandle sndHandle;

    // sound core data
    float *inputData;
    float volume, floor_db, max_db;

    Params params;
};
