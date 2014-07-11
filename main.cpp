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
#include "soundView.h"

#define SAMPLERATE 44100

using namespace std;

void
help(char* command){
	std::cout   << "Usage : " << command
                << "    [-h] [-r] [-v volume] [-t Max_dB] [-f Floor_dB] [filename]" << endl
                << endl
                << "    -h              : view this help" << endl
                << "    -r              : record audio from system microphone" << endl
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
    return std::rand()*100;
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
    char *fn_outputImage, *fn_baseAudio, *fn_input;
    bool isRecord = false;
    bool isScore = false;

	int optionChar, prev_ind;
	while(prev_ind = optind, (optionChar = getopt(argc,argv,"hrv:t:f:o:s:"))!=EOF){
		if(optind == prev_ind + 2 && *optarg == '-' && atoi(optarg)==0){
			optionChar = ':';
			-- optind;
		}
		switch(optionChar){
			case 'v':
				volume = atof(optarg);
				cout << "Volume		:" << volume << endl;
				break;
			case 'f':
				floor_db = atoi(optarg);
				cout << "Floor dB	:" << floor_db << endl;
				break;
			case 't':
				max_db = atoi(optarg);
				cout << "Max dB		:" << max_db << endl;
				break;
            case 'r':
                isRecord = true;
                cout << "Record MIC :" << isRecord << endl;
                break;
            case 'o':
                fn_outputImage = new char[sizeof(optarg)];
                strcpy(fn_outputImage, optarg);
                cout << "Save image :" << fn_outputImage << endl;
                break;
            case 's':
                isScore = true;
                fn_baseAudio = new char[sizeof(optarg)];
                strcpy(fn_baseAudio, optarg);
                cout << "Score with :" << fn_baseAudio << endl;
                break;
			case '?':
			case ':':
				cerr << "Argument error !" << endl;
			case 'h':
				help(argv[0]);
				return 1;
				break;
		}
	}

    // Check whether use microphone, if not, a input audio file is needed
    if (!isRecord) {
        if(optind < argc){
            fn_input = new char[sizeof(argv[optind])];
            strcpy(fn_input, argv[optind]);
        } else {
            cout << "Sound file name must be specified." << endl;
            return 1;
        }
    }
	

	//
	// Initialization of soundView class
	//
    soundView *inputView, *scoreView;
    soundView::Params inputParams;
    inputParams.sampleRate = SAMPLERATE;
    
    if (isRecord) {
        inputParams = soundView::Params(); // default setting is USE_MIC
    } else {
        inputParams.inputDevice = USE_FILE;
        inputParams.inputFilename = new char[sizeof(fn_input)-1];
        strcpy(inputParams.inputFilename, fn_input);
        inputParams.outputDevice = Pa_GetDefaultOutputDevice();
    }
    inputView = new soundView(inputParams);
    inputView->setLevels(volume, max_db, floor_db);

    if (isScore) {
        soundView::Params scoreParams;
        scoreParams.sampleRate = SAMPLERATE;
        scoreParams.inputDevice = USE_FILE;
        scoreParams.outputDevice = Pa_GetDefaultOutputDevice();
        scoreParams.inputFilename = fn_baseAudio;
        scoreView = new soundView(scoreParams);
        scoreView->setLevels(volume, max_db, floor_db);
    }
    
	//
	// The main loop
	//
    cv::namedWindow("Spectogram");
    cv::Mat displayMat;
    char keyCode;
    bool inScore = false;
    
    // If use score, play base audio first
    if (isScore) {
        cout << ">>>> Now player sample audio <<<<" << endl;
        scoreView->start();
        displayMat = scoreView->Spectogram();
        inScore = true;
    } else if (isRecord) {
        cout << "Press 't' to start recording, press 't' again to stop" << endl;
        displayMat = inputView->Spectogram();
        inScore = false;
    } else {
        displayMat = inputView->Spectogram();
        inputView->start();
    }
    
	while (true)
	{
		cv::imshow("Spectogram", displayMat);
		keyCode = cv::waitKey(20);
		if (keyCode == 'q' || keyCode == 'Q') {
			break;
		}
        if (keyCode == 'r' || keyCode == 'R') {
            keyCode = '0';
            if(inScore) scoreView->start();
            else inputView->start();
        }
        if (keyCode == 't' || keyCode == 'T') {
            keyCode = '0';
            if(!inputView->start()) {
                inputView->stop();
                if(isScore)
                    cout << "Score : " <<
                    score(scoreView->Spectogram(), inputView->Spectogram()) << endl;
                break;
            }
        }

	} ;

	// Stop the portAudio stream
    if (isRecord) {
        scoreView->stop();
    }
    inputView->stop();
    
	return 0 ;
} /* main */