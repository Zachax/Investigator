#ifndef INVESTIGATOR_INPUT_H
#define INVESTIGATOR_INPUT_H

#include "common.h"

// Process keyboard input; returns 1 if map reload requested (L pressed once)
int handleInput(double *camX,double *camY,double *camZ,double *ang,double speed,double rotSpeed,double *outSpeedCurr,int *lastLState,int *outShootPressed,int *lastShootState);

#endif // INVESTIGATOR_INPUT_H
