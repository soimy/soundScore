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
#include <sndfile.h>

// Opencv addon include file
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>

// Portaudio 2.0 include file
#include <portaudio.h>

// kissFFT addon include file
#include "kiss_fft.h"
#include "kiss_fftr.h"

// local includes
#include "common.h"
#include "soundView.h"

// libsndfile can handle more than 6 channels but we'll restrict it to 6. */
#define MAX_CHANNELS 2
// Mat height & width
#define WIDTH 512
#define HEIGHT 200

// Define use visual
#define USE_VISUAL true
#define VIS_TOPFREQ 200


using namespace std;

void
soundView::paExitWithError(PaError err)
{
	Pa_Terminate();
	cerr << "An error occured while using the portaudio stream" << endl ;
	cerr << "Error number	: " << err << endl;
	cerr << "Error message	: " << Pa_GetErrorText( err ) << endl;
	exit(err);
}; /* paExitWithError */

soundView::Params::Params()
{
    // Default setting is using microphone input
    inputDevice = USE_MIC;
    outputDevice = paNoDevice;
//    *inputFilename = '\0';
    sampleRate = 44100;
}; /* soundView::Params::Params() */

soundView::soundView(const soundView::Params &parameters) :
    stream(0), volume(1), floor_db(0), max_db(200), params(parameters),
    spectogram(cv::Size(WIDTH, HEIGHT),CV_8UC3)
{
	
	//
	// Initialization of FFT
	//
	fftcfg = kiss_fftr_alloc( 2 * BUFFER_LEN, 0, NULL, NULL);
	if(fftcfg == NULL){
		cerr << "Fatal: Not enough memory!" << endl;
		exit(-1) ;
	}
    
    // memory allocation of sound data
    inputData = new float[BUFFER_LEN];
    if (inputData == NULL) {
		cerr << "Fatal: Not enough memory!" << endl;
        exit(-1);
    }
    
    //
    // Initialization I/O
    //
    if (params.inputDevice == USE_FILE) {
        if (!init_file())
            exit(-1);
    } else {
        if (!init_mic())
            exit(-1);
    }
    
}; /* soundView::soundView() */

bool
soundView::close()
{
    Pa_Terminate();
    return true;
}; /* soundView::close() */

bool
soundView::start()
{
    PaError err;
    // First need to check if the stream is occupied
//    if (Pa_IsStreamActive( stream )) {
//        cerr << "!!!! Audio stream is busy !!!!" << endl;
//        return false;
//    }
    
    if (params.inputDevice == USE_MIC)
    {
//        err = Pa_OpenStream(&stream, &inputParameters, NULL, params.sampleRate,
//                                    BUFFER_LEN, paClipOff, RecordCallback, this);
        err = Pa_OpenDefaultStream(&stream, 1, 0, paFloat32, params.sampleRate, BUFFER_LEN, RecordCallback, this);
        if(err!=paNoError)
            paExitWithError( err );
        err = Pa_StartStream( stream );
        if(err!=paNoError)
            paExitWithError( err );
        cout << ">>>> Now recording , please speak to the microphone. <<<<" << endl;
    }
    else
    {
        // If no output device assigned, we will leave the matter to playCallback, not here
        sndHandle.seek(0, SEEK_SET); // seek to the start of sound file.
//        err = Pa_OpenStream(&stream, NULL, &outputParameters, params.sampleRate,
//                            BUFFER_LEN, paClipOff, playCallback, this);
        err = Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, params.sampleRate, BUFFER_LEN, playCallback, this);
        if(err!=paNoError)
            paExitWithError( err );
        err = Pa_StartStream( stream );
        if(err!=paNoError)
            paExitWithError( err );
        if(params.outputDevice == paNoDevice)
            cout << ">>>> Opening file : " << params.inputFilename << " <<<<" << endl;
        else
            cout << ">>>> Playing file : " << params.inputFilename << " <<<<" << endl;
    }
    return true;
} /* soundView::start() */

bool
soundView::stop()
{
    PaError err;
    err = Pa_CloseStream( stream );
    if(err != paNoError)
        paExitWithError(err);
    return true;
} /* soundView:stop() */

void
soundView::setLevels(const float _volume, const float _max_db, const float _floor_db)
{
    volume = _volume;
    max_db = _max_db;
    floor_db = _floor_db;
}

cv::Mat
soundView::Spectogram()
{
    return spectogram;
}

bool
soundView::init_file()
{
    //
    // Initialization of libsndfile setup
    //
    sndHandle = SndfileHandle(params.inputFilename);
    
    if (! sndHandle.rawHandle()) {
        /* Open failed so print an error message. */
        std::cerr << "Not able to open input file : " << params.inputFilename << endl ;
        /* Print the error message from libsndfile. */
        puts ( sndHandle.strError() );
        return false;
    }else {
        cout << "Opening sound file : " << params.inputFilename << endl;
        cout << "Frames             : " << sndHandle.frames() << endl;
        cout << "Sample rate        : " << sndHandle.samplerate() << endl;
        cout << "Channels           : " << sndHandle.channels() << endl;
    }
    
    if (sndHandle.channels() > MAX_CHANNELS) {
        printf ("Not able to process more than %d channels\n", MAX_CHANNELS) ;
        return false;
    }

    //
    // Initializing portaudio output device
    //
    PaError err = Pa_Initialize();
    if (err != paNoError)
        paExitWithError(err);
    
    outputParameters.device = params.outputDevice;
//    if (outputParameters.device == paNoDevice) {
//        std::cerr   << "Cannot open output sound device : "
//                    << params.outputDevice << endl;
//        return false;
//    }
    
//    outputParameters.channelCount = 2;       /* stereo output */
//    outputParameters.sampleFormat = paFloat32; /* 32 bit floating point output */
//    outputParameters.suggestedLatency =
//        Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
//    outputParameters.hostApiSpecificStreamInfo = NULL;

    return true;
} /* soundView::init_file() */

