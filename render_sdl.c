#include "common.h"
#include "render.h"

// SDL-compatible draw helpers
static void unpackColor(COLORREF c, Uint8 *r, Uint8 *g, Uint8 *b) {
    *r = (c >> 16) & 0xFF;
    *g = (c >> 8) & 0xFF;
    *b = c & 0xFF;
}

static int shouldDrawFaceSDL(const Vec3 *verts, int vertCount, const ShapeFace *face, double ox,double oy,double oz, double camX,double camY,double camZ,double rotX_deg,double rotY_deg,double rotZ_deg) {
    if (!face || face->vertexCount < 3 || !face->vertexIndices) return 0;
    double rx = rotX_deg * (PI/180.0);
    double ry = rotY_deg * (PI/180.0);
    double rz = rotZ_deg * (PI/180.0);
    double crx = cos(rx), srx = sin(rx);
    double cry = cos(ry), sry = sin(ry);
    double crz = cos(rz), srz = sin(rz);

    struct { double x, y, z; } facePts[16];
    int faceCount = 0;

    for (int i = 0; i < face->vertexCount && faceCount < 16; ++i) {
        int vi = face->vertexIndices[i];
        if (vi < 0 || vi >= vertCount) continue;
        double vx = verts[vi].x;
        double vy = verts[vi].y;
        double vz = verts[vi].z;
        double vy1 = vy*crx - vz*srx;
        double vz1 = vy*srx + vz*crx;
        double vx2 = vx*cry + vz1*sry;
        double vz2 = -vx*sry + vz1*cry;
        double vx3 = vx2*crz - vy1*srz;
        double vy3 = vx2*srz + vy1*crz;
        facePts[faceCount].x = vx3 + ox;
        facePts[faceCount].y = vy3 + oy;
        facePts[faceCount].z = vz2 + oz;
        faceCount++;
    }
    if (faceCount < 3) return 0;

    double v1x = facePts[1].x - facePts[0].x;
    double v1y = facePts[1].y - facePts[0].y;
    double v1z = facePts[1].z - facePts[0].z;
    double v2x = facePts[2].x - facePts[0].x;
    double v2y = facePts[2].y - facePts[0].y;
    double v2z = facePts[2].z - facePts[0].z;

    double nx = v1y * v2z - v1z * v2y;
    double ny = v1z * v2x - v1x * v2z;
    double nz = v1x * v2y - v1y * v2x;

    double cx = 0.0, cy = 0.0, cz = 0.0;
    for (int i = 0; i < faceCount; ++i) {
        cx += facePts[i].x;
        cy += facePts[i].y;
        cz += facePts[i].z;
    }
    cx /= faceCount; cy /= faceCount; cz /= faceCount;

    double viewX = camX - cx;
    double viewY = camY - cy;
    double viewZ = camZ - cz;

    return (nx * viewX + ny * viewY + nz * viewZ) < 0.0;
}

static void drawFilledFaceSDL(SDL_Renderer *dc, const Vec3 *verts, int vertCount, const ShapeFace *face, double ox,double oy,double oz, double camX,double camY,double camZ,double ang,int cx,int cy,double rotX_deg,double rotY_deg,double rotZ_deg,COLORREF color) {
    if (!dc || !face || face->vertexCount < 3 || !face->vertexIndices || !color) return;
    const double nearPlane = 0.05;
    double rx = rotX_deg * (PI/180.0);
    double ry = rotY_deg * (PI/180.0);
    double rz = rotZ_deg * (PI/180.0);
    double crx = cos(rx), srx = sin(rx);
    double cry = cos(ry), sry = sin(ry);
    double crz = cos(rz), srz = sin(rz);
    struct { int x; int y; } pts[16];
    int count = 0;
    for (int i = 0; i < face->vertexCount && count < 16; ++i) {
        int vi = face->vertexIndices[i];
        if (vi < 0 || vi >= vertCount) continue;
        double vx = verts[vi].x;
        double vy = verts[vi].y;
        double vz = verts[vi].z;
        double vy1 = vy*crx - vz*srx;
        double vz1 = vy*srx + vz*crx;
        double vx2 = vx*cry + vz1*sry;
        double vz2 = -vx*sry + vz1*cry;
        double vx3 = vx2*crz - vy1*srz;
        double vy3 = vx2*srz + vy1*crz;
        Vec3 v = { vx3 + ox, vy3 + oy, vz2 + oz };
        double tx,ty,tz;
        transformPoint(&v, camX,camY,camZ, ang, &tx,&ty,&tz);
        if (tz <= nearPlane) return;
        pts[count].x = cx + (int)((tx * FOV_SCALE) / tz);
        pts[count].y = cy - (int)((ty * FOV_SCALE) / tz);
        count++;
    }
    if (count < 3) return;
    int minY = pts[0].y, maxY = pts[0].y;
    for (int i = 1; i < count; ++i) { if (pts[i].y < minY) minY = pts[i].y; if (pts[i].y > maxY) maxY = pts[i].y; }
    Uint8 r, g, b;
    unpackColor(color, &r, &g, &b);
    SDL_SetRenderDrawColor(dc, r, g, b, 180);
    for (int y = minY; y <= maxY; ++y) {
        int xs[16]; int xc = 0;
        for (int i = 0; i < count; ++i) {
            int j = (i + 1) % count;
            int y1 = pts[i].y;
            int y2 = pts[j].y;
            if (y1 == y2) continue;
            if ((y >= y1 && y <= y2) || (y >= y2 && y <= y1)) {
                int x1 = pts[i].x;
                int x2 = pts[j].x;
                int x = x1 + (y - y1) * (x2 - x1) / (y2 - y1);
                xs[xc++] = x;
            }
        }
        if (xc < 2) continue;
        for (int i = 0; i < xc - 1; ++i) {
            for (int j = i + 1; j < xc; ++j) {
                if (xs[i] > xs[j]) { int t = xs[i]; xs[i] = xs[j]; xs[j] = t; }
            }
        }
        for (int i = 0; i + 1 < xc; i += 2) {
            SDL_RenderDrawLine(dc, xs[i], y, xs[i + 1], y);
        }
    }
}

void drawWire(HDC dc, const Vec3 *verts, int vertCount, int edges[][2], int edgeCount, double ox,double oy,double oz, double camX,double camY,double camZ,double ang, int cx,int cy, double rotX_deg, double rotY_deg, double rotZ_deg, const ShapeFace *faces, int faceCount, COLORREF lineColor, COLORREF fillColor, int hasFillColor) {
    const double nearPlane = 0.05;
    double rx = rotX_deg * (PI/180.0);
    double ry = rotY_deg * (PI/180.0);
    double rz = rotZ_deg * (PI/180.0);
    double crx = cos(rx), srx = sin(rx);
    double cry = cos(ry), sry = sin(ry);
    double crz = cos(rz), srz = sin(rz);
    if (faceCount > 0 && faces) {
        for (int i = 0; i < faceCount; ++i) {
            const ShapeFace *face = &faces[i];
            COLORREF faceColor = face->hasColor ? face->color : (hasFillColor ? fillColor : 0);
            if (faceColor && shouldDrawFaceSDL(verts, vertCount, face, ox, oy, oz, camX,camY,camZ, rotX_deg, rotY_deg, rotZ_deg)) {
                drawFilledFaceSDL(dc, verts, vertCount, face, ox, oy, oz, camX,camY,camZ, ang, cx, cy, rotX_deg, rotY_deg, rotZ_deg, faceColor);
            }
        }
    }
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
