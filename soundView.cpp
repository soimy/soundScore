#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
// libsndfile addon include file
#include <sndfile.h>
// Opencv addon include file
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
// kissFFT addon include file
#include "kiss_fft.h"
#include "kiss_fftr.h"
// Portaudio 2.0 include file
#include <portaudio.h>

// Define buffer length to hold the sound data
#define BUFFER_LEN 512
// libsndfile can handle more than 6 channels but we'll restrict it to 6. */
#define MAX_CHANNELS 1
// Mat height & width
#define WIDTH 512
#define HEIGHT 200

// Define use visual
#define USE_VISUAL true

#define ARRAY_LEN(x)		((int) (sizeof (x) / sizeof (x [0])))

using namespace std;


typedef struct{
	// raw sound data in float (-1.0 .. 1.0)
	float data[BUFFER_LEN];
	float volume;
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


static float besseli0 (float x) ;
static float factorial (int k) ;
static void apply_window (float* out, const float* data, int datalen);
static void interp_spec (float* mag, int maglen, const float* spec, int speclen);
static void paExitWithError(PaError err);

// Portaudio callback function
static int paCallback(	const void *input,
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
	float mag [ BUFFER_LEN ] /*, interp_mag [ HEIGHT ]*/;
	kiss_fft_scalar in_win[ 2 * BUFFER_LEN];

	for(i=0; i<BUFFER_LEN; i++){
		data->in[i] = data->in[ BUFFER_LEN + i ];
		data->in[ BUFFER_LEN + i ] = data->data[i];
	}

	apply_window(in_win, data->in , 2 * BUFFER_LEN);
	kiss_fftr(data->fftcfg, in_win, data->out);

	// 0Hz set to 0
	mag[0] = 0;
	//data->max_mag = 0;
	for(int i = 1; i < BUFFER_LEN; i++){
		mag[i] = std::sqrtf(data->out[i].i * data->out[i].i +
							data->out[i].r * data->out[i].r);
		//data->max_mag = std::max(data->max_mag, mag[i]);
		//printf("mag=%f\n", data->max_mag);
	}
	// Draw sound spectogram
	cv::line(data->specMat, cv::Point2i(data->col,0), cv::Point2i(data->col,HEIGHT), cv::Scalar(0,0,0));
	cv::line(data->specMat, cv::Point2i(data->col+1,0), cv::Point2i(data->col+1,HEIGHT), cv::Scalar(0,0,255));
	for(int row = 0; row < HEIGHT; row++){
		data->specMat.at<cv::Vec3b>(row, data->col)
			= cv::Vec3b(	mag[row]/data->max_mag * 255,
							mag[row]/data->max_mag * 255,
							mag[row]/data->max_mag * 255);
	}
	data->col = (data->col+1) % WIDTH;

	return 0;
}

void
help(){
	printf("Usage: soundScore [file1.wav] [drawThreshold 0-200]\n");
}

int
main (int argc, char* argv[])
{
	//float data [ BUFFER_LEN ] ;
	//SNDFILE *stdfile ;
	//SF_INFO sfinfo ;

	sndData paUserData;

	if(argc < 2){
		help();
		return 1;
	}

	//
	// Initialization of libsndfile setup
	//

	// The SF_INFO struct must be initialized before using it.
	if(argc < 3)
		paUserData.max_mag = 180;
	else
		paUserData.max_mag = std::atof(argv[2]);
	paUserData.volume = 0.2;
	memset(&(paUserData.sfinfo), 0, sizeof(paUserData.sfinfo));
	//paUserData->sfinfo = sfinfo;

	if (! (paUserData.sndfile = sf_open(argv[1], SFM_READ, &paUserData.sfinfo))) {
		/* Open failed so print an error message. */
		printf ("Not able to open input file %s.\n", argv[1]) ;
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
								paUserData.sfinfo.channels,	// output channels
								paFloat32,						// 32 bit floating point output
								paUserData.sfinfo.samplerate,	// audio sampleRate
								BUFFER_LEN,						// frames per buffer
								paCallback,						// signal process callback function
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
	//kiss_fftr_cfg kfft_cfg;
	//float in[ 2 * BUFFER_LEN], in_win[ 2 * BUFFER_LEN];
	//kiss_fft_cpx out[ 2 * BUFFER_LEN];
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
calc_kaiser_window (float* data, int datalen, float beta)
{
	/*
	**			besseli0 (beta * sqrt (1- (2*x/N).^2))
	** w (x) =	--------------------------------------,  -N/2 <= x <= N/2
	**				   besseli0 (beta)
	*/

	float two_n_on_N, denom ;
	int k ;

	denom = besseli0 (beta) ;

	if (! isfinite (denom)) {
		printf ("besseli0 (%f) : %f\nExiting\n", beta, denom) ;
		exit (1) ;
	} ;

	for (k = 0 ; k < datalen ; k++) {
		float n = k + 0.5 - 0.5 * datalen ;
		two_n_on_N = (2.0 * n) / datalen ;
		data [k] = besseli0 (beta * sqrt (1.0 - two_n_on_N * two_n_on_N)) / denom ;
	} ;

	return ;
} /* calc_kaiser_window */

static float
besseli0 (float x)
{	int k ;
	float result = 0.0 ;

	for (k = 1 ; k < 25 ; k++) {
		float temp ;
		temp = pow (0.5 * x, k) / factorial (k) ;
		result += temp * temp ;
	} ;

	return 1.0 + result ;
} /* besseli0 */

static float
factorial (int val)
{	static float memory [64] = { 1.0 } ;
	static int have_entry = 0 ;

	int k ;

	if (val < 0) {
		printf ("Oops : val < 0.\n") ;
		exit (1) ;
	} ;

	if (val > ARRAY_LEN (memory)) {
		printf ("Oops : val > ARRAY_LEN (memory).\n") ;
		exit (1) ;
	} ;

	if (val < have_entry)
		return memory [val] ;

	for (k = have_entry + 1 ; k <= val ; k++)
		memory [k] = k * memory [k - 1] ;

	return memory [val] ;
} /* factorial */

static void
apply_window (float * out, const float * data, int datalen)
{
	assert(ARRAY_LEN(out) != datalen);
	static float window [ 2 * BUFFER_LEN] ;
	static int window_len = 0 ;
	int k ;
	if (window_len != datalen) {
		window_len = datalen ;
		if (datalen > ARRAY_LEN (window)) {
			printf ("%s : datalen >  MAX_WIDTH\n", __func__) ;
			exit (1) ;
		} ;

		calc_kaiser_window (window, datalen, 20.0) ;
	} ;

	for (k = 0 ; k < datalen ; k++)
		out[k] = data[k] * window [k] ;

	return ;
} /* apply_window */

static void
interp_spec (float* mag, int maglen, const float* spec, int speclen)
{
	int k, lastspec = 0 ;

	mag [0] = spec [0] ;

	for (k = 1 ; k < maglen ; k++) {
		float sum = 0.0 ;
		int count = 0 ;

		do {
			sum += spec [lastspec] ;
			lastspec ++ ;
			count ++ ;
		} while (lastspec <= ceil ((k * speclen) / maglen)) ;

		mag [k] = sum / count ;
	} ;

	return ;
} /* interp_spec */

static void
paExitWithError(PaError err)
{
	Pa_Terminate();
	fprintf( stderr, "An error occured while using the portaudio stream\n" );
	fprintf( stderr, "Error number: %d\n", err );
	fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
	exit(err);
}
