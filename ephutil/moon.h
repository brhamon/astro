#include <novas.h>
#include "ephutil.h"

#define NBR_OF_MOON_PHASES 8

const char *moon_phase_names[NBR_OF_MOON_PHASES];

short int moon_phase(time_parameters_t* tp,
        object* sun, object* moon, short int accuracy,
        double* phlat, double* phlon, int* phindex);
