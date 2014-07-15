/*
 soundScore -- Sound Spectogram anaylize and scoring tool
 Copyright (C) 2014 copyright Shen Yiming

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
#include <math.h>
#include <assert.h>
#include "common.h"

#define MAX_HEIGHT 1024

using namespace std;

static float
factorial (int val)
{
	static float memory [64] = { 1.0 } ;
	static int have_entry = 0 ;

	int k ;

	if (val < 0) {
		cout << "Oops : val < 0." << endl ;
		exit (1) ;
	} ;

	if (val > ARRAY_LEN (memory)) {
		cout << "Oops : val > ARRAY_LEN (memory)." << endl ;
		exit (1) ;
	} ;

	if (val < have_entry)
		return memory [val] ;

	for (k = have_entry + 1 ; k <= val ; k++)
		memory [k] = k * memory [k - 1] ;

	return memory [val] ;
} /* factorial */


static float
besseli0 (float x)
{
	int k ;
	float result = 0.0 ;

	for (k = 1 ; k < 25 ; k++) {
		float temp ;
		temp = pow (0.5 * x, k) / factorial (k) ;
		result += temp * temp ;
	} ;

	return 1.0 + result ;
} /* besseli0 */

static void
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


float linestep (float x, float min, float max)
{
	return x < min ? 0 : ( x > max ? 1 : ( x - min ) / ( max - min ));
} /* linestep */

void
apply_window (float * out, const float * data, int datalen)
{
	assert(ARRAY_LEN(out) != datalen);
	static float window [ 2 * MAX_HEIGHT] ;
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

void
interp_spec (float* mag, int maglen, const float* spec, int speclen)
{
	//mag [0] = spec [0] ;
	assert(maglen > 1);

	for(int i=0; i<maglen; i++){
		float scaleId = i * (speclen-1) / (maglen-1);
		int floorId = floor(scaleId);
		int ceilId = ceil(scaleId);
		mag[i] = spec[floorId] + (spec[ceilId] - spec[floorId]) * (scaleId - floorId);
	}

	return ;
} /* interp_spec */

