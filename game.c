#include "game.h"
#include <math.h>
#include <stdlib.h>

Projectile projectiles[MAX_PROJECTILES];
char shotDebugText[256] = "";

int detectCollision(double camX,double camY,double camZ,double playerHitbox, MapObject **out, int *outIndex){
    MapObject *collided = NULL; int collIndex = -1;
    for (int mi=0; mi<mapObjectCount; ++mi){
        MapObject *mo = &mapObjects[mi];
        if (mo->hitboxRadius <= 0.0) continue;
        double dx = mo->x - camX;
        double dy = mo->y - camY;
        double dz = mo->z - camZ;
        double dist = sqrt(dx*dx + dy*dy + dz*dz);
        if (dist <= (mo->hitboxRadius + playerHitbox)) { collided = mo; collIndex = mi; break; }
    }
    if (out) *out = collided;
    if (outIndex) *outIndex = collIndex;
    return (collided != NULL);
}

void updateObjects(double dt, double playerX, double playerY, double playerZ) {
    if (dt <= 0.0) return;
    for (int i=0;i<mapObjectCount;i++){
        MapObject *mo = &mapObjects[i];
        if (mo->movementType == 1) {
            /* wandering */
            mo->wanderTimer -= dt;
            if (mo->wanderTimer <= 0.0) {
                mo->wanderAngle = ((double)rand() / RAND_MAX) * 2.0 * PI;
                mo->wanderInterval = 0.5 + ((double)rand() / RAND_MAX) * 2.5;
                mo->wanderTimer = mo->wanderInterval;
            }
            double step = mo->moveSpeed * dt;
            double nx = cos(mo->wanderAngle) * step;
            double nz = sin(mo->wanderAngle) * step;
            double newx = mo->x + nx;
            double newz = mo->z + nz;
            double dx = newx - mo->homeX;
            double dz = newz - mo->homeZ;
            double dist = sqrt(dx*dx + dz*dz);
            if (dist > mo->maxDistFromHome) {
                /* turn back towards home */
                double ang = atan2(mo->homeZ - mo->z, mo->homeX - mo->x);
                mo->wanderAngle = ang;
                newx = mo->x + cos(mo->wanderAngle) * step;
                newz = mo->z + sin(mo->wanderAngle) * step;
            }
            mo->x = newx; mo->z = newz;
            /* update desired yaw to face movement and interpolate turning */
            double desired = mo->wanderAngle * (180.0 / PI);
            mo->desiredRy = desired;
            /* normalize angle difference to [-180,180] */
            double diff = mo->desiredRy - mo->ry;
            while (diff > 180.0) diff -= 360.0;
            while (diff < -180.0) diff += 360.0;
            double maxTurn = mo->turnSpeed * dt;
            if (diff > maxTurn) diff = maxTurn;
            if (diff < -maxTurn) diff = -maxTurn;
            mo->ry += diff;
        } else if (mo->movementType == 2) {
            /* chase player (towards player's X,Z) without changing orientation */
            double dx = playerX - mo->x;
            double dz = playerZ - mo->z;
            double dist = sqrt(dx*dx + dz*dz);
            if (dist > 0.001) {
                double nx = dx / dist;
                double nz = dz / dist;
                double step = mo->moveSpeed * dt;
                if (step > dist) step = dist;
                mo->x += nx * step;
                mo->z += nz * step;
            }
        }
    }
}

void spawnPlayerShot(double camX, double camY, double camZ, double ang) {
    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        Projectile *pr = &projectiles[i];
        if (pr->active) continue;
        pr->active = 1;
        pr->x = camX + sin(ang) * 0.25;
        pr->y = camY;
        pr->z = camZ + cos(ang) * 0.25;
        pr->dirX = sin(ang);
        pr->dirY = 0.0;
        pr->dirZ = cos(ang);
        pr->speed = 8.0;
        pr->life = 6.0;
        pr->spin = 0.0;
        pr->hitboxRadius = 0.2;
        pr->hitSomething = 0;
        pr->debugMessage[0] = '\0';
        shotDebugText[0] = '\0';
        break;
    }
}

void updateProjectiles(double dt, double camX, double camY, double camZ) {
    if (dt <= 0.0) return;
    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        Projectile *pr = &projectiles[i];
        if (!pr->active) continue;
        if (pr->hitSomething) {
            pr->active = 0;
            continue;
        }

        pr->x += pr->dirX * pr->speed * dt;
        pr->z += pr->dirZ * pr->speed * dt;
        pr->spin += dt * 720.0;
        pr->life -= dt;

        double dx = pr->x - camX;
        double dy = pr->y - camY;
        double dz = pr->z - camZ;
        double dist = sqrt(dx*dx + dy*dy + dz*dz);
        if (pr->life <= 0.0 || dist > 80.0) {
            pr->active = 0;
            continue;
        }

        for (int mi = 0; mi < mapObjectCount; ++mi) {
            MapObject *mo = &mapObjects[mi];
            if (mo->hitboxRadius <= 0.0) continue;
            double mdx = mo->x - pr->x;
            double mdy = mo->y - pr->y;
            double mdz = mo->z - pr->z;
            double mdist = sqrt(mdx*mdx + mdy*mdy + mdz*mdz);
            if (mdist <= (mo->hitboxRadius + pr->hitboxRadius)) {
                pr->hitSomething = 1;
                pr->active = 0;
                snprintf(pr->debugMessage, sizeof(pr->debugMessage), "Shot hit: %s", mo->name);
                snprintf(shotDebugText, sizeof(shotDebugText), "%s", pr->debugMessage);
                break;
            }
        }
    }
}

