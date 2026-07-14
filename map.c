#include "common.h"
#include <time.h>

static int parse_color_token(const char *text, COLORREF *out) {
    if (!text || !*text) return 0;
    if (text[0] == '#') {
        unsigned int v = 0;
        if (sscanf(text + 1, "%x", &v) == 1) {
            int r = (v >> 16) & 0xFF;
            int g = (v >> 8) & 0xFF;
            int b = v & 0xFF;
            *out = RGB(r, g, b);
            return 1;
        }
    } else if (strchr(text, ',') != NULL) {
        int r, g, b;
        if (sscanf(text, "%d,%d,%d", &r, &g, &b) == 3) {
            *out = RGB(r, g, b);
            return 1;
        }
    } else if (strncmp(text, "0x", 2) == 0 || strncmp(text, "0X", 2) == 0) {
        unsigned int v = 0;
        if (sscanf(text, "%x", &v) == 1) {
            int r = (v >> 16) & 0xFF;
            int g = (v >> 8) & 0xFF;
            int b = v & 0xFF;
            *out = RGB(r, g, b);
            return 1;
        }
    }
    return 0;
}

/* Helpers to keep loadMap concise */
static void parse_shape_block(FILE *f, const char *firstLine) {
    char sname[64];
    if (sscanf(firstLine+5, "%63s", sname) < 1) return;
    Vec3 *vbuf = NULL; int vcount=0;
    int (*ebuf)[2] = NULL; int ecount=0;
    ShapeFace *fbuf = NULL; int fcount=0;
    char line[256];
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
        } else if (*t=='f' || *t=='F'){
            char *rest = t + 1;
            int indices[16]; int icount = 0;
            COLORREF faceColor = 0; int hasFaceColor = 0;
            char *tok = strtok(rest, " \t\r\n");
            while (tok && icount < 16) {
                if (parse_color_token(tok, &faceColor)) {
                    hasFaceColor = 1;
                } else {
                    indices[icount++] = atoi(tok);
                }
                tok = strtok(NULL, " \t\r\n");
            }
            if (icount >= 3) {
                ShapeFace *ft = realloc(fbuf, (fcount+1)*sizeof(ShapeFace));
                if (!ft) break;
                fbuf = ft;
                fbuf[fcount].vertexCount = icount;
                fbuf[fcount].vertexIndices = malloc(icount * sizeof(int));
                if (!fbuf[fcount].vertexIndices) break;
                for (int j = 0; j < icount; ++j) fbuf[fcount].vertexIndices[j] = indices[j];
                fbuf[fcount].color = faceColor;
                fbuf[fcount].hasColor = hasFaceColor;
                fcount++;
            }
        }
    }
    if (vcount>0 && (ecount>0 || fcount>0)){
        addCustomShape(sname, vbuf, vcount, ebuf, ecount, fbuf, fcount);
    } else {
        if (vbuf) free(vbuf);
        if (ebuf) free(ebuf);
        if (fbuf) {
            for (int i=0;i<fcount;i++) if (fbuf[i].vertexIndices) free(fbuf[i].vertexIndices);
            free(fbuf);
        }
    }
}

