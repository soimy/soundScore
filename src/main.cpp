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

#include <iostream>
#include <getopt.h>
#include <string>
#include <opencv2/opencv.hpp>
#include "soundView.h"
#include "common.h"

#define SAMPLERATE 44100

using namespace std;

void
help(char* command){
	std::cout   << "Usage : " << command
                << "    [-hrp] [-vtfos arguments] [filename]" << endl
                << endl
                << "    -h              : view this help" << endl
                << "    -r              : record audio from system microphone" << endl
                << "    -p              : playback audio" << endl
                << "    -v volume       : set the input/output volume level(dB), default = 1" << endl
                << "    -t Max_dB       : set the max visualized level(dB), default = 200dB" << endl
                << "    -f floor_dB     : set the min visualized level(dB), default = -180dB" << endl
                << "    -o image_file   : save visualized image to file" << endl
                << "    -s basefile     : compare with basefile and output score" << endl
                << "    filename        : input audio file (WAV|OGG|FLAC supported)" << endl
                << "                      if has '-r', this file is ignored." << endl;

}

int
score(cv::Mat mat1, cv::Mat mat2){
    cv::Mat v1, v2;
    cv::cvtColor(mat1, v1, CV_BGR2GRAY);
    cv::cvtColor(mat2, v2, CV_BGR2GRAY);
    cv::MatND hist1,hist2;
    
    /// Using 50 bins for hue and 60 for saturation
    int bins = 256;
    int histSize[] = { bins };
    
    // hue varies from 0 to 179, saturation from 0 to 255
    float l_ranges[] = { 0, 256 };
    
    const float* ranges[] = { l_ranges };
    
    // Use the o-th and 1-st channels
    int channels[] = { 0 };
    
    // Calculate the histograms for the HSV images
    calcHist( &v1, 1, channels, cv::Mat(), hist1, 1, histSize, ranges, true, false );
    normalize( hist1, hist1, 0, 1, cv::NORM_MINMAX, -1, cv::Mat() );
    
    calcHist( &v2, 1, channels, cv::Mat(), hist2, 1, histSize, ranges, true, false );
    normalize( hist2, hist2, 0, 1, cv::NORM_MINMAX, -1, cv::Mat() );
    
    float res, base;
    res = cv::compareHist(hist1, hist2, 0);
    base = cv::compareHist(hist1, hist1, 0);
//    cout << res << " | " << base << endl;
    return  100 - (base - res)  * 300000;
//    return std::rand()k
}

bool
phrase(soundView *view)
{
    int keyCode;
    cv::Mat specMat = view->Spectogram();
    cv::namedWindow("Spectogram");
    view->start();
    if(view->isPlayback())
    {
        std::cout << "[Info] Press 'q' to terminate." << endl ;
        while(view->isPlaying())
        {
            cv::imshow("Spectogram", specMat);
            keyCode = cv::waitKey(20);
            if(keyCode == 'q' || keyCode == 'Q')
            {
                view->stop();
                break;
            }
        }
    }
    return true;
}

int
main (int argc, char* argv[])
{
	//
	// Commandline argument processing
	//
	if(argc < 2){
		help(argv[0]);
		return 1;
	}

	// Setup default argument
	float max_db = 80;
	float floor_db = -180;
	float volume = 1;
    char fn_outputImage[255], fn_baseAudio[255], fn_input[255];
    bool isRecord = false;
    bool isScore = false;
    bool isPlayback = false;
    bool isSave = false;

	int optionChar, prev_ind;
	while(prev_ind = optind, (optionChar = getopt(argc,argv,"hrpv:t:f:o:s:"))!=EOF){
		if(optind == prev_ind + 2 && *optarg == '-' && atoi(optarg)==0){
			optionChar = ':';
			-- optind;
		}
		switch(optionChar){
			case 'v':
				volume = atof(optarg);
				cout << "Volume             : " << volume << endl;
				break;
			case 'f':
				floor_db = atoi(optarg);
				cout << "Floor dB           : " << floor_db << endl;
				break;
			case 't':
				max_db = atoi(optarg);
				cout << "Max dB             : " << max_db << endl;
				break;
            case 'r':
                isRecord = true;
                cout << "Record MIC         : " << isRecord << endl;
                break;
            case 'p':
                isPlayback = true;
                cout << "isPlayback         : " << isPlayback << endl;
                break;
            case 'o':
                isSave = true;
                strcpy(fn_outputImage, optarg);
                cout << "Output             : " << fn_outputImage << endl;
                break;
            case 's':
                isScore = true;
                strcpy(fn_baseAudio, optarg);
                cout << "Score with         : " << fn_baseAudio << endl;
                break;
			case '?':
			case ':':
				cerr << "[Error] Argument error." << endl;
			case 'h':
				help(argv[0]);
				return 1;
				break;
		}
	}

    // Check whether using microphone, if not, a input audio file is needed
    if (!isRecord) {
        if(optind < argc){
            strcpy(fn_input, argv[optind]);
        } else {
            cerr << "[Error] Sound file name must be specified." << endl;
            return 1;
        }
    }


	//
	// Initialization of soundView class
	//

    soundView::init();

    soundView *inputView, *scoreView;
    soundView::Params inputParams, scoreParams;

    if (!isRecord)
    {
        inputParams.inputDevice = USE_FILE;
        inputParams.inputFilename = new char[ARRAY_LEN(fn_input)+1];
        strcpy(inputParams.inputFilename, fn_input);
        inputParams.outputDevice = isPlayback ? Pa_GetDefaultOutputDevice() : paNoDevice;
        inputParams.sampleRate = SAMPLERATE;
    } else {
        inputParams.inputDevice = USE_MIC;
        inputParams.outputDevice = paNoDevice;
        inputParams.sampleRate = SAMPLERATE;
    }

    inputView = new soundView(inputParams);
    inputView->setLevels(volume, max_db, floor_db);

	//
	// The main loop
	//

    //
    // Phrase 1 : Load the score base audio
    //
    if (isScore) {

        // initialize soundView scoreView
        scoreParams.sampleRate = SAMPLERATE;
        scoreParams.inputDevice = USE_FILE;
        scoreParams.inputFilename = new char[ARRAY_LEN(fn_baseAudio)+1];
        strcpy(scoreParams.inputFilename, fn_baseAudio);
        scoreParams.outputDevice = isPlayback ? Pa_GetDefaultOutputDevice() : paNoDevice;
        scoreView = new soundView(scoreParams);
        scoreView->setLevels(volume, max_db, floor_db);

        phrase(scoreView);
    }

    //
    // Phrase 2 : Load the input audio
    // if USE_MIC, start recording and draw Spectogram
    // if USE_FILE, playback and draw Spectogram
    //
    phrase(inputView);


    // Save the Spectogram image
    if(isSave){
        string fn(fn_outputImage, find(fn_outputImage, fn_outputImage+255, '\0'));
        bool status = cv::imwrite(fn, inputView->Spectogram());
        if(status)
            cout << "[Info] Successfully saved to file : " << fn << endl;
        else
            cerr << "[Error] Failed saving file : " << fn << endl;

    }


    // Print the score
    if(isScore)
        std::cout << "[Score] " << score(scoreView->Spectogram(), inputView->Spectogram()) << endl;


    // Close the portaudio
    soundView::close();
	return 0 ;
} /* main */

