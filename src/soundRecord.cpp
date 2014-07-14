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

// libsndfile can handle more than 6 channels but we'll restrict it to 6. */
#define MAX_CHANNELS 1
// Mat height & width
#define WIDTH 512
#define HEIGHT 200

// Define use visual
#define USE_VISUAL true
#define VIS_TOPFREQ 200

#define BUFFER_LEN 512
#define SAMPLERATE 44100

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

using namespace std;

typedef struct{
    // raw sound data in float (-1.0 .. 1.0)
    float data[BUFFER_LEN];
    float volume, max_db, floor_db;
    // libsndfile handle and sfinfo
    SNDFILE* sndfile;
    SF_INFO sfinfo;
    // kiss_fftr setup datas
    kiss_fftr_cfg fftcfg;
    kiss_fft_scalar in[BUFFER_LEN * 2];
    kiss_fft_cpx out[BUFFER_LEN * 2];
    float max_mag;
    // opencv data
    string winName;
    cv::Mat specMat;
    int col;
} sndData;


static void
paExitWithError(PaError err)
{
	Pa_Terminate();
	cerr << "An error occured while using the portaudio stream" << endl ;
	cerr << "Error number	: " << err << endl;
	cerr << "Error message	: " << Pa_GetErrorText( err ) << endl;
	exit(err);
}; /* paExitWithError */


//
// Portaudio callback function : FileIn
//
int recordCallback(	const void *input,
					void *output,
					unsigned long framePerBuffer,
					const PaStreamCallbackTimeInfo *timeInfo,
					PaStreamCallbackFlags statusFlags,
					void *userData)
{
	sndData* data = (sndData*)userData;
	float* in = (float*)input;
	size_t i;

	(void) output;
	(void) timeInfo;
	(void) statusFlags;

	// fill output stream with libsndfile data
	for(i=0; i<framePerBuffer; i++ ){
		//*out++ = data->data[i] * data->volume;
		data->data[i] = in[i] * data->volume;
	}

	//
	// Do time domain windowing and FFT convertion
	//
	float mag [ BUFFER_LEN ] , interp_mag [ HEIGHT ];
	kiss_fft_scalar in_win[ 2 * BUFFER_LEN];

	for(i=0; i<BUFFER_LEN; i++){
		data->in[i] = data->in[ BUFFER_LEN + i ];
		data->in[ BUFFER_LEN + i ] = data->data[i];
	}

	apply_window(in_win, data->in , 2 * BUFFER_LEN);
	kiss_fftr(data->fftcfg, in_win, data->out);

	// 0Hz set to 0
	mag[0] = 0;
	//data->max_db = 0;
	for(int i = 1; i < BUFFER_LEN; i++){
		mag[i] = std::sqrtf(data->out[i].i * data->out[i].i +
							data->out[i].r * data->out[i].r);
		// Convert to dB range 20log10(v1/v2)
		mag[i] = 20 * log10(mag[i]);
		// Convert to RGB space
		mag[i] = linestep(mag[i], data->floor_db, data->max_db) * 255;
	}
	interp_spec(interp_mag, HEIGHT, mag, VIS_TOPFREQ); // Draw sound spectogram

	cv::line(data->specMat, cv::Point2i(data->col,0), cv::Point2i(data->col,HEIGHT), cv::Scalar(0,0,0));
	cv::line(data->specMat, cv::Point2i(data->col+1,0), cv::Point2i(data->col+1,HEIGHT), cv::Scalar(0,0,255));
	for(int row = 0; row < HEIGHT; row++){
		data->specMat.at<cv::Vec3b>(row, data->col)
			= cv::Vec3b(	interp_mag[row],
							interp_mag[row],
							interp_mag[row]);
	}
	data->col = (data->col+1) % WIDTH;

	return 0;
}