bool
soundView::init_mic()
{

    //
    // Initialization of portaudio record stream
    //
    PaError err = Pa_Initialize();
    if (err != paNoError)
        paExitWithError(err);
    inputParameters.device = Pa_GetDefaultInputDevice();
    if (inputParameters.device == paNoDevice) {
        cerr << "Cannot open audio record device!" << endl;
        paExitWithError(err);
    }
    inputParameters.channelCount = 1;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency =
        Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;
    
    // Use microphone recording don't require playback
    // So no need to initialize portaudio output
    
    return true;
} /* soundView::init_mic() */


//
// Portaudio callback function : FileIn
//
int
soundView::RecordCallback(const void *input,
                          void *output,
                          unsigned long framePerBuffer,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData)
{
    return ((soundView*)userData)->RecordCallbackImpl(input, output,
                        framePerBuffer, timeInfo, statusFlags);
} /* soundView::RecordCallback */

int
soundView::RecordCallbackImpl(	const void *input,
					void *output,
					unsigned long framePerBuffer,
					const PaStreamCallbackTimeInfo *timeInfo,
					PaStreamCallbackFlags statusFlags)
{
	const float* rptr = (const float*)input;
	size_t i;

	(void) output;
	(void) timeInfo;
	(void) statusFlags;

	// fill output stream with libsndfile data
	for(i=0; i<framePerBuffer; i++ ){
		*inputData = *rptr * volume; // multiply by volume setting
        inputData++, rptr++;
	}
    drawBuffer(inputData);
    return paContinue;
} /* soundView:RecordCallbackImpl */

//
// Portaudio callback function : FileIn
//
int
soundView::playCallback(const void *input,
                          void *output,
                          unsigned long framePerBuffer,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData)
{
    return ((soundView*)userData)->RecordCallbackImpl(input, output,
                        framePerBuffer, timeInfo, statusFlags);
} /* soundView::playCallback */

int
soundView::playCallbackImpl(	const void *input,
					void *output,
					unsigned long framePerBuffer,
					const PaStreamCallbackTimeInfo *timeInfo,
					PaStreamCallbackFlags statusFlags)
{
	float* wptr = (float*)output;
	size_t i;

	(void) input ;
	(void) timeInfo;
	(void) statusFlags;

	// If it's dual channel audio, double the read buffer
    // And downsample to mono data
    unsigned int chn = sndHandle.channels();
    if (chn > 1) {
        float* steroData = new float[framePerBuffer * chn];
        sndHandle.read(steroData, framePerBuffer * chn);
        for (i=0; i<framePerBuffer; i++) {
            float mix = 0;
            for(int j = 0; j < chn; j++)
                mix += steroData[ i * chn + j ];
            *inputData++ = mix/chn;
        }
        
    }else
        sndHandle.read(inputData, framePerBuffer);
    

    // If has playback device supplied
	// fill output stream with libsndfile data
    if (params.outputDevice != paNoDevice) {
        for(i=0; i<framePerBuffer; i++ ){
            *wptr++ = *inputData * volume;
            if( sndHandle.channels() == 2 ){
                inputData ++;
                *wptr++ = *inputData++ * volume;
            } else {
                *wptr++ = *inputData++ * volume;
            }
        }
    }
    
	// Read sndfile in, check if the file reach the end?
	// If so, return to the start of sndfile (loop)
    // Or, finish playing with paComplete
	if(ARRAY_LEN(inputData) < framePerBuffer) {
        // sndHandle.seek(0, SEEK_SET);
        cout << "Playback finished, Press 'r' to replay." << endl;
        return paComplete;
    }
    
    return paContinue;
} /* soundView::playbackImpl */

void
soundView::drawBuffer(const void* input)
{

    const float* data = (const float*)input;
    size_t i;
	//
	// Do time domain windowing and FFT convertion
	//
	float mag [ BUFFER_LEN ] , interp_mag [ HEIGHT ];
	kiss_fft_scalar in_win[ 2 * BUFFER_LEN];

	for(i=0; i<BUFFER_LEN; i++){
		in[i] = in[ BUFFER_LEN + i ];
		in[ BUFFER_LEN + i ] = data[i];
	}

	apply_window(in_win, in , 2 * BUFFER_LEN);
	kiss_fftr(fftcfg, in_win, out);

	// 0Hz set to 0
	mag[0] = 0;
	//data->max_db = 0;
	for(int i = 1; i < BUFFER_LEN; i++){
		mag[i] = std::sqrtf(out[i].i * out[i].i +
							out[i].r * out[i].r);
		// Convert to dB range 20log10(v1/v2)
		mag[i] = 20 * log10(mag[i]);
		// Convert to RGB space
		mag[i] = linestep(mag[i], floor_db, max_db) * 255;
	}
	interp_spec(interp_mag, HEIGHT, mag, VIS_TOPFREQ); // Draw sound spectogram

	cv::line(spectogram, cv::Point2i(col,0), cv::Point2i(col,HEIGHT), cv::Scalar(0,0,0));
	cv::line(spectogram, cv::Point2i(col+1,0), cv::Point2i(col+1,HEIGHT), cv::Scalar(0,0,255));
	for(int row = 0; row < HEIGHT; row++){
		spectogram.at<cv::Vec3b>(row, col)
			= cv::Vec3b(	interp_mag[row],
							interp_mag[row],
							interp_mag[row]);
	}
	col = (col+1) % WIDTH;
} /* soundView::drawBuffer */


