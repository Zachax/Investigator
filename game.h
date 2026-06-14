#ifndef INVESTIGATOR_GAME_H
#define INVESTIGATOR_GAME_H

#include "common.h"

// Collision detection
int detectCollision(double camX,double camY,double camZ,double playerHitbox, MapObject **out, int *outIndex);

// Handle teleport activation when colliding; may modify camera and map state.
void handleTeleportIfNeeded(MapObject **collided, double *camX,double *camY,double *camZ,double playerHitbox);

// Update dynamic objects (movement/AI)
void updateObjects(double dt, double playerX, double playerY, double playerZ);

#endif // INVESTIGATOR_GAME_H
