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

// Map object types and storage
#define OBJ_CUBE 1
#define OBJ_PYRAMID 2
#define OBJ_CUSTOM 3

typedef struct {
    int type;
    double x,y,z;
    double rx,ry,rz; // rotation in degrees around X,Y,Z (optional)
    int shapeIndex; // for custom shapes
    COLORREF color; // optional color for this object (defaults to defaultColor)
    char name[64];
    double hitboxRadius; // 0 = no hitbox
    int isTeleporter; // 1 if teleporter entrance
    char tp_targetMap[256]; // empty = same map
    int tp_mode; // 0=coords,1=spawn,2=default,3=same
    char tp_spawnName[64];
    double tp_x, tp_y, tp_z; // target coords if mode==0
} MapObject;

typedef struct {
    char name[64];
    Vec3 *verts;
    int vertCount;
    int (*edges)[2];
    int edgeCount;
} CustomShape;

static MapObject *mapObjects = NULL;
static int mapObjectCount = 0;
static CustomShape *customShapes = NULL;
static int customShapeCount = 0;
// spawn points for current map
typedef struct { char name[64]; double x,y,z; } SpawnPoint;
static SpawnPoint *spawnPoints = NULL;
static int spawnPointCount = 0;
static int hasDefaultSpawn = 0;
static double defaultSpawnX=0, defaultSpawnY=0, defaultSpawnZ=0;

// teleport state (prevent immediate back-and-forth)
static int canUseTeleport = 1;
static char currentMapFile[256] = "map.txt"; // change this variable to point to another map file
// index of teleporter the player last used and must leave before reusing (-1 = none)
static int lastTeleIndex = -1;

// default wireframe color (can be overridden per-object in the map file)
static COLORREF defaultColor = RGB(255,255,255);

static void freeMap(){
    if (mapObjects) free(mapObjects);
    mapObjects = NULL;
    mapObjectCount = 0;
    for (int i=0;i<customShapeCount;i++){
        if (customShapes[i].verts) free(customShapes[i].verts);
        if (customShapes[i].edges) free(customShapes[i].edges);
    }
    if (customShapes) free(customShapes);
    customShapes = NULL;
    customShapeCount = 0;
    if (spawnPoints) free(spawnPoints);
    spawnPoints = NULL;
    spawnPointCount = 0;
    hasDefaultSpawn = 0;
}

// Clear only map objects and spawn points but preserve custom shape definitions.
static void clearMapObjectsAndSpawns(){
    if (mapObjects) free(mapObjects);
    mapObjects = NULL;
    mapObjectCount = 0;
    if (spawnPoints) free(spawnPoints);
    spawnPoints = NULL;
    spawnPointCount = 0;
    hasDefaultSpawn = 0;
}

static void addSpawnPoint(const char *name, double x,double y,double z){
    SpawnPoint *tmp = realloc(spawnPoints, (spawnPointCount+1)*sizeof(SpawnPoint));
    if (!tmp) return;
    spawnPoints = tmp;
    strncpy(spawnPoints[spawnPointCount].name, name, sizeof(spawnPoints[spawnPointCount].name)-1);
    spawnPoints[spawnPointCount].name[sizeof(spawnPoints[spawnPointCount].name)-1]=0;
    spawnPoints[spawnPointCount].x = x; spawnPoints[spawnPointCount].y = y; spawnPoints[spawnPointCount].z = z;
    spawnPointCount++;
}

static int findSpawnIndex(const char *name){
    for (int i=0;i<spawnPointCount;i++) if (_stricmp(spawnPoints[i].name,name)==0) return i;
    return -1;
}

static void addCustomShape(const char *name, Vec3 *verts, int vcount, int (*edges)[2], int ecount){
    CustomShape *tmp = realloc(customShapes, (customShapeCount+1)*sizeof(CustomShape));
    if (!tmp) return;
    customShapes = tmp;
    CustomShape *cs = &customShapes[customShapeCount];
    strncpy(cs->name, name, sizeof(cs->name)-1); cs->name[sizeof(cs->name)-1]=0;
    cs->verts = verts;
    cs->vertCount = vcount;
    cs->edges = edges;
    cs->edgeCount = ecount;
    customShapeCount++;
}

