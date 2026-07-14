#include "common.h"
#include "render.h"
#include "input.h"
#include "game.h"
#include <SDL2/SDL.h>

typedef struct {
    int index;
    double depth;
} RenderObjectEntry;

static int compareRenderObjectDepth(const void *a, const void *b) {
    const RenderObjectEntry *ea = (const RenderObjectEntry *)a;
    const RenderObjectEntry *eb = (const RenderObjectEntry *)b;
    if (ea->depth < eb->depth) return 1;
    if (ea->depth > eb->depth) return -1;
    return 0;
}

int main(int argc, char **argv){
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0){
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *win = SDL_CreateWindow("Investigator - Wireframe (SDL)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, 0);
    if (!win){ fprintf(stderr, "CreateWindow failed: %s\n", SDL_GetError()); SDL_Quit(); return 1; }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren){ fprintf(stderr, "CreateRenderer failed: %s\n", SDL_GetError()); SDL_DestroyWindow(win); SDL_Quit(); return 1; }

    double camX = 0.0, camY = 1.2, camZ = -6.0;
    double ang = 0.0; double speed = 0.08; double rotSpeed = 0.04; double playerHitbox = 0.6;
    loadMap(currentMapFile);
    int lastLState = 0;
    int lastShootState = 0;

    int running = 1;
    Uint32 lastTick = SDL_GetTicks();
    while (running){
        SDL_Event ev;
        while (SDL_PollEvent(&ev)){
            if (ev.type == SDL_QUIT) running = 0;
        }

        Uint32 now = SDL_GetTicks();
        double dt = (now > lastTick) ? ((now - lastTick) / 1000.0) : 0.016;
        lastTick = now;

        /* update object movement/AI */
        updateObjects(dt, camX, camY, camZ);

        double speedCurr = 0.0;
        int shootPressed = 0;
        int reload = handleInput(&camX, &camY, &camZ, &ang, speed, rotSpeed, &speedCurr, &lastLState, &shootPressed, &lastShootState);
        if (reload) loadMap(currentMapFile);
        if (shootPressed) spawnPlayerShot(camX, camY, camZ, ang);
        updateProjectiles(dt, camX, camY, camZ);

        // clear sky
        Uint8 sr=100, sg=160, sb=240;
        SDL_SetRenderDrawColor(ren, sr,sg,sb,255);
        SDL_RenderClear(ren);

        // ground
        int horizon = WIN_H/2 - (int)((camY - 1.0) * 40);
        SDL_Rect ground = {0, horizon, WIN_W, WIN_H - horizon};
        SDL_SetRenderDrawColor(ren, 80,140,60,255);
        SDL_RenderFillRect(ren, &ground);

        // grid
        SDL_SetRenderDrawColor(ren, 200,200,200,255);
        int cx = WIN_W/2;
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
            SDL_RenderDrawLine(ren, x1, y1, x2, y2);
        }

        // draw objects via drawWire (white)
        if (mapObjectCount > 0) {
            RenderObjectEntry *renderOrder = malloc(sizeof(RenderObjectEntry) * mapObjectCount);
            for (int mi=0; mi<mapObjectCount; ++mi) {
                double dx = mapObjects[mi].x - camX;
                double dy = mapObjects[mi].y - camY;
                double dz = mapObjects[mi].z - camZ;
                renderOrder[mi].index = mi;
                renderOrder[mi].depth = dx*dx + dy*dy + dz*dz;
            }
            qsort(renderOrder, mapObjectCount, sizeof(RenderObjectEntry), compareRenderObjectDepth);
            for (int oi=0; oi<mapObjectCount; ++oi) {
                MapObject *mo = &mapObjects[renderOrder[oi].index];
                const Vec3 *verts = (mo->type==OBJ_CUBE ? cubeVerts : (mo->type==OBJ_PYRAMID ? pyramidVerts : NULL));
                int vcount = (mo->type==OBJ_CUBE ? cubeVertCount : (mo->type==OBJ_PYRAMID ? pyramidVertCount : 0));
                int (*edges)[2] = (mo->type==OBJ_CUBE ? cubeEdges : (mo->type==OBJ_PYRAMID ? pyramidEdges : NULL));
                int ecount = (mo->type==OBJ_CUBE ? cubeEdgeCount : (mo->type==OBJ_PYRAMID ? pyramidEdgeCount : 0));
                drawWire(ren, verts, vcount, edges, ecount, mo->x, mo->y, mo->z, camX,camY,camZ, ang, cx, horizon, mo->rx, mo->ry, mo->rz, (mo->type==OBJ_CUBE ? cubeFaces : (mo->type==OBJ_PYRAMID ? pyramidFaces : NULL)), (mo->type==OBJ_CUBE ? cubeFaceCount : (mo->type==OBJ_PYRAMID ? pyramidFaceCount : 0)), mo->color, mo->fillColor, mo->hasFillColor);
            }
            free(renderOrder);
        }

        for (int pi=0; pi<MAX_PROJECTILES; ++pi) {
            Projectile *pr = &projectiles[pi];
            if (!pr->active) continue;
            drawWire(ren, shotVerts, shotVertCount, shotEdges, shotEdgeCount, pr->x, pr->y, pr->z, camX,camY,camZ, ang, cx, horizon, 0.0, 0.0, pr->spin, NULL, 0, RGB(255,255,0), 0, 0);
        }

        // simple mini-map: black rect and small markers
        int mapLeft = 10, mapTop = 10, mapSize = 160; double mapScale = 8.0; int mapCx = mapLeft + mapSize/2; int mapCy = mapTop + mapSize/2;
        SDL_Rect mr = {mapLeft, mapTop, mapSize, mapSize};
        SDL_SetRenderDrawColor(ren, 0,0,0,255); SDL_RenderFillRect(ren, &mr);
        SDL_SetRenderDrawColor(ren, 200,200,200,255); SDL_RenderDrawRect(ren, &mr);
        for (int mi=0; mi<mapObjectCount; ++mi){
            MapObject *mo = &mapObjects[mi];
            int ox = mapCx + (int)((mo->x - camX) * mapScale);
            int oy = mapCy - (int)((mo->z - camZ) * mapScale);
            SDL_Rect dot = {ox-3, oy-3, 6,6};
            Uint8 r=(mo->color>>16)&0xFF, g=(mo->color>>8)&0xFF, b=mo->color&0xFF;
            SDL_SetRenderDrawColor(ren, r,g,b,255); SDL_RenderFillRect(ren, &dot);
        }

        SDL_RenderPresent(ren);

        Uint32 elapsed = SDL_GetTicks() - lastTick;
        if (elapsed < 16) SDL_Delay(16 - elapsed);
        lastTick = SDL_GetTicks();
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
