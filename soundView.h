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

// Define buffer length to hold the sound data
#define BUFFER_LEN 512


void paExitWithError(PaError err);

class soundView
{
	public:
		soundView();
		~soundView();
		bool open(PaDeviceIndex);
		bool open(PaDeviceIndex, char *);
		bool close();
		bool start();
		bool stop();
		bool setCapture(bool);
	
	private:
		// callback in Portaudio stream
		static int paCallback(	const void *input,
			void *output,
			unsigned long framePerBuffer,
			const PaStreamCallbackTimeInfo *timeInfo,
			PaStreamCallbackFlags statusFlags,
			void *userData);

		static void paStreamFinished(void *userData);

		int paCallbackImpl(	const void *input,
			void *output,
			unsigned long framePerBuffer,
			const PaStreamCallbackTimeInfo *timeInfo,
			PaStreamCallbackFlags statusFlags,
			void *userData);

		sf_count_t sfx_mix_mono_read_double (SNDFILE * file, double * data, sf_count_t datalen) ;
		
		PaStream *stream;
		bool isCapture;			// Whether input from 
		cv::Mat spectogram;		// opencv Mat storing spectogram image

		// kiss_fft data
		kiss_fft_scalar in[BUFFER_LEN*2];
		kiss_fft_cpx out[BUFFER_LEN*2];
		kiss_fftr_cfg fftcfg;

		// libsndfile data
		SNDFILE *sndfile;
		SF_INFO sfinfo;

		// core data
		float *inputData;		// main user data pass to paCallback
		float volume, floor_db, max_db;



};
