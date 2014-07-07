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

using namespace std;


//
// Portaudio callback function : FileIn
//
int paFileCallback(	const void *input,
					void *output,
					unsigned long framePerBuffer,
					const PaStreamCallbackTimeInfo *timeInfo,
					PaStreamCallbackFlags statusFlags,
					void *userData)
{
	sndData* data = (sndData*)userData;
	float* out = (float*)output;
	size_t i;

	(void) input;
	(void) timeInfo;
	(void) statusFlags;

	// Read sndfile in, check if the file reach the end?
	// If so, return to the start of sndfile
	//
	int readcount = sf_read_float(data->sndfile, data->data, BUFFER_LEN);
	if(readcount < BUFFER_LEN)
		sf_seek(data->sndfile, 0, SEEK_SET);

	// fill output stream with libsndfile data
	for(i=0; i<framePerBuffer; i++ ){
		*out++ = data->data[i] * data->volume;
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
	std::cout << "Usage : soundView [-h] [-v volume] [-t Max_dB] [-f Floor_dB] filename" << endl;
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

	char* fileName;
	if(optind < argc){
		fileName = argv[optind];
	} else {
		cout << "Sound file name must be specified." << endl;
		return 1;
	}

	//
	// Initialization of libsndfile setup
	//

	// The SF_INFO struct must be initialized before using it.
	memset(&(paUserData.sfinfo), 0, sizeof(paUserData.sfinfo));
	//paUserData->sfinfo = sfinfo;

	if (! (paUserData.sndfile = sf_open(fileName, SFM_READ, &paUserData.sfinfo))) {
		/* Open failed so print an error message. */
		printf ("Not able to open input file %s.\n", fileName) ;
		/* Print the error message from libsndfile. */
		puts (sf_strerror(NULL));
		return	1 ;
	}else {
		printf("frames		:%d\n", (int)paUserData.sfinfo.frames);
		printf("sampleRate	:%d\n", paUserData.sfinfo.samplerate);
		printf("channels	:%d\n", paUserData.sfinfo.channels);
	}

	if (paUserData.sfinfo.channels > MAX_CHANNELS) {
		printf ("Not able to process more than %d channels\n", MAX_CHANNELS) ;
		return	1 ;
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
								0,								// no input channels
								paUserData.sfinfo.channels,		// output channels
								paFloat32,						// 32 bit floating point output
								paUserData.sfinfo.samplerate,	// audio sampleRate
								BUFFER_LEN,						// frames per buffer
								paFileCallback,					// signal process callback function
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

	// First, start portaudio stream
	err = Pa_StartStream( stream );
	if( err != paNoError ) paExitWithError( err );

	while (true)
	{
		cv::imshow(paUserData.winName, paUserData.specMat);
		keyCode = cv::waitKey(20);
		if(keyCode == 'q' || keyCode == 'Q')
		{
			break;
		}

	} ;

	// Stop the portAudio stream
	err = Pa_StopStream( stream );
	if( err != paNoError ) paExitWithError( err );

	err = Pa_CloseStream( stream );
	if( err != paNoError ) paExitWithError( err );

	Pa_Terminate();
	printf("Finished playing.\n");

	/* Close input and output files. */
	sf_close (paUserData.sndfile) ;
	kiss_fftr_free(paUserData.fftcfg);

	return 0 ;
} /* main */

void
paExitWithError(PaError err)
{
	Pa_Terminate();
	fprintf( stderr, "An error occured while using the portaudio stream\n" );
	fprintf( stderr, "Error number: %d\n", err );
	fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
	exit(err);
}

sf_count_t
sfx_mix_mono_read_double (SNDFILE * file, double * data, sf_count_t datalen)
{
	SF_INFO info ;

#if HAVE_SF_GET_INFO
	/*
	**	The function sf_get_info was in a number of 1.0.18 pre-releases but was removed
	**	before 1.0.18 final and replaced with the SFC_GET_CURRENT_SF_INFO command.
	*/
	sf_get_info (file, &info) ;
#else
	sf_command (file, SFC_GET_CURRENT_SF_INFO, &info, sizeof (info)) ;
#endif

	if (info.channels == 1)
		return sf_read_double (file, data, datalen) ;

	static double multi_data [2048] ;
	int k, ch, frames_read ;
	sf_count_t dataout = 0 ;

	while (dataout < datalen)
	{	int this_read ;

		this_read = MIN (ARRAY_LEN (multi_data) / info.channels, datalen) ;

		frames_read = sf_readf_double (file, multi_data, this_read) ;
		if (frames_read == 0)
			break ;

		for (k = 0 ; k < frames_read ; k++)
		{	double mix = 0.0 ;

			for (ch = 0 ; ch < info.channels ; ch++)
				mix += multi_data [k * info.channels + ch] ;
			data [dataout + k] = mix / info.channels ;
			} ;

		dataout += this_read ;
		} ;

	return dataout ;
} /* sfx_mix_mono_read_double */

