#include "common.h"
#include "render.h"
#include "input.h"
#include "game.h"

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
    double playerHitbox = 0.6; // radius for player collision detection

    // load map and initialize state
    loadMap(currentMapFile);
    int lastLState = 0;

    // main loop
    MSG msg;
    PeekMessage(&msg,NULL,0,0,PM_NOREMOVE);
    int cx = WIN_W/2, cy = WIN_H/2;

    unsigned long long lastTick = GetTickCount64();

    while (1){
        unsigned long long now = GetTickCount64();
        double dt = (now > lastTick) ? ((now - lastTick) / 1000.0) : 0.016;
        lastTick = now;
        while (PeekMessage(&msg,NULL,0,0,PM_REMOVE)){
            if (msg.message == WM_QUIT) return (int)msg.wParam;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        double speedCurr = 0.0;
        int reload = handleInput(&camX, &camY, &camZ, &ang, speed, rotSpeed, &speedCurr, &lastLState);
        if (reload) loadMap(currentMapFile);

        updateObjects(dt, camX, camY, camZ);

        // rendering
        HDC hdc = GetDC(hwnd);
        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, WIN_W, WIN_H);
        HGDIOBJ old = SelectObject(mem, bmp);

        int horizon = cy - (int)( (camY - 1.0) * 40 );
        RECT r = {0,0,WIN_W, WIN_H};
        HBRUSH sky = CreateSolidBrush(RGB(100,160,240));
        FillRect(mem, &r, sky);
        DeleteObject(sky);
        RECT ground = {0,horizon,WIN_W,WIN_H};
        HBRUSH groundb = CreateSolidBrush(RGB(80,140,60));
        FillRect(mem, &ground, groundb);
        DeleteObject(groundb);

        HPEN penH = CreatePen(PS_SOLID, 2, RGB(200,200,200));
        HPEN oldPen = SelectObject(mem, penH);
        MoveToEx(mem, 0, horizon, NULL);
        LineTo(mem, WIN_W, horizon);
        SelectObject(mem, oldPen);
        DeleteObject(penH);

        HPEN pen = CreatePen(PS_SOLID, 2, defaultColor);
        SelectObject(mem, pen);

        for (int i=-20;i<=20;i++){
            Vec3 g1 = { i*1.0, 0.0, 5.0 };
            Vec3 g2 = { i*1.0, 0.0, 40.0 };

            double atx,aty,atz, btx,bty,btz;
            transformPoint(&g1, camX,camY,camZ, ang, &atx,&aty,&atz);
            transformPoint(&g2, camX,camY,camZ, ang, &btx,&bty,&btz);
            const double nearPlane = 0.05;
            if (atz <= nearPlane && btz <= nearPlane) continue;
            if (atz <= nearPlane || btz <= nearPlane){
                double t = 0.0;
                if ((btz - atz) != 0.0) t = (nearPlane - atz) / (btz - atz);
                if (atz < btz){
                    atx = atx + (btx - atx) * t;
                    aty = aty + (bty - aty) * t;
                    atz = nearPlane;
                } else {
                    btx = atx + (btx - atx) * t;
                    bty = aty + (bty - aty) * t;
                    btz = nearPlane;
                }
            }
            int x1 = cx + (int)((atx * FOV_SCALE) / atz);
            int y1 = horizon - (int)((aty * FOV_SCALE) / atz);
            int x2 = cx + (int)((btx * FOV_SCALE) / btz);
            int y2 = horizon - (int)((bty * FOV_SCALE) / btz);
            MoveToEx(mem, x1, y1, NULL);
            LineTo(mem, x2, y2);
        }

        for (int mi=0; mi<mapObjectCount; ++mi){
            MapObject *mo = &mapObjects[mi];
            HPEN objPen = CreatePen(PS_SOLID, 2, mo->color);
            HGDIOBJ prevPen = SelectObject(mem, objPen);
            if (mo->type == OBJ_CUBE) {
                drawWire(mem, cubeVerts, cubeVertCount, (int (*)[2])cubeEdges, cubeEdgeCount, mo->x, mo->y, mo->z, camX,camY,camZ, ang, cx, horizon, mo->rx, mo->ry, mo->rz);
            } else if (mo->type == OBJ_PYRAMID) {
                drawWire(mem, pyramidVerts, pyramidVertCount, (int (*)[2])pyramidEdges, pyramidEdgeCount, mo->x, mo->y, mo->z, camX,camY,camZ, ang, cx, horizon, mo->rx, mo->ry, mo->rz);
            } else if (mo->type == OBJ_CUSTOM) {
                if (mo->shapeIndex >=0 && mo->shapeIndex < customShapeCount){
                    CustomShape *cs = &customShapes[mo->shapeIndex];
                    drawWire(mem, cs->verts, cs->vertCount, cs->edges, cs->edgeCount, mo->x, mo->y, mo->z, camX,camY,camZ, ang, cx, horizon, mo->rx, mo->ry, mo->rz);
                }
            }
            SelectObject(mem, prevPen);
            DeleteObject(objPen);
        }

        MapObject *collided = NULL;
        int collIndex = -1;
        detectCollision(camX, camY, camZ, playerHitbox, &collided, &collIndex);
        handleTeleportIfNeeded(&collided, &camX, &camY, &camZ, playerHitbox);

        {
            int mapLeft = 10, mapTop = 10, mapSize = 160;
            double mapScale = 8.0;
            int mapCx = mapLeft + mapSize/2;
            int mapCy = mapTop + mapSize/2;

            HBRUSH mbg = CreateSolidBrush(RGB(0,0,0));
            RECT mr = { mapLeft, mapTop, mapLeft+mapSize, mapTop+mapSize };
            FillRect(mem, &mr, mbg);
            DeleteObject(mbg);

            HPEN mp = CreatePen(PS_SOLID, 1, RGB(200,200,200));
            HPEN oldMp = SelectObject(mem, mp);
            HBRUSH oldBrush = (HBRUSH)SelectObject(mem, GetStockObject(NULL_BRUSH));
            Rectangle(mem, mapLeft, mapTop, mapLeft+mapSize, mapTop+mapSize);
            SelectObject(mem, oldBrush);

            int half = mapSize/2;
            for (int mi=0; mi<mapObjectCount; ++mi){
                MapObject *mo = &mapObjects[mi];
                int ox = mapCx + (int)((mo->x - camX) * mapScale);
                int oy = mapCy - (int)((mo->z - camZ) * mapScale);
                int dxo = ox - mapCx;
                int dyo = oy - mapCy;
                if (dxo >= -half && dxo <= half && dyo >= -half && dyo <= half) {
                    HBRUSH ob = CreateSolidBrush(mo->color);
                    HBRUSH oldb = (HBRUSH)SelectObject(mem, ob);
                    Ellipse(mem, ox-3, oy-3, ox+3, oy+3);
                    SelectObject(mem, oldb);
                    DeleteObject(ob);
                    if (mo->hitboxRadius > 0.0) {
                        int hr = (int)(mo->hitboxRadius * mapScale);
                        HPEN hpen = CreatePen(PS_SOLID, 1, mo->color);
                        HGDIOBJ oldp = SelectObject(mem, hpen);
                        HBRUSH nbrush = (HBRUSH)SelectObject(mem, GetStockObject(NULL_BRUSH));
                        Ellipse(mem, ox-hr, oy-hr, ox+hr, oy+hr);
                        SelectObject(mem, nbrush);
                        SelectObject(mem, oldp);
                        DeleteObject(hpen);
                    }
                }
            }

            Ellipse(mem, mapCx-4, mapCy-4, mapCx+4, mapCy+4);
            double hx = sin(ang), hz = cos(ang);
            int hx_px = mapCx + (int)(hx * 12);
            int hy_py = mapCy - (int)(hz * 12);
            MoveToEx(mem, mapCx, mapCy, NULL);
            LineTo(mem, hx_px, hy_py);

            char buf[128];
            snprintf(buf, sizeof(buf), "X: %.2f  Z: %.2f  A: %.2f", camX, camZ, ang);
            SetTextColor(mem, RGB(255,255,255));
            SetBkMode(mem, TRANSPARENT);
            TextOut(mem, mapLeft, mapTop + mapSize + 6, buf, (int)strlen(buf));

            if (collided) {
                int hxW = 220, hxH = 28;
                int hxLeft = WIN_W - hxW - 10;
                int hxTop = 10;
                HPEN cp = CreatePen(PS_SOLID, 3, collided->color);
                HGDIOBJ oldCp = SelectObject(mem, cp);
                HBRUSH oldB = (HBRUSH)SelectObject(mem, GetStockObject(NULL_BRUSH));
                Rectangle(mem, hxLeft, hxTop, hxLeft+hxW, hxTop+hxH);
                SelectObject(mem, oldB);
                SelectObject(mem, oldCp);
                DeleteObject(cp);
                char cbuf[128];
                snprintf(cbuf, sizeof(cbuf), "Collision: %s (r=%.2f)", collided->name, collided->hitboxRadius);
                SetTextColor(mem, collided->color);
                TextOut(mem, hxLeft+6, hxTop+6, cbuf, (int)strlen(cbuf));
                SetTextColor(mem, RGB(255,255,255));
            }

            SelectObject(mem, oldMp);
            DeleteObject(mp);
        }

        SelectObject(mem, GetStockObject(BLACK_PEN));
        DeleteObject(pen);

        BitBlt(hdc, 0,0,WIN_W,WIN_H, mem, 0,0, SRCCOPY);

        SelectObject(mem, old);
        DeleteObject(bmp);
        DeleteDC(mem);
        ReleaseDC(hwnd, hdc);

        Sleep(16);
    }

    return 0;
}
