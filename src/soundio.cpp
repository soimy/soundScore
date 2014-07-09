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

 File Name: soundio.cpp
 Description:
 Class implemention for sound data input / ouput.
 sound input/output using portaudio.
 sound file readin/out using libsndfile.
 */

#include <iostream>
#include <math.h>
#include <sndfile.hh>
#include <portaudio.h>

#include "soundio.h"

soundio::Params::Params()
{
	 input = USE_MIC;
	 inputFilename = NULL;
	 outputFilename = NULL;
};

soundio::soundio(const soundio::Params &parameters): params(parameters)
{
	if(params.input == USE_MIC)
		init();
}

bool
soundio::start()
{
	if(params.input == USE_MIC){
		return recordMic();
	}else{
		return recordFile();
	}
}

bool
soundio::stop()
{
	if(params.input == USE_MIC){
		return stopMic();
	}else{
		return true;
	}
}

bool
soundio::init()
{
    return true;
}