void handleTeleportIfNeeded(MapObject **collided, double *camX,double *camY,double *camZ,double playerHitbox){
    MapObject *c = (collided?*collided:NULL);
    if (c && c->hitboxRadius > 0.0 && c->isTeleporter) {
        if (canUseTeleport) {
            double oldX = *camX, oldY = *camY, oldZ = *camZ;
            if (c->tp_targetMap[0] == '\0') {
                if (c->tp_mode == 0) {
                    *camX = c->tp_x; *camY = c->tp_y; *camZ = c->tp_z;
                    if (*camY < 0.5) *camY = 1.0;
                } else if (c->tp_mode == 1) {
                    int si = findSpawnIndex(c->tp_spawnName);
                    if (si >= 0) { *camX = spawnPoints[si].x; *camY = spawnPoints[si].y; *camZ = spawnPoints[si].z; }
                    else if (hasDefaultSpawn) { *camX = defaultSpawnX; *camY = defaultSpawnY; *camZ = defaultSpawnZ; }
                    if (*camY < 0.5) *camY = 1.0;
                } else if (c->tp_mode == 2) {
                    if (hasDefaultSpawn) { *camX = defaultSpawnX; *camY = defaultSpawnY; *camZ = defaultSpawnZ; }
                    if (*camY < 0.5) *camY = 1.0;
                } else if (c->tp_mode == 3) {
                    // same coords - no-op
                }
                int foundIndex = -1;
                for (int i=0;i<mapObjectCount;i++){
                    MapObject *m = &mapObjects[i];
                    if (!m->isTeleporter || m->hitboxRadius <= 0.0) continue;
                    double dx = m->x - *camX; double dy = m->y - *camY; double dz = m->z - *camZ;
                    double d = sqrt(dx*dx + dy*dy + dz*dz);
                    if (d <= (m->hitboxRadius + playerHitbox + 0.001)) { foundIndex = i; break; }
                }
                if (foundIndex >= 0) { lastTeleIndex = foundIndex; canUseTeleport = 0; }
                else { lastTeleIndex = -1; canUseTeleport = 1; }
            } else {
                char prevMap[256]; strncpy(prevMap, currentMapFile, sizeof(prevMap)-1); prevMap[sizeof(prevMap)-1]=0;
                strncpy(currentMapFile, c->tp_targetMap, sizeof(currentMapFile)-1); currentMapFile[sizeof(currentMapFile)-1]=0;
                loadMap(currentMapFile);
                if (c->tp_mode == 0) {
                    *camX = c->tp_x; *camY = c->tp_y; *camZ = c->tp_z;
                    if (*camY < 0.5) *camY = 1.0;
                } else if (c->tp_mode == 1) {
                    int si = findSpawnIndex(c->tp_spawnName);
                    if (si >= 0) { *camX = spawnPoints[si].x; *camY = spawnPoints[si].y; *camZ = spawnPoints[si].z; }
                    else if (hasDefaultSpawn) { *camX = defaultSpawnX; *camY = defaultSpawnY; *camZ = defaultSpawnZ; }
                    else { *camX = oldX; *camY = oldY; *camZ = oldZ; }
                    if (*camY < 0.5) *camY = 1.0;
                } else if (c->tp_mode == 2) {
                    if (hasDefaultSpawn) { *camX = defaultSpawnX; *camY = defaultSpawnY; *camZ = defaultSpawnZ; }
                    else { *camX = oldX; *camY = oldY; *camZ = oldZ; }
                    if (*camY < 0.5) *camY = 1.0;
                } else if (c->tp_mode == 3) {
                    *camX = oldX; *camY = oldY; *camZ = oldZ;
                }
                int foundIndex = -1;
                for (int i=0;i<mapObjectCount;i++){
                    MapObject *m = &mapObjects[i];
                    if (!m->isTeleporter || m->hitboxRadius <= 0.0) continue;
                    double dx = m->x - *camX; double dy = m->y - *camY; double dz = m->z - *camZ;
                    double d = sqrt(dx*dx + dy*dy + dz*dz);
                    if (d <= (m->hitboxRadius + playerHitbox + 0.001)) { foundIndex = i; break; }
                }
                if (foundIndex >= 0) { lastTeleIndex = foundIndex; canUseTeleport = 0; }
                else { lastTeleIndex = -1; canUseTeleport = 1; }
            }
            if (collided) *collided = NULL;
        }
    }
    if (lastTeleIndex >= 0) {
        if (lastTeleIndex < mapObjectCount) {
            MapObject *lt = &mapObjects[lastTeleIndex];
            double dx = lt->x - *camX; double dy = lt->y - *camY; double dz = lt->z - *camZ;
            double d = sqrt(dx*dx + dy*dy + dz*dz);
            if (d > (lt->hitboxRadius + playerHitbox)) { canUseTeleport = 1; lastTeleIndex = -1; }
        } else {
            lastTeleIndex = -1; canUseTeleport = 1;
        }
    }
}
