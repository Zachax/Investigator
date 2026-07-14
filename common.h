#ifndef INVESTIGATOR_COMMON_H
#define INVESTIGATOR_COMMON_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <SDL2/SDL.h>
#endif

#ifndef _WIN32
// Minimal compatibility for Windows GDI types used in code
typedef unsigned int COLORREF;
#define RGB(r,g,b) (((r)<<16) | ((g)<<8) | (b))
#include <strings.h>
// case-insensitive compare aliases
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#endif

#define WIN_W 800
#define WIN_H 600
#define FOV_SCALE 300.0
#define PI 3.14159265358979323846
#define MAX_PROJECTILES 8

typedef struct { double x,y,z; } Vec3;

// Map object types
#define OBJ_CUBE 1
#define OBJ_PYRAMID 2
#define OBJ_CUSTOM 3

typedef struct {
    int type;
    double x,y,z;
    double rx,ry,rz;
    int shapeIndex;
    COLORREF color;
    COLORREF fillColor;
    int hasFillColor;
    char name[64];
    double hitboxRadius;
    int isTeleporter;
    char tp_targetMap[256];
    int tp_mode;
    char tp_spawnName[64];
    double tp_x, tp_y, tp_z;
    /* Movement / AI */
    double moveSpeed;       /* units per second */
    int movementType;       /* 0=none,1=wandering,2=chase-player */
    double homeX, homeY, homeZ; /* starting position for bounded wandering */
    double wanderAngle;     /* current wander direction (radians) */
    double wanderTimer;     /* seconds until next direction change */
    double wanderInterval;  /* base interval */
    double maxDistFromHome; /* maximum allowed distance from home */
    double turnSpeed;       /* degrees per second for yaw turning */
    double desiredRy;       /* desired yaw (degrees) to interpolate towards */
} MapObject;

typedef struct {
    int vertexCount;
    int *vertexIndices;
    COLORREF color;
    int hasColor;
} ShapeFace;

typedef struct {
    char name[64];
    Vec3 *verts;
    int vertCount;
    int (*edges)[2];
    int edgeCount;
    ShapeFace *faces;
    int faceCount;
} CustomShape;

typedef struct {
    int active;
    double x,y,z;
    double dirX,dirY,dirZ;
    double speed;
    double life;
    double spin;
    double hitboxRadius;
    int hitSomething;
    char debugMessage[128];
} Projectile;

typedef struct { char name[64]; double x,y,z; } SpawnPoint;

// Map/global storage (managed by map.c)
extern MapObject *mapObjects;
extern int mapObjectCount;
extern CustomShape *customShapes;
extern int customShapeCount;
extern SpawnPoint *spawnPoints;
extern int spawnPointCount;
extern int hasDefaultSpawn;
extern double defaultSpawnX, defaultSpawnY, defaultSpawnZ;

// Teleport state
extern int canUseTeleport;
extern char currentMapFile[256];
extern int lastTeleIndex;
extern Projectile projectiles[MAX_PROJECTILES];
extern char shotDebugText[256];

// Default rendering color
extern COLORREF defaultColor;

#ifndef _WIN32
// On non-Windows builds, alias HDC to SDL_Renderer* so existing render APIs keep the same signature.
typedef SDL_Renderer* HDC;
#define SetTextColor(renderer,col) ((void)0)
#endif

// Map APIs
int loadMap(const char *filename);
void freeMap(void);
void clearMapObjectsAndSpawns(void);
int findSpawnIndex(const char *name);
int findCustomShape(const char *name);
void addCustomShape(const char *name, Vec3 *verts, int vcount, int (*edges)[2], int ecount, ShapeFace *faces, int faceCount);

#endif // INVESTIGATOR_COMMON_H