static int findCustomShape(const char *name){
    for (int i=0;i<customShapeCount;i++){
        if (_stricmp(customShapes[i].name, name)==0) return i;
    }
    return -1;
}

static int loadMap(const char *filename){
    FILE *f = fopen(filename, "r");
    if (!f) return 0;
    // preserve previously defined custom shapes so cross-map placements can reference them
    clearMapObjectsAndSpawns();
    char line[256];
    while (fgets(line, sizeof(line), f)){
        // trim leading spaces
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '#' || *s == '\n' || *s == '\0') continue;
        // detect start of a shape definition
        if (_strnicmp(s, "shape", 5) == 0){
            char sname[64];
            if (sscanf(s+5, "%63s", sname) >= 1){
                // gather vertices and edges until 'endshape'
                Vec3 *vbuf = NULL; int vcount=0;
                int (*ebuf)[2] = NULL; int ecount=0;
                while (fgets(line, sizeof(line), f)){
                    char *t = line;
                    while (*t==' '||*t=='\t') t++;
                    if (_strnicmp(t, "endshape", 8)==0) break;
                    if (*t=='#' || *t=='\n' || *t=='\0') continue;
                    if (*t=='v' || *t=='V'){
                        double vx,vy,vz;
                        if (sscanf(t+1, "%lf %lf %lf", &vx,&vy,&vz) >= 3){
                            Vec3 *vt = realloc(vbuf, (vcount+1)*sizeof(Vec3));
                            if (!vt) break;
                            vbuf = vt;
                            vbuf[vcount].x = vx; vbuf[vcount].y = vy; vbuf[vcount].z = vz; vcount++;
                        }
                    } else if (*t=='e' || *t=='E'){
                        int a,b;
                        if (sscanf(t+1, "%d %d", &a,&b) >= 2){
                            int (*et)[2] = realloc(ebuf, (ecount+1)*sizeof(int[2]));
                            if (!et) break;
                            ebuf = et;
                            ebuf[ecount][0] = a; ebuf[ecount][1] = b; ecount++;
                        }
                    }
                }
                if (vcount>0 && ecount>0){
                    addCustomShape(sname, vbuf, vcount, ebuf, ecount);
                } else {
                    if (vbuf) free(vbuf);
                    if (ebuf) free(ebuf);
                }
            }
            continue;
        }

        // spawn point definitions
        if (_strnicmp(s, "spawn_default", 13) == 0) {
            double sx,sy,sz; if (sscanf(s+13, "%lf %lf %lf", &sx,&sy,&sz) >= 3){ hasDefaultSpawn = 1; defaultSpawnX = sx; defaultSpawnY = sy; defaultSpawnZ = sz; }
            continue;
        }
        if (_strnicmp(s, "spawn", 5) == 0){
            char sname[64]; double sx,sy,sz; if (sscanf(s+5, "%63s %lf %lf %lf", sname, &sx,&sy,&sz) >= 4){ addSpawnPoint(sname, sx,sy,sz); }
            continue;
        }

        // otherwise parse an object placement: <type_or_name> x y z [rx ry rz] [color]
        // tokenize to handle optional color token flexibly
        char *tokens[16]; int tcount=0;
        char *tok = strtok(s, " \t\r\n");
        while (tok && tcount < 16) { tokens[tcount++] = tok; tok = strtok(NULL, " \t\r\n"); }
        if (tcount >= 4){
            char *name = tokens[0];
            double x = atof(tokens[1]);
            double y = atof(tokens[2]);
            double z = atof(tokens[3]);
            double rx = 0, ry = 0, rz = 0;
            COLORREF col = defaultColor;
            int shapeIndex = -1;
            // decide if next tokens are rotations or color
            int idx = 4;
            if (tcount - idx >= 3) {
                // try parsing three rotation numbers
                char *endptr;
                double rxt = strtod(tokens[idx], &endptr);
                if (endptr != tokens[idx]) {
                    // token parse succeeded as number; treat as rotations
                    rx = rxt;
                    ry = atof(tokens[idx+1]);
                    rz = atof(tokens[idx+2]);
                    idx += 3;
                }
            }
            // determine type/shape
            int type = 0;
            if (_stricmp(name, "cube") == 0 || _stricmp(name, "box") == 0) type = OBJ_CUBE;
            else if (_stricmp(name, "pyramid") == 0 || _stricmp(name, "pyr") == 0) type = OBJ_PYRAMID;
            else {
                int idxs = findCustomShape(name);
                if (idxs >= 0) { type = OBJ_CUSTOM; shapeIndex = idxs; }
                else continue; // unknown type/name
            }
            // parse trailing optional tokens (color, hitbox) in any order; collect teleporter into temporaries
            double hitbox = 0.0;
            int colorConsumed = 0;
            int isTele = 0;
            char tpTarget[256] = "";
            int tpMode = 3;
            char tpSpawn[64] = "";
            double tpX = 0, tpY = 0, tpZ = 0;
            while (idx < tcount) {
                char *tkn = tokens[idx];
                // color
                if (!colorConsumed) {
                    if (tkn[0] == '#'){
                        unsigned int v=0; if (sscanf(tkn+1, "%x", &v)==1){ int r=(v>>16)&0xFF; int g=(v>>8)&0xFF; int b=v&0xFF; col = RGB(r,g,b); colorConsumed = 1; idx++; continue; }
                    } else if (strchr(tkn, ',') != NULL) { int r,g,b; if (sscanf(tkn, "%d,%d,%d", &r,&g,&b)==3){ col = RGB(r,g,b); colorConsumed = 1; idx++; continue; } }
                    else if (strncmp(tkn, "0x", 2)==0 || strncmp(tkn, "0X",2)==0) { unsigned int v=0; if (sscanf(tkn, "%x", &v)==1){ int r=(v>>16)&0xFF; int g=(v>>8)&0xFF; int b=v&0xFF; col = RGB(r,g,b); colorConsumed = 1; idx++; continue; } }
                }
                // hitbox
                if (_strnicmp(tkn, "hb=", 3) == 0) { hitbox = atof(tkn+3); idx++; continue; }
                if (_stricmp(tkn, "hb") == 0 && idx+1 < tcount) { hitbox = atof(tokens[idx+1]); idx += 2; continue; }
                // teleporter spec (collect but don't assign to mo yet)
                if (strncmp(tkn, "->", 2) == 0) {
                    char *spec = tkn + 2;
                    isTele = 1;
                    tpTarget[0] = '\0'; tpSpawn[0] = '\0'; tpMode = 3;
                    char *colon = strchr(spec, ':'); char left[256]; char right[256];
                    if (colon) { int l = colon - spec; if (l > 255) l = 255; strncpy(left, spec, l); left[l]=0; strncpy(right, colon+1, 255); right[255]=0; }
                    else { left[0]=0; strncpy(right, spec, 255); right[255]=0; }
                    if (left[0] != 0) { strncpy(tpTarget, left, sizeof(tpTarget)-1); tpTarget[sizeof(tpTarget)-1]=0; }
                    if (_strnicmp(right, "spawn=", 6) == 0) { tpMode = 1; strncpy(tpSpawn, right+6, sizeof(tpSpawn)-1); tpSpawn[sizeof(tpSpawn)-1]=0; }
                    else if (_stricmp(right, "default") == 0) { tpMode = 2; }
                    else if (_stricmp(right, "same") == 0) { tpMode = 3; }
                    else if (_strnicmp(right, "coords=",7)==0) { tpMode = 0; double tx=0,ty=0,tz=0; if (sscanf(right+7, "%lf,%lf,%lf", &tx,&ty,&tz)==3){ tpX=tx; tpY=ty; tpZ=tz; } }
                    idx++; continue;
                }
                // numeric standalone -> treat as hitbox if not a rotation (rotations already parsed)
                char *endp; double hv = strtod(tkn, &endp);
                if (endp != tkn) { hitbox = hv; idx++; continue; }
                // unknown token - skip
                idx++;
            }

            MapObject mo;
            memset(&mo, 0, sizeof(mo));
            mo.type = type; mo.x = x; mo.y = y; mo.z = z; mo.rx = rx; mo.ry = ry; mo.rz = rz; mo.shapeIndex = shapeIndex; mo.color = col; mo.hitboxRadius = hitbox;
            // set object name for HUD reporting
            if (type == OBJ_CUSTOM && shapeIndex >= 0) strncpy(mo.name, customShapes[shapeIndex].name, sizeof(mo.name)-1);
            else strncpy(mo.name, name, sizeof(mo.name)-1);
            mo.name[sizeof(mo.name)-1] = '\0';
            if (isTele) { mo.isTeleporter = 1; strncpy(mo.tp_targetMap, tpTarget, sizeof(mo.tp_targetMap)-1); mo.tp_targetMap[sizeof(mo.tp_targetMap)-1]=0; mo.tp_mode = tpMode; strncpy(mo.tp_spawnName, tpSpawn, sizeof(mo.tp_spawnName)-1); mo.tp_x = tpX; mo.tp_y = tpY; mo.tp_z = tpZ; }

            MapObject *tmp = realloc(mapObjects, (mapObjectCount+1)*sizeof(MapObject));
            if (!tmp) break;
            mapObjects = tmp;
            mapObjects[mapObjectCount++] = mo;
        }
    }
    fclose(f);
    return 1;
}

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
static void drawWire(HDC dc, const Vec3 *verts, int vertCount, int edges[][2], int edgeCount, double ox,double oy,double oz, double camX,double camY,double camZ,double ang, int cx,int cy, double rotX_deg, double rotY_deg, double rotZ_deg) {
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
        // apply rotation around object-local origin (X then Y then Z)
        double vx_a = verts[a].x;
        double vy_a = verts[a].y;
        double vz_a = verts[a].z;
        // rotate X
        double vy_a1 = vy_a*crx - vz_a*srx;
        double vz_a1 = vy_a*srx + vz_a*crx;
        // rotate Y
        double vx_a2 = vx_a*cry + vz_a1*sry;
        double vz_a2 = -vx_a*sry + vz_a1*cry;
        // rotate Z
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
    double playerHitbox = 0.6; // radius for player collision detection

    // place objects on the map (loaded from file)
    loadMap(currentMapFile);
    // pressing 'L' reloads the current map file at runtime
    int lastLState = 0;

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

        // Shift increases movement speed
        int shiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        double speedCurr = speed * (shiftHeld ? 2.0 : 1.0);

        if (moveForward) { camX += fx * speedCurr; camZ += fz * speedCurr; }
        if (moveBack)    { camX -= fx * speedCurr; camZ -= fz * speedCurr; }
        if (turnLeft)    { ang -= rotSpeed; }
        if (turnRight)   { ang += rotSpeed; }
        if (strafeLeft)  { camX -= rx * speedCurr; camZ -= rz * speedCurr; }
        if (strafeRight) { camX += rx * speedCurr; camZ += rz * speedCurr; }

        // Reload map on L key press (press once to reload current map file)
        int lState = (GetAsyncKeyState('L') & 0x8000) != 0;
        if (lState && !lastLState) {
            loadMap(currentMapFile);
        }
        lastLState = lState;

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

        // prepare pen for wireframe (use defaultColor)
        HPEN pen = CreatePen(PS_SOLID, 2, defaultColor);
        SelectObject(mem, pen);

        // draw a simple grid on ground to help sense movement
        // use near-plane clipping so long lines don't vanish when part goes behind camera
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

        // draw map objects (use per-object color when available)
        for (int mi=0; mi<mapObjectCount; ++mi){
            MapObject *mo = &mapObjects[mi];
            // teleporter entrances are invisible in-world but still show on mini-map/hitbox
            if (mo->isTeleporter) continue;
            // create object pen with specified color
            HPEN objPen = CreatePen(PS_SOLID, 2, mo->color);
            HGDIOBJ prevPen = SelectObject(mem, objPen);
            if (mo->type == OBJ_CUBE) {
                drawWire(mem, cubeVerts, sizeof(cubeVerts)/sizeof(Vec3), (int (*)[2])cubeEdges, cubeEdgeCount, mo->x, mo->y, mo->z, camX,camY,camZ, ang, cx, horizon, mo->rx, mo->ry, mo->rz);
            } else if (mo->type == OBJ_PYRAMID) {
                drawWire(mem, pyramidVerts, sizeof(pyramidVerts)/sizeof(Vec3), (int (*)[2])pyramidEdges, pyramidEdgeCount, mo->x, mo->y, mo->z, camX,camY,camZ, ang, cx, horizon, mo->rx, mo->ry, mo->rz);
            } else if (mo->type == OBJ_CUSTOM) {
                if (mo->shapeIndex >=0 && mo->shapeIndex < customShapeCount){
                    CustomShape *cs = &customShapes[mo->shapeIndex];
                    drawWire(mem, cs->verts, cs->vertCount, cs->edges, cs->edgeCount, mo->x, mo->y, mo->z, camX,camY,camZ, ang, cx, horizon, mo->rx, mo->ry, mo->rz);
                }
            }
            // restore and delete object pen
            SelectObject(mem, prevPen);
            DeleteObject(objPen);
        }

        // collision detection (player vs object hitboxes)
        MapObject *collided = NULL;
        int collIndex = -1;
        for (int mi=0; mi<mapObjectCount; ++mi){
            MapObject *mo = &mapObjects[mi];
            if (mo->hitboxRadius <= 0.0) continue;
            double dx = mo->x - camX;
            double dy = mo->y - camY;
            double dz = mo->z - camZ;
            double dist = sqrt(dx*dx + dy*dy + dz*dz);
            if (dist <= (mo->hitboxRadius + playerHitbox)) { collided = mo; collIndex = mi; break; }
        }

        // teleporter activation
        if (collided && collided->hitboxRadius > 0.0 && collided->isTeleporter) {
            if (canUseTeleport) {
                // perform teleport
                double oldX = camX, oldY = camY, oldZ = camZ;
                if (collided->tp_targetMap[0] == '\0') {
                    // same map teleport
                    if (collided->tp_mode == 0) {
                        camX = collided->tp_x; camY = collided->tp_y; camZ = collided->tp_z;
                    } else if (collided->tp_mode == 1) {
                        int si = findSpawnIndex(collided->tp_spawnName);
                        if (si >= 0) { camX = spawnPoints[si].x; camY = spawnPoints[si].y; camZ = spawnPoints[si].z; }
                        else if (hasDefaultSpawn) { camX = defaultSpawnX; camY = defaultSpawnY; camZ = defaultSpawnZ; }
                    } else if (collided->tp_mode == 2) {
                        if (hasDefaultSpawn) { camX = defaultSpawnX; camY = defaultSpawnY; camZ = defaultSpawnZ; }
                    } else if (collided->tp_mode == 3) {
                        // same coords - no-op
                    }
                    // after moving within same map, check if player is still overlapping any teleporter at new position
                    int foundIndex = -1;
                    for (int i=0;i<mapObjectCount;i++){
                        MapObject *m = &mapObjects[i];
                        if (!m->isTeleporter || m->hitboxRadius <= 0.0) continue;
                        double dx = m->x - camX; double dy = m->y - camY; double dz = m->z - camZ;
                        double d = sqrt(dx*dx + dy*dy + dz*dz);
                        if (d <= (m->hitboxRadius + playerHitbox + 0.001)) { foundIndex = i; break; }
                    }
                    if (foundIndex >= 0) { lastTeleIndex = foundIndex; canUseTeleport = 0; }
                    else { lastTeleIndex = -1; canUseTeleport = 1; }
                } else {
                    // teleport to another map file
                    char prevMap[256]; strncpy(prevMap, currentMapFile, sizeof(prevMap)-1); prevMap[sizeof(prevMap)-1]=0;
                    strncpy(currentMapFile, collided->tp_targetMap, sizeof(currentMapFile)-1); currentMapFile[sizeof(currentMapFile)-1]=0;
                    // load target map
                    loadMap(currentMapFile);
                    // determine spawn location on new map
                    if (collided->tp_mode == 0) {
                        camX = collided->tp_x; camY = collided->tp_y; camZ = collided->tp_z;
                    } else if (collided->tp_mode == 1) {
                        int si = findSpawnIndex(collided->tp_spawnName);
                        if (si >= 0) { camX = spawnPoints[si].x; camY = spawnPoints[si].y; camZ = spawnPoints[si].z; }
                        else if (hasDefaultSpawn) { camX = defaultSpawnX; camY = defaultSpawnY; camZ = defaultSpawnZ; }
                        else { camX = oldX; camY = oldY; camZ = oldZ; }
                    } else if (collided->tp_mode == 2) {
                        if (hasDefaultSpawn) { camX = defaultSpawnX; camY = defaultSpawnY; camZ = defaultSpawnZ; }
                        else { camX = oldX; camY = oldY; camZ = oldZ; }
                    } else if (collided->tp_mode == 3) {
                        // same coords on target map
                        camX = oldX; camY = oldY; camZ = oldZ;
                    }
                    // after loading target map and moving player, find any teleporter at the landing position and require leaving it before reuse
                    int foundIndex = -1;
                    for (int i=0;i<mapObjectCount;i++){
                        MapObject *m = &mapObjects[i];
                        if (!m->isTeleporter || m->hitboxRadius <= 0.0) continue;
                        double dx = m->x - camX; double dy = m->y - camY; double dz = m->z - camZ;
                        double d = sqrt(dx*dx + dy*dy + dz*dz);
                        if (d <= (m->hitboxRadius + playerHitbox + 0.001)) { foundIndex = i; break; }
                    }
                    if (foundIndex >= 0) { lastTeleIndex = foundIndex; canUseTeleport = 0; }
                    else { lastTeleIndex = -1; canUseTeleport = 1; }
                }
                collided = NULL; // avoid immediate HUD message
            }
        }
        // if player used a teleporter recently, require leaving that teleporter's hitbox before re-enabling teleport
        if (lastTeleIndex >= 0) {
            if (lastTeleIndex < mapObjectCount) {
                MapObject *lt = &mapObjects[lastTeleIndex];
                double dx = lt->x - camX; double dy = lt->y - camY; double dz = lt->z - camZ;
                double d = sqrt(dx*dx + dy*dy + dz*dz);
                if (d > (lt->hitboxRadius + playerHitbox)) { canUseTeleport = 1; lastTeleIndex = -1; }
            } else {
                // teleporter no longer exists on this map (map changed) — allow use
                lastTeleIndex = -1; canUseTeleport = 1;
            }
        }

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
            int half = mapSize/2;
            // draw all map objects on mini-map (clamped to map rectangle)
            for (int mi=0; mi<mapObjectCount; ++mi){
                MapObject *mo = &mapObjects[mi];
                int ox = mapCx + (int)((mo->x - camX) * mapScale);
                int oy = mapCy - (int)((mo->z - camZ) * mapScale);
                int dxo = ox - mapCx;
                int dyo = oy - mapCy;
                if (dxo >= -half && dxo <= half && dyo >= -half && dyo <= half) {
                    // small center marker
                    HBRUSH ob = CreateSolidBrush(mo->color);
                    HBRUSH oldb = (HBRUSH)SelectObject(mem, ob);
                    Ellipse(mem, ox-3, oy-3, ox+3, oy+3);
                    SelectObject(mem, oldb);
                    DeleteObject(ob);
                    // draw hitbox circle on mini-map if present
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

            // collision HUD: top-right small info box when colliding
            if (collided) {
                int hxW = 220, hxH = 28;
                int hxLeft = WIN_W - hxW - 10;
                int hxTop = 10;
                // colored border
                HPEN cp = CreatePen(PS_SOLID, 3, collided->color);
                HGDIOBJ oldCp = SelectObject(mem, cp);
                HBRUSH oldB = (HBRUSH)SelectObject(mem, GetStockObject(NULL_BRUSH));
                Rectangle(mem, hxLeft, hxTop, hxLeft+hxW, hxTop+hxH);
                SelectObject(mem, oldB);
                SelectObject(mem, oldCp);
                DeleteObject(cp);
                // text
                char cbuf[128];
                snprintf(cbuf, sizeof(cbuf), "Collision: %s (r=%.2f)", collided->name, collided->hitboxRadius);
                SetTextColor(mem, collided->color);
                TextOut(mem, hxLeft+6, hxTop+6, cbuf, (int)strlen(cbuf));
                // restore text color
                SetTextColor(mem, RGB(255,255,255));
            }

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
