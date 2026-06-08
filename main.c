#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WIN_W 800
#define WIN_H 600
#define FOV_SCALE 300.0
#define PI 3.14159265358979323846

typedef struct { double x,y,z; } Vec3;

// Simple shapes: cube and pyramid
static Vec3 cubeVerts[] = {
    {-1,-1, 1}, {1,-1, 1}, {1,1,1}, {-1,1,1}, // front
    {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1} // back
};
static int cubeEdges[][2] = {
    {0,1},{1,2},{2,3},{3,0}, // front
    {4,5},{5,6},{6,7},{7,4}, // back
    {0,4},{1,5},{2,6},{3,7}  // connect
};
static int cubeEdgeCount = sizeof(cubeEdges)/sizeof(cubeEdges[0]);

static Vec3 pyramidVerts[] = {
    {0,1.2,0}, {-1,-1,1}, {1,-1,1}, {1,-1,-1}, {-1,-1,-1}
};
static int pyramidEdges[][2] = {
    {0,1},{0,2},{0,3},{0,4},
    {1,2},{2,3},{3,4},{4,1}
};
static int pyramidEdgeCount = sizeof(pyramidEdges)/sizeof(pyramidEdges[0]);

// transform a world point into camera space (rotated & translated)
static void transformPoint(const Vec3 *v, double camX,double camY,double camZ,double ang, double *tx,double *ty,double *tz){
    double dx = v->x - camX;
    double dy = v->y - camY;
    double dz = v->z - camZ;
    double ca = cos(ang), sa = sin(ang);
    *tx = ca*dx - sa*dz;
    *tz = sa*dx + ca*dz;
    *ty = dy;
}

// project a single world point (returns 0 if behind near plane)
static int project(const Vec3 *v, double camX,double camY,double camZ,double ang, int cx,int cy, int *sx,int *sy) {
    double tx,ty,tz;
    transformPoint(v, camX,camY,camZ, ang, &tx,&ty,&tz);
    if (tz <= 0.05) return 0; // behind camera
    *sx = cx + (int)( (tx * FOV_SCALE) / tz );
    *sy = cy - (int)( (ty * FOV_SCALE) / tz );
    return 1;
}