static int parse_object_tokens_and_add(char **tokens, int tcount) {
    if (tcount < 4) return 0;
    char *name = tokens[0];
    double x = atof(tokens[1]);
    double y = atof(tokens[2]);
    double z = atof(tokens[3]);
    double rx = 0, ry = 0, rz = 0;
    COLORREF col = defaultColor;
    int shapeIndex = -1;
    int idx = 4;
    if (tcount - idx >= 3) {
        char *endptr;
        double rxt = strtod(tokens[idx], &endptr);
        if (endptr != tokens[idx]) {
            rx = rxt;
            ry = atof(tokens[idx+1]);
            rz = atof(tokens[idx+2]);
            idx += 3;
        }
    }
    int type = 0;
    if (_stricmp(name, "cube") == 0 || _stricmp(name, "box") == 0) type = OBJ_CUBE;
    else if (_stricmp(name, "pyramid") == 0 || _stricmp(name, "pyr") == 0) type = OBJ_PYRAMID;
    else {
        int idxs = findCustomShape(name);
        if (idxs >= 0) { type = OBJ_CUSTOM; shapeIndex = idxs; }
        else return 0;
    }
    double hitbox = 0.0;
    int colorConsumed = 0;
    COLORREF fillColorLocal = 0;
    int hasFillColorLocal = 0;
    int isTele = 0;
    char tpTarget[256] = "";
    int tpMode = 3;
    char tpSpawn[64] = "";
    double tpX = 0, tpY = 0, tpZ = 0;
    /* temporary movement overrides parsed from tokens */
    int movementTypeLocal = -1;
    double mspeedLocal = -1.0;
    double turnLocal = -1.0;
    double maxdistLocal = -1.0;

    while (idx < tcount) {
        char *tkn = tokens[idx];
        /* movement and behavior flags */
        if (_strnicmp(tkn, "move=", 5) == 0) {
            char *v = tkn + 5;
            if (_stricmp(v, "wandering") == 0 || _stricmp(v, "wander") == 0) movementTypeLocal = 1;
            else if (_stricmp(v, "chase") == 0 || _stricmp(v, "chaseplayer") == 0) movementTypeLocal = 2;
            else if (_stricmp(v, "none") == 0) movementTypeLocal = 0;
            idx++; continue;
        }
        if (_strnicmp(tkn, "mspeed=", 7) == 0) { mspeedLocal = atof(tkn+7); idx++; continue; }
        if (_strnicmp(tkn, "turn=", 5) == 0) { turnLocal = atof(tkn+5); idx++; continue; }
        if (_strnicmp(tkn, "maxdist=", 8) == 0) { maxdistLocal = atof(tkn+8); idx++; continue; }

        if (!colorConsumed) {
            if (tkn[0] == '#'){
                unsigned int v=0; if (sscanf(tkn+1, "%x", &v)==1){ int r=(v>>16)&0xFF; int g=(v>>8)&0xFF; int b=v&0xFF; col = RGB(r,g,b); colorConsumed = 1; idx++; continue; }
            } else if (strchr(tkn, ',') != NULL) { int r,g,b; if (sscanf(tkn, "%d,%d,%d", &r,&g,&b)==3){ col = RGB(r,g,b); colorConsumed = 1; idx++; continue; } }
            else if (strncmp(tkn, "0x", 2)==0 || strncmp(tkn, "0X",2)==0) { unsigned int v=0; if (sscanf(tkn, "%x", &v)==1){ int r=(v>>16)&0xFF; int g=(v>>8)&0xFF; int b=v&0xFF; col = RGB(r,g,b); colorConsumed = 1; idx++; continue; } }
        }
        if (_strnicmp(tkn, "fill=", 5) == 0 || _strnicmp(tkn, "fillcolor=", 10) == 0) {
            const char *value = (_strnicmp(tkn, "fill=", 5) == 0) ? (tkn + 5) : (tkn + 10);
            if (parse_color_token(value, &fillColorLocal)) { hasFillColorLocal = 1; }
            idx++; continue;
        }
        if (_strnicmp(tkn, "hb=", 3) == 0) { hitbox = atof(tkn+3); idx++; continue; }
        if (_stricmp(tkn, "hb") == 0 && idx+1 < tcount) { hitbox = atof(tokens[idx+1]); idx += 2; continue; }
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
        char *endp; double hv = strtod(tkn, &endp);
        if (endp != tkn) { hitbox = hv; idx++; continue; }
        idx++;
    }

    MapObject mo;
    memset(&mo, 0, sizeof(mo));
    mo.type = type; mo.x = x; mo.y = y; mo.z = z; mo.rx = rx; mo.ry = ry; mo.rz = rz; mo.shapeIndex = shapeIndex; mo.color = col; mo.hitboxRadius = hitbox;
    if (type == OBJ_CUSTOM && shapeIndex >= 0) strncpy(mo.name, customShapes[shapeIndex].name, sizeof(mo.name)-1);
    else strncpy(mo.name, name, sizeof(mo.name)-1);
    mo.name[sizeof(mo.name)-1] = '\0';
    if (isTele) {
        mo.isTeleporter = 1;
        strncpy(mo.tp_targetMap, tpTarget, sizeof(mo.tp_targetMap)-1);
        mo.tp_targetMap[sizeof(mo.tp_targetMap)-1]=0;
        mo.tp_mode = tpMode;
        /* copy spawn name safely and ensure NUL termination */
        strncpy(mo.tp_spawnName, tpSpawn, sizeof(mo.tp_spawnName)-1);
        mo.tp_spawnName[sizeof(mo.tp_spawnName)-1] = '\0';
        mo.tp_x = tpX; mo.tp_y = tpY; mo.tp_z = tpZ;
    }

    /* initialize movement defaults */
    mo.homeX = mo.x; mo.homeY = mo.y; mo.homeZ = mo.z;
    mo.wanderAngle = ((double)rand() / RAND_MAX) * 2.0 * PI;
    mo.wanderInterval = 0.5 + ((double)rand() / RAND_MAX) * 2.5; /* 0.5..3.0s */
    mo.wanderTimer = mo.wanderInterval;
    mo.maxDistFromHome = 5.0;
    mo.turnSpeed = 45.0; /* degrees per second */
    mo.desiredRy = mo.ry;
    if (mo.type == OBJ_CUBE) {
        mo.movementType = 1; /* wandering */
        mo.moveSpeed = 0.02; /* very slow */
        mo.maxDistFromHome = 4.0;
        mo.turnSpeed = 20.0;
        mo.hasFillColor = 1;
        mo.fillColor = RGB(180, 180, 220);
    } else if (mo.type == OBJ_PYRAMID) {
        mo.movementType = 2; /* chase player */
        mo.moveSpeed = 0.6; /* slow but faster than cubes */
        mo.maxDistFromHome = 1000.0; /* not strictly bounded */
        mo.turnSpeed = 60.0;
        mo.hasFillColor = 1;
        mo.fillColor = RGB(220, 120, 70);
    } else {
        mo.movementType = 0;
        mo.moveSpeed = 0.0;
    }
    if (hasFillColorLocal) {
        mo.hasFillColor = 1;
        mo.fillColor = fillColorLocal;
    }

    /* apply overrides parsed from tokens */
    if (movementTypeLocal != -1) mo.movementType = movementTypeLocal;
    if (mspeedLocal >= 0.0) mo.moveSpeed = mspeedLocal;
    if (turnLocal >= 0.0) mo.turnSpeed = turnLocal;
    if (maxdistLocal >= 0.0) mo.maxDistFromHome = maxdistLocal;

    MapObject *tmp = realloc(mapObjects, (mapObjectCount+1)*sizeof(MapObject));
    if (!tmp) return 0;
    mapObjects = tmp;
    mapObjects[mapObjectCount++] = mo;
    return 1;
}

