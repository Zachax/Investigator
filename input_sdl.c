#include "input.h"
#include <SDL2/SDL.h>

int handleInput(double *camX,double *camY,double *camZ,double *ang,double speed,double rotSpeed,double *outSpeedCurr,int *lastLState){
    const Uint8 *state = SDL_GetKeyboardState(NULL);
    double fx = sin(*ang);
    double fz = cos(*ang);
    double rx = cos(*ang);
    double rz = -sin(*ang);

    int moveForward = state[SDL_SCANCODE_W] || state[SDL_SCANCODE_UP];
    int moveBack = state[SDL_SCANCODE_S] || state[SDL_SCANCODE_DOWN];
    int turnLeft = state[SDL_SCANCODE_A] || state[SDL_SCANCODE_LEFT];
    int turnRight = state[SDL_SCANCODE_D] || state[SDL_SCANCODE_RIGHT];
    int strafeLeft = state[SDL_SCANCODE_Z];
    int strafeRight = state[SDL_SCANCODE_C];

    int shiftHeld = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
    double speedCurr = speed * (shiftHeld ? 2.0 : 1.0);
    *outSpeedCurr = speedCurr;

    if (moveForward) { *camX += fx * speedCurr; *camZ += fz * speedCurr; }
    if (moveBack)    { *camX -= fx * speedCurr; *camZ -= fz * speedCurr; }
    if (turnLeft)    { *ang -= rotSpeed; }
    if (turnRight)   { *ang += rotSpeed; }
    if (strafeLeft)  { *camX -= rx * speedCurr; *camZ -= rz * speedCurr; }
    if (strafeRight) { *camX += rx * speedCurr; *camZ += rz * speedCurr; }

    int lState = state[SDL_SCANCODE_L];
    int reload = 0;
    if (lState && !(*lastLState)) reload = 1;
    *lastLState = lState;
    return reload;
}
