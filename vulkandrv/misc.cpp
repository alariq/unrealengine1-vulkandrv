/**
\namespace misc
Miscellaneous helper functions that don't depend on an API
*/

#include <cmath>
#include "misc.h"

static const float PI = 3.1415926535897932f;

/**
Calculates the horizontal field of view for a given resolution.
Code from http://emsai.net/projects/widescreen/fovcalc/
*/
int Misc::getFov(int defaultFOV, int resX, int resY)
{
	float aspect = (float)resX/(float)resY;
	float fov = (float) (atan(tan(defaultFOV*PI/360.0)*(aspect/(4.0/3.0)))*360.0)/PI;
	return (int) (fov + 0.5f);	
}