// draw wireframe for a shape with position offset, with near-plane clipping
static void drawWire(HDC dc, const Vec3 *verts, int vertCount, int edges[][2], int edgeCount, double ox,double oy,double oz, double camX,double camY,double camZ,double ang, int cx,int cy) {
    const double nearPlane = 0.05;
    for (int i=0;i<edgeCount;i++){
        int a = edges[i][0];
        int b = edges[i][1];
        Vec3 va = { verts[a].x + ox, verts[a].y + oy, verts[a].z + oz };
        Vec3 vb = { verts[b].x + ox, verts[b].y + oy, verts[b].z + oz };

        double atx,aty,atz, btx,bty,btz;
        transformPoint(&va, camX,camY,camZ, ang, &atx,&aty,&atz);
        transformPoint(&vb, camX,camY,camZ, ang, &btx,&bty,&btz);

        // clip against near plane (simple linear interpolation)
        if (atz <= nearPlane && btz <= nearPlane) continue; // fully behind
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

        // project both transformed points (may result off-screen coordinates)
        int ax = cx + (int)((atx * FOV_SCALE) / atz);
        int ay = cy - (int)((aty * FOV_SCALE) / atz);
        int bx = cx + (int)((btx * FOV_SCALE) / btz);
        int by = cy - (int)((bty * FOV_SCALE) / btz);

        MoveToEx(dc, ax, ay, NULL);
        LineTo(dc, bx, by);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg){
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd,msg,wParam,lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmd, int nCmdShow){
    const char *clsName = "InvestigatorClass";
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = clsName;
    wc.hbrBackground = NULL;
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, clsName, "Investigator - Wireframe Demo", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,CW_USEDEFAULT, WIN_W, WIN_H, NULL,NULL,hInstance,NULL);
    if (!hwnd) return 0;

    // camera
    double camX = 0.0, camY = 1.2, camZ = -6.0;
    double ang = 0.0; // yaw
    double speed = 0.08;
    double rotSpeed = 0.04;

    // place objects on the map
    double cubeX = 0.0, cubeY = 0.0, cubeZ = 8.0;
    double pyrX = 3.5, pyrY = 0.0, pyrZ = 12.0;

    // main loop
    MSG msg;
    PeekMessage(&msg,NULL,0,0,PM_NOREMOVE);
    int cx = WIN_W/2, cy = WIN_H/2;

    while (1){
        while (PeekMessage(&msg,NULL,0,0,PM_REMOVE)){
            if (msg.message == WM_QUIT) return (int)msg.wParam;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // input (keyboard) - consolidated mapping
        // KEYS (do not change): W = forward, A = back, S = turn left, D = turn right
        // Z = strafe left, C = strafe right. Arrow keys provide equivalent forward/turn controls.
        double fx = sin(ang);
        double fz = cos(ang);
        double rx = cos(ang);
        double rz = -sin(ang);

        int moveForward = (GetAsyncKeyState(VK_UP) & 0x8000) || (GetAsyncKeyState('W') & 0x8000);
        int moveBack    = (GetAsyncKeyState(VK_DOWN) & 0x8000) || (GetAsyncKeyState('S') & 0x8000);
        int turnLeft    = (GetAsyncKeyState(VK_LEFT) & 0x8000) || (GetAsyncKeyState('A') & 0x8000);
        int turnRight   = (GetAsyncKeyState(VK_RIGHT) & 0x8000) || (GetAsyncKeyState('D') & 0x8000);
        int strafeLeft  = (GetAsyncKeyState('Z') & 0x8000);
        int strafeRight = (GetAsyncKeyState('C') & 0x8000);

        if (moveForward) { camX += fx * speed; camZ += fz * speed; }
        if (moveBack)    { camX -= fx * speed; camZ -= fz * speed; }
        if (turnLeft)    { ang -= rotSpeed; }
        if (turnRight)   { ang += rotSpeed; }
        if (strafeLeft)  { camX -= rx * speed; camZ -= rz * speed; }
        if (strafeRight) { camX += rx * speed; camZ += rz * speed; }

        // rendering
        HDC hdc = GetDC(hwnd);
        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, WIN_W, WIN_H);
        HGDIOBJ old = SelectObject(mem, bmp);

        // sky and ground split with horizon that reflects camera height
        int horizon = cy - (int)( (camY - 1.0) * 40 );
        RECT r = {0,0,WIN_W, WIN_H};
        HBRUSH sky = CreateSolidBrush(RGB(100,160,240));
        FillRect(mem, &r, sky);
        DeleteObject(sky);
        RECT ground = {0,horizon,WIN_W,WIN_H};
        HBRUSH groundb = CreateSolidBrush(RGB(80,140,60));
        FillRect(mem, &ground, groundb);
        DeleteObject(groundb);

        // draw horizon line
        HPEN penH = CreatePen(PS_SOLID, 2, RGB(200,200,200));
        HPEN oldPen = SelectObject(mem, penH);
        MoveToEx(mem, 0, horizon, NULL);
        LineTo(mem, WIN_W, horizon);
        SelectObject(mem, oldPen);
        DeleteObject(penH);

        // prepare pen for wireframe
        HPEN pen = CreatePen(PS_SOLID, 2, RGB(255,255,255));
        SelectObject(mem, pen);

        // draw a simple grid on ground to help sense movement
        for (int i=-20;i<=20;i++){
            Vec3 g1 = { i*1.0, 0.0, 5.0 };
            Vec3 g2 = { i*1.0, 0.0, 40.0 };
            int x1,y1,x2,y2;
            if (project(&g1, camX,camY,camZ, ang, cx,horizon, &x1,&y1) && project(&g2, camX,camY,camZ, ang, cx,horizon, &x2,&y2)){
                MoveToEx(mem, x1, y1, NULL);
                LineTo(mem, x2, y2);
            }
        }

        // draw cube and pyramid
        drawWire(mem, cubeVerts, sizeof(cubeVerts)/sizeof(Vec3), (int (*)[2])cubeEdges, cubeEdgeCount, cubeX, cubeY, cubeZ, camX,camY,camZ, ang, cx, horizon);
        drawWire(mem, pyramidVerts, sizeof(pyramidVerts)/sizeof(Vec3), (int (*)[2])pyramidEdges, pyramidEdgeCount, pyrX, pyrY, pyrZ, camX,camY,camZ, ang, cx, horizon);

        // HUD: mini-map + coordinates
        {
            int mapLeft = 10, mapTop = 10, mapSize = 160;
            double mapScale = 8.0; // pixels per world unit
            int mapCx = mapLeft + mapSize/2;
            int mapCy = mapTop + mapSize/2;

            // background (black per user request)
            HBRUSH mbg = CreateSolidBrush(RGB(0,0,0));
            RECT mr = { mapLeft, mapTop, mapLeft+mapSize, mapTop+mapSize };
            FillRect(mem, &mr, mbg);
            DeleteObject(mbg);

            // border (draw only outline - keep black background)
            HPEN mp = CreatePen(PS_SOLID, 1, RGB(200,200,200));
            HPEN oldMp = SelectObject(mem, mp);
            HBRUSH oldBrush = (HBRUSH)SelectObject(mem, GetStockObject(NULL_BRUSH));
            Rectangle(mem, mapLeft, mapTop, mapLeft+mapSize, mapTop+mapSize);
            SelectObject(mem, oldBrush);

            // draw objects relative to camera (top-down: X to right, Z to up)
            int ox = mapCx + (int)((cubeX - camX) * mapScale);
            int oy = mapCy - (int)((cubeZ - camZ) * mapScale);
            Ellipse(mem, ox-3, oy-3, ox+3, oy+3);

            int px = mapCx + (int)((pyrX - camX) * mapScale);
            int py = mapCy - (int)((pyrZ - camZ) * mapScale);
            Ellipse(mem, px-3, py-3, px+3, py+3);

            // player indicator (center)
            Ellipse(mem, mapCx-4, mapCy-4, mapCx+4, mapCy+4);
            double hx = sin(ang), hz = cos(ang);
            int hx_px = mapCx + (int)(hx * 12);
            int hy_py = mapCy - (int)(hz * 12);
            MoveToEx(mem, mapCx, mapCy, NULL);
            LineTo(mem, hx_px, hy_py);

            // coordinates text
            char buf[128];
            snprintf(buf, sizeof(buf), "X: %.2f  Z: %.2f  A: %.2f", camX, camZ, ang);
            SetTextColor(mem, RGB(255,255,255));
            SetBkMode(mem, TRANSPARENT);
            TextOut(mem, mapLeft, mapTop + mapSize + 6, buf, (int)strlen(buf));

            SelectObject(mem, oldMp);
            DeleteObject(mp);
        }

        // cleanup pen
        SelectObject(mem, GetStockObject(BLACK_PEN));
        DeleteObject(pen);

        // blit
        BitBlt(hdc, 0,0,WIN_W,WIN_H, mem, 0,0, SRCCOPY);

        // release
        SelectObject(mem, old);
        DeleteObject(bmp);
        DeleteDC(mem);
        ReleaseDC(hwnd, hdc);

        Sleep(16);
    }

    return 0;
}
