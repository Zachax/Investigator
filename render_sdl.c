#include "common.h"
#include "render.h"

// SDL-compatible draw helpers
static void unpackColor(COLORREF c, Uint8 *r, Uint8 *g, Uint8 *b) {
    *r = (c >> 16) & 0xFF;
    *g = (c >> 8) & 0xFF;
    *b = c & 0xFF;
}

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

        SDL_SetRenderDrawColor(dc, 255,255,255,255);
        SDL_RenderDrawLine(dc, ax, ay, bx, by);
    }
}