MapObject *mapObjects = NULL;
int mapObjectCount = 0;
CustomShape *customShapes = NULL;
int customShapeCount = 0;
SpawnPoint *spawnPoints = NULL;
int spawnPointCount = 0;
int hasDefaultSpawn = 0;
double defaultSpawnX=0, defaultSpawnY=0, defaultSpawnZ=0;

int canUseTeleport = 1;
char currentMapFile[256] = "map.txt";
int lastTeleIndex = -1;

COLORREF defaultColor = RGB(255,255,255);

void freeMap(){
    if (mapObjects) free(mapObjects);
    mapObjects = NULL; mapObjectCount = 0;
    for (int i=0;i<customShapeCount;i++){
        if (customShapes[i].verts) free(customShapes[i].verts);
        if (customShapes[i].edges) free(customShapes[i].edges);
        if (customShapes[i].faces) {
            for (int f=0; f<customShapes[i].faceCount; ++f) {
                if (customShapes[i].faces[f].vertexIndices) free(customShapes[i].faces[f].vertexIndices);
            }
            free(customShapes[i].faces);
        }
    }
    if (customShapes) free(customShapes);
    customShapes = NULL; customShapeCount = 0;
    if (spawnPoints) free(spawnPoints);
    spawnPoints = NULL; spawnPointCount = 0;
    hasDefaultSpawn = 0;
}

