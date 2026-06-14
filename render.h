#ifndef INVESTIGATOR_RENDER_H
#define INVESTIGATOR_RENDER_H

#include "common.h"

void transformPoint(const Vec3 *v, double camX,double camY,double camZ,double ang, double *tx,double *ty,double *tz);
int project(const Vec3 *v, double camX,double camY,double camZ,double ang, int cx,int cy, int *sx,int *sy);
void drawWire(HDC dc, const Vec3 *verts, int vertCount, int edges[][2], int edgeCount, double ox,double oy,double oz, double camX,double camY,double camZ,double ang, int cx,int cy, double rotX_deg, double rotY_deg, double rotZ_deg);

// primitive shapes exported
extern Vec3 cubeVerts[];
extern int cubeEdges[][2];
extern int cubeEdgeCount;
extern int cubeVertCount;
extern Vec3 pyramidVerts[];
extern int pyramidEdges[][2];
extern int pyramidEdgeCount;
extern int pyramidVertCount;

#endif // INVESTIGATOR_RENDER_H
