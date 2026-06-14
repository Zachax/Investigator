#include "input.h"

int handleInput(double *camX,double *camY,double *camZ,double *ang,double speed,double rotSpeed,double *outSpeedCurr,int *lastLState){
    double fx = sin(*ang);
    double fz = cos(*ang);
    double rx = cos(*ang);
    double rz = -sin(*ang);

    int moveForward = (GetAsyncKeyState(VK_UP) & 0x8000) || (GetAsyncKeyState('W') & 0x8000);
    int moveBack    = (GetAsyncKeyState(VK_DOWN) & 0x8000) || (GetAsyncKeyState('S') & 0x8000);
    int turnLeft    = (GetAsyncKeyState(VK_LEFT) & 0x8000) || (GetAsyncKeyState('A') & 0x8000);
    int turnRight   = (GetAsyncKeyState(VK_RIGHT) & 0x8000) || (GetAsyncKeyState('D') & 0x8000);
    int strafeLeft  = (GetAsyncKeyState('Z') & 0x8000);
    int strafeRight = (GetAsyncKeyState('C') & 0x8000);

    int shiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    double speedCurr = speed * (shiftHeld ? 2.0 : 1.0);
    *outSpeedCurr = speedCurr;

    if (moveForward) { *camX += fx * speedCurr; *camZ += fz * speedCurr; }
    if (moveBack)    { *camX -= fx * speedCurr; *camZ -= fz * speedCurr; }
    if (turnLeft)    { *ang -= rotSpeed; }
    if (turnRight)   { *ang += rotSpeed; }
    if (strafeLeft)  { *camX -= rx * speedCurr; *camZ -= rz * speedCurr; }
    if (strafeRight) { *camX += rx * speedCurr; *camZ += rz * speedCurr; }

    int lState = (GetAsyncKeyState('L') & 0x8000) != 0;
    int reload = 0;
    if (lState && !(*lastLState)) reload = 1;
    *lastLState = lState;
    return reload;
}
