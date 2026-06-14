#include "render.h"

// Simple shapes: cube and pyramid
Vec3 cubeVerts[] = {
    {-1,-1, 1}, {1,-1, 1}, {1,1,1}, {-1,1,1}, // front
    {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1} // back
};
int cubeEdges[][2] = {
    {0,1},{1,2},{2,3},{3,0}, // front
    {4,5},{5,6},{6,7},{7,4}, // back
    {0,4},{1,5},{2,6},{3,7}  // connect
};
int cubeEdgeCount = sizeof(cubeEdges)/sizeof(cubeEdges[0]);
int cubeVertCount = sizeof(cubeVerts)/sizeof(Vec3);

Vec3 pyramidVerts[] = {
    {0,1.2,0}, {-1,-1,1}, {1,-1,1}, {1,-1,-1}, {-1,-1,-1}
};
int pyramidEdges[][2] = {
    {0,1},{0,2},{0,3},{0,4},
    {1,2},{2,3},{3,4},{4,1}
};
int pyramidEdgeCount = sizeof(pyramidEdges)/sizeof(pyramidEdges[0]);
int pyramidVertCount = sizeof(pyramidVerts)/sizeof(Vec3);

void transformPoint(const Vec3 *v, double camX,double camY,double camZ,double ang, double *tx,double *ty,double *tz){
    double dx = v->x - camX;
    double dy = v->y - camY;
    double dz = v->z - camZ;
    double ca = cos(ang), sa = sin(ang);
    *tx = ca*dx - sa*dz;
    *tz = sa*dx + ca*dz;
    *ty = dy;
}

int project(const Vec3 *v, double camX,double camY,double camZ,double ang, int cx,int cy, int *sx,int *sy) {
    double tx,ty,tz;
    transformPoint(v, camX,camY,camZ, ang, &tx,&ty,&tz);
    if (tz <= 0.05) return 0;
    *sx = cx + (int)( (tx * FOV_SCALE) / tz );
    *sy = cy - (int)( (ty * FOV_SCALE) / tz );
    return 1;
}

#ifdef _WIN32
void drawWire(HDC dc, const Vec3 *verts, int vertCount, int edges[][2], int edgeCount, double ox,double oy,double oz, double camX,double camY,double camZ,double ang, int cx,int cy, double rotX_deg, double rotY_deg, double rotZ_deg) {
    const double nearPlane = 0.05;
    double rx = rotX_deg * (PI/180.0);
    double ry = rotY_deg * (PI/180.0);
    double rz = rotZ_deg * (PI/180.0);
    double crx = cos(rx), srx = sin(rx);
    double cry = cos(ry), sry = sin(ry);
    double crz = cos(rz), srz = sin(rz);
    for (int i=0;i<edgeCount;i++){
        int a = edges[i][0];
        int b = edges[i][1];
        double vx_a = verts[a].x;
        double vy_a = verts[a].y;
        double vz_a = verts[a].z;
        double vy_a1 = vy_a*crx - vz_a*srx;
        double vz_a1 = vy_a*srx + vz_a*crx;
        double vx_a2 = vx_a*cry + vz_a1*sry;
        double vz_a2 = -vx_a*sry + vz_a1*cry;
        double vx_a3 = vx_a2*crz - vy_a1*srz;
        double vy_a3 = vx_a2*srz + vy_a1*crz;
        Vec3 va = { vx_a3 + ox, vy_a3 + oy, vz_a2 + oz };

        double vx_b = verts[b].x;
        double vy_b = verts[b].y;
        double vz_b = verts[b].z;
        double vy_b1 = vy_b*crx - vz_b*srx;
        double vz_b1 = vy_b*srx + vz_b*crx;
        double vx_b2 = vx_b*cry + vz_b1*sry;
        double vz_b2 = -vx_b*sry + vz_b1*cry;
        double vx_b3 = vx_b2*crz - vy_b1*srz;
        double vy_b3 = vx_b2*srz + vy_b1*crz;
        Vec3 vb = { vx_b3 + ox, vy_b3 + oy, vz_b2 + oz };

        double atx,aty,atz, btx,bty,btz;
        transformPoint(&va, camX,camY,camZ, ang, &atx,&aty,&atz);
        transformPoint(&vb, camX,camY,camZ, ang, &btx,&bty,&btz);

        if (atz <= nearPlane && btz <= nearPlane) continue;
        if (atz <= nearPlane || btz <= nearPlane) {
            double t = 0.0;
            if ((btz - atz) != 0.0) t = (nearPlane - atz) / (btz - atz);
            if (atz < btz) {
                atx = atx + (btx - atx) * t;
                aty = aty + (bty - aty) * t;
                atz = nearPlane;
            } else {
                btx = atx + (btx - atx) * t;
                bty = aty + (bty - aty) * t;
                btz = nearPlane;
            }
        }

        int ax = cx + (int)((atx * FOV_SCALE) / atz);
        int ay = cy - (int)((aty * FOV_SCALE) / atz);
        int bx = cx + (int)((btx * FOV_SCALE) / btz);
        int by = cy - (int)((bty * FOV_SCALE) / btz);

        MoveToEx(dc, ax, ay, NULL);
        LineTo(dc, bx, by);
    }
}

    #endif // _WIN32