void clearMapObjectsAndSpawns(){
    if (mapObjects) free(mapObjects);
    mapObjects = NULL; mapObjectCount = 0;
    if (spawnPoints) free(spawnPoints);
    spawnPoints = NULL; spawnPointCount = 0;
    hasDefaultSpawn = 0;
}

void addSpawnPoint(const char *name, double x,double y,double z){
    SpawnPoint *tmp = realloc(spawnPoints, (spawnPointCount+1)*sizeof(SpawnPoint));
    if (!tmp) return;
    spawnPoints = tmp;
    strncpy(spawnPoints[spawnPointCount].name, name, sizeof(spawnPoints[spawnPointCount].name)-1);
    spawnPoints[spawnPointCount].name[sizeof(spawnPoints[spawnPointCount].name)-1]=0;
    spawnPoints[spawnPointCount].x = x; spawnPoints[spawnPointCount].y = y; spawnPoints[spawnPointCount].z = z;
    spawnPointCount++;
}

int findSpawnIndex(const char *name){
    for (int i=0;i<spawnPointCount;i++) if (_stricmp(spawnPoints[i].name,name)==0) return i;
    return -1;
}

void addCustomShape(const char *name, Vec3 *verts, int vcount, int (*edges)[2], int ecount, ShapeFace *faces, int faceCount){
    CustomShape *tmp = realloc(customShapes, (customShapeCount+1)*sizeof(CustomShape));
    if (!tmp) return;
    customShapes = tmp;
    CustomShape *cs = &customShapes[customShapeCount];
    strncpy(cs->name, name, sizeof(cs->name)-1); cs->name[sizeof(cs->name)-1]=0;
    cs->verts = verts;
    cs->vertCount = vcount;
    cs->edges = edges;
    cs->edgeCount = ecount;
    cs->faces = faces;
    cs->faceCount = faceCount;
    customShapeCount++;
}

int findCustomShape(const char *name){
    for (int i=0;i<customShapeCount;i++){
        if (_stricmp(customShapes[i].name, name)==0) return i;
    }
    return -1;
}

int loadMap(const char *filename){
    FILE *f = fopen(filename, "r");
    if (!f) return 0;
    /* seed RNG for wandering behavior */
    srand((unsigned)time(NULL));
    clearMapObjectsAndSpawns();
    char line[256];
    while (fgets(line, sizeof(line), f)){
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '#' || *s == '\n' || *s == '\0') continue;
        if (_strnicmp(s, "shape", 5) == 0){
            parse_shape_block(f, s);
            continue;
        }

        if (_strnicmp(s, "spawn_default", 13) == 0) {
            double sx,sy,sz; if (sscanf(s+13, "%lf %lf %lf", &sx,&sy,&sz) >= 3){ hasDefaultSpawn = 1; defaultSpawnX = sx; defaultSpawnY = sy; defaultSpawnZ = sz; }
            continue;
        }
        if (_strnicmp(s, "spawn", 5) == 0){
            char sname[64]; double sx,sy,sz; if (sscanf(s+5, "%63s %lf %lf %lf", sname, &sx,&sy,&sz) >= 4){ addSpawnPoint(sname, sx,sy,sz); }
            continue;
        }

        char *tokens[16]; int tcount=0;
        char *tok = strtok(s, " \t\r\n");
        while (tok && tcount < 16) { tokens[tcount++] = tok; tok = strtok(NULL, " \t\r\n"); }
        if (tcount >= 4){
            parse_object_tokens_and_add(tokens, tcount);
        }
    }
    fclose(f);
    return 1;
}
