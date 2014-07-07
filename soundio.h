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

 File Name: soundio.h
 Description:
 Class definition for sound data input / ouput.
 sound input/output using portaudio.
 sound file readin/out using libsndfile.
 */
#include <portaudio.h>

enum SNDIO_PARAM{
	USE_MIC = 0,
	USE_FILE = 1
};

class soundio
{
	public:
		struct Params{
			Params();
			SNDIO_PARAM input;
			char* inputFilename;
			char* outputFilename;
		};

		soundio(const soundio::Params &parameters = soundio::Params());
		bool start();
		bool stop();

	private:

		bool init();

		static int paRecordCallback(const void* inputBuffer, void* outputBuffer,
				unsigned long framesPerBuffer,
				const PaStreamCallbackTimeInfo* timeInfo,
				PaStreamCallbackFlags statusFlags,
				void* userData);

		int paRecordCallbackImpl(const void* inputBuffer, void* outputBuffer,
				unsigned long framesPerBuffer,
				const PaStreamCallbackTimeInfo* timeInfo,
				PaStreamCallbackFlags statusFlags);

		bool recordMic();
		bool recordFile();

		float* sndCache;
		Params params;
		float max_db;
		long currentIndex;
};

