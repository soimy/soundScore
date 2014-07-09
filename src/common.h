#ifndef COMMON_H
#define COMMON_H

#ifndef ARRAY_LEN
#define ARRAY_LEN(x)	((int) (sizeof (x) / sizeof (x [0])))
#endif

#ifndef MAX
#define MAX(x,y)		((x) > (y) ? (x) : (y))
#endif
#ifndef MIN
#define MIN(x,y)		((x) < (y) ? (x) : (y))
#endif

float linestep (float x, float min, float max);
void apply_window (float* out, const float* data, int datalen);
void interp_spec (float* mag, int maglen, const float* spec, int speclen);

#endif