void
help(){
	//printf("Usage: soundScore [file1.wav] [drawThreshold 0-200]\n");
	std::cout << "Usage : soundRecord [-h] [-v volume] [-t Max_dB] [-f Floor_dB]" << endl;
}

int
main (int argc, char* argv[])
{
	//float data [ BUFFER_LEN ] ;
	//SNDFILE *stdfile ;
	//SF_INFO sfinfo ;

	sndData paUserData;

	//
	// Commandline argument processing
	//
	if(argc < 2){
		help();
		return 1;
	}

	// Setup default argument
	paUserData.max_db = 80;
	paUserData.floor_db = -180;
	paUserData.volume = 0.2;

	int optionChar, prev_ind;
	while(prev_ind = optind, (optionChar = getopt(argc,argv,"hv:t:f:"))!=EOF){
		if(optind == prev_ind + 2 && *optarg == '-' && atoi(optarg)==0){
			optionChar = ':';
			-- optind;
		}
		switch(optionChar){
			case 'v':
				paUserData.volume = atof(optarg);
				cout << "Volume		:" << paUserData.volume << endl;
				break;
			case 'f':
				paUserData.floor_db = atoi(optarg);
				cout << "Floor dB	:" << paUserData.floor_db<< endl;
				break;
			case 't':
				paUserData.max_db = atoi(optarg);
				cout << "Max dB		:" << paUserData.max_db<< endl;
				break;
			case '?':
			case ':':
				cerr << "Argument error !" << endl;
			case 'h':
				help();
				return 1;
				break;
		}
	}


	//
	// Initialization of portaudio
	//
	PaError err;
	//PaStreamParameters outputParam;
	PaStream* stream;

	err = Pa_Initialize();
	if(err != paNoError)
		paExitWithError(err);

	err = Pa_OpenDefaultStream( &stream,
								1,								// no input channels
								0,		// output channels
								paFloat32,						// 32 bit floating point output
								SAMPLERATE,						// audio sampleRate
								BUFFER_LEN,						// frames per buffer
								recordCallback,					// signal process callback function
								&paUserData);					// data pass to callback
	if(err != paNoError)
		paExitWithError(err);


	//
	// Initialization of Opencv
	//
	paUserData.specMat = cv::Mat(cv::Size(WIDTH, HEIGHT),CV_8UC3);
	paUserData.winName = "WAV Spectorgram";
	paUserData.col = 0;
	cv::namedWindow(paUserData.winName);
	int keyCode;

	//
	// Initialization of FFT
	//
	paUserData.fftcfg = kiss_fftr_alloc( 2 * BUFFER_LEN, 0, NULL, NULL);
	if(paUserData.fftcfg == NULL){
		printf("Fatal: Not enough memory!\n");
		return 1;
	}

	//
	// The main loop
	//
	cout << "Press 'T' to start recording, press 'T' again to stop." << endl;

	while (true)
	{
		cv::imshow(paUserData.winName, paUserData.specMat);
		keyCode = cv::waitKey(20);
		if(keyCode == 'q' || keyCode == 'Q')
		{
			if(!Pa_IsStreamActive( stream )) {
				err = Pa_StopStream( stream );
				if( err != paNoError ) paExitWithError( err );
			}
			break;
		}
		if(keyCode == 't' || keyCode == 'T')
		{
			// start portaudio stream
			if(!Pa_IsStreamActive( stream )){
				err = Pa_StartStream( stream );
				if( err != paNoError ) paExitWithError( err );
			}else{
				// Stop the portAudio stream
				err = Pa_StopStream( stream );
				if( err != paNoError ) paExitWithError( err );
			}
		}

	} ;

	err = Pa_CloseStream( stream );
	if( err != paNoError ) paExitWithError( err );

	Pa_Terminate();
	printf("Finished playing.\n");

	/* Close input and output files. */
	//sf_close (paUserData.sndfile) ;
	kiss_fftr_free(paUserData.fftcfg);

	return 0 ;
} /* main */

