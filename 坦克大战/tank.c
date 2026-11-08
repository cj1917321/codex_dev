#define _WIN32_WINNT 0x0601
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Standalone Win32 tank game. Simulation uses a fixed 60 Hz timestep. */
enum { W=1000,H=740,MX=24,MY=96,TILE=32,MW=20,MH=18,MAX_ENEMIES=12,MAX_BULLETS=160,MAX_FX=80 };
enum { TITLE,PLAY,PAUSE,CLEAR,LOSE,WIN };
typedef struct { float x,y; } Vec;
typedef struct { float x,y,hp,maxhp,r,speed,cool,think; int dir,kind,alive; } Tank;
typedef struct { float x,y,vx,vy,life; int damage,enemy,alive; } Bullet;
typedef struct { float x,y,life,maxlife; int color; } Effect;
static Tank player,enemies[MAX_ENEMIES];
static Bullet bullets[MAX_BULLETS];
static Effect effects[MAX_FX];
static int map[MH][MW],keys[256],mode=TITLE,level=1,kills=0,total=0,points=0;
static float invincible=0,elapsed=0;
static HWND gameWindow;
static HFONT heading,normal,small,big;
static LARGE_INTEGER frequency,lastTime;
static double accumulator;
typedef struct {
    HDC scene,output;
    HBITMAP sceneBitmap,outputBitmap;
    HGDIOBJ oldScene,oldOutput;
    int width,height;
} FrameBuffers;
static FrameBuffers frames;
static const int dirX[]={0,1,0,-1},dirY[]={-1,0,1,0};
static const int controlKeys[]={'W','A','S','D',VK_UP,VK_RIGHT,VK_DOWN,VK_LEFT,VK_SPACE,'J'};
typedef SHORT (WINAPI *KeyStateReader)(int);
static BOOL isControlKey(WPARAM key) {
    for(unsigned i=0;i<sizeof(controlKeys)/sizeof(controlKeys[0]);i++)
        if(key==(WPARAM)controlKeys[i]) return TRUE;
    return FALSE;
}
/* Continuous actions use the physical held state, independently of keyboard
   repeat messages and IME/character message processing. Only poll our window. */
static void sampleControls(BOOL foreground,KeyStateReader readKey) {
    for(unsigned i=0;i<sizeof(controlKeys)/sizeof(controlKeys[0]);i++) {
        int key=controlKeys[i];
        keys[key]=foreground && (readKey(key)&0x8000)!=0;
    }
}
static const COLORREF BG=RGB(12,18,29),WHITE=RGB(237,244,252),MUTED=RGB(149,169,193),GREEN=RGB(99,236,178);
static float limit(float v,float lo,float hi) { return v<lo?lo:v>hi?hi:v; }
static float distance(float x,float y,float a,float b) { float dx=x-a,dy=y-b; return sqrtf(dx*dx+dy*dy); }
static void rect(HDC dc,int x,int y,int w,int h,COLORREF c,int radius) {
    HBRUSH b=CreateSolidBrush(c); HGDIOBJ ob=SelectObject(dc,b),op=SelectObject(dc,GetStockObject(NULL_PEN));
    if(radius) RoundRect(dc,x,y,x+w,y+h,radius,radius); else Rectangle(dc,x,y,x+w,y+h);
    SelectObject(dc,ob); SelectObject(dc,op); DeleteObject(b);
}
static void text(HDC dc,int x,int y,int w,int h,LPCWSTR value,HFONT f,COLORREF c,UINT align) {
    RECT r={x,y,x+w,y+h}; HGDIOBJ old=SelectObject(dc,f); SetBkMode(dc,TRANSPARENT); SetTextColor(dc,c);
    DrawTextW(dc,value,-1,&r,DT_SINGLELINE|DT_VCENTER|align); SelectObject(dc,old);
}
static HFONT makeFont(int size,int weight) {
    return CreateFontW(-size,0,0,0,weight,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
}
static void initFonts(void) { heading=makeFont(28,FW_BOLD); normal=makeFont(19,FW_BOLD); small=makeFont(15,FW_NORMAL); big=makeFont(48,FW_BOLD); }
static void freeFonts(void) { DeleteObject(heading); DeleteObject(normal); DeleteObject(small); DeleteObject(big); }
static void effect(float x,float y,int color) {
    for(int i=0;i<MAX_FX;i++) if(effects[i].life<=0) { effects[i]=(Effect){x,y,.36f,.36f,color}; return; }
}
static BOOL wallCollision(float x,float y,float r) {
    if(x-r<0 || y-r<0 || x+r>MW*TILE || y+r>MH*TILE) return TRUE;
    for(int row=(int)((y-r)/TILE);row<=(int)((y+r)/TILE);row++)
        for(int col=(int)((x-r)/TILE);col<=(int)((x+r)/TILE);col++) {
            if(row<0 || col<0 || row>=MH || col>=MW) continue;
            if(map[row][col]) {
                float cx=limit(x,(float)col*TILE,(float)(col+1)*TILE),cy=limit(y,(float)row*TILE,(float)(row+1)*TILE);
                if(distance(x,y,cx,cy)<r) return TRUE;
            }
        }
    return FALSE;
}
static BOOL moveTank(Tank *t,float dx,float dy) {
    float x=t->x+dx,y=t->y+dy;
    if(wallCollision(x,y,t->r)) return FALSE;
    if(t!=&player && player.alive && distance(x,y,player.x,player.y)<t->r+player.r) return FALSE;
    for(int i=0;i<MAX_ENEMIES;i++) if(enemies[i].alive && t!=&enemies[i] && distance(x,y,enemies[i].x,enemies[i].y)<t->r+enemies[i].r) return FALSE;
    t->x=x; t->y=y; return TRUE;
}
static void spawnBullet(Tank *t,float angle,int enemy,int damage) {
    for(int i=0;i<MAX_BULLETS;i++) if(!bullets[i].alive) {
        float vx=sinf(angle),vy=-cosf(angle),speed=enemy?(t->kind==3?205:180):370;
        bullets[i]=(Bullet){t->x+vx*(t->r+7),t->y+vy*(t->r+7),vx*speed,vy*speed,5,damage,enemy,1}; return;
    }
}
static void fire(Tank *t,int enemy) {
    if(t->cool>0) return;
    float angle=(float)t->dir*1.570796327f;
    spawnBullet(t,angle,enemy,enemy?(t->kind==3?16:t->kind==2?13:9):20);
    if(t->kind==3) {
        spawnBullet(t,angle-.28f,enemy,16); spawnBullet(t,angle+.28f,enemy,16);
        if(t->hp<t->maxhp*.45f) { spawnBullet(t,angle-.55f,enemy,16); spawnBullet(t,angle+.55f,enemy,16); }
    }
    t->cool=enemy?(t->kind==3?(t->hp<t->maxhp*.45f?.70f:1.05f):t->kind==2?1.25f:1.65f):.22f;
}
static void damagePlayer(int damage) {
    if(invincible>0 || !player.alive) return;
    player.hp-=damage; invincible=.65f; effect(player.x,player.y,0);
    if(player.hp<=0) { player.hp=0; player.alive=0; mode=LOSE; }
}
static void damageEnemy(Tank *t,int damage) {
    if(!t->alive) return;
    t->hp-=damage; effect(t->x,t->y,1);
    if(t->hp<=0) { t->alive=0; kills++; points+=t->kind==3?1000:t->kind==2?200:100; }
}
static void clearSpawn(float x,float y,float r) {
    for(int row=0;row<MH;row++) for(int col=0;col<MW;col++)
        if(distance(x,y,col*TILE+16.0f,row*TILE+16.0f)<r+30) map[row][col]=0;
}
static void addEnemy(int index,float x,float y,int kind) {
    int hp=kind==3?700:kind==2?100:40;
    enemies[index]=(Tank){x,y,(float)hp,(float)hp,kind==3?27.0f:13.0f,kind==3?45.0f:kind==2?62.0f:42.0f,1.0f+index*.25f,.6f,2,kind,1};
    clearSpawn(x,y,enemies[index].r);
}
static void loadLevel(int n) {
    level=n; kills=0; elapsed=0; invincible=1.5f;
    ZeroMemory(map,sizeof(map)); ZeroMemory(enemies,sizeof(enemies)); ZeroMemory(bullets,sizeof(bullets)); ZeroMemory(effects,sizeof(effects)); ZeroMemory(keys,sizeof(keys));
    for(int row=3;row<=14;row+=3) for(int col=2;col<18;col++) {
        if(col==5 || col==6 || col==12 || col==13) continue;
        map[row][col]=n==1?1:((col+row)%4==0?2:1);
    }
    /* Keep clear lanes around cover; all enemy spawn points have exits. */
    player=(Tank){320,536,100,100,12,145,0,0,0,0,1}; clearSpawn(player.x,player.y,24);
    if(n==1) {
        total=6;
        for(int i=0;i<total;i++) addEnemy(i,64.0f+i*100,48.0f+(i%2)*128,1);
    } else if(n==2) {
        total=8;
        for(int i=0;i<total;i++) addEnemy(i,64.0f+(i%4)*168,48.0f+(i/4)*144,2);
    } else {
        total=5; addEnemy(0,320,55,3);
        addEnemy(1,64,64,2); addEnemy(2,568,64,2); addEnemy(3,64,240,1); addEnemy(4,568,240,1);
    }
    mode=PLAY;
}
static void newGame(void) { points=0; loadLevel(1); }
static int aimAtPlayer(Tank *t) {
    float dx=player.x-t->x,dy=player.y-t->y;
    return fabsf(dx)>fabsf(dy)?(dx>0?1:3):(dy>0?2:0);
}
static void updateBullets(float dt) {
    for(int i=0;i<MAX_BULLETS;i++) {
        Bullet *b=&bullets[i]; if(!b->alive) continue;
        b->life-=dt; if(b->life<=0) { b->alive=0; continue; }
        /* Substeps prevent fast bullets tunnelling through cover or tanks. */
        for(int sub=0;sub<3 && b->alive;sub++) {
            b->x+=b->vx*dt/3; b->y+=b->vy*dt/3;
            if(b->x<0 || b->y<0 || b->x>=MW*TILE || b->y>=MH*TILE) { b->alive=0; break; }
            int col=(int)b->x/TILE,row=(int)b->y/TILE;
            if(map[row][col]) {
                if(map[row][col]==1) map[row][col]=0;
                effect(b->x,b->y,2); b->alive=0; break;
            }
            if(b->enemy) {
                if(player.alive && distance(b->x,b->y,player.x,player.y)<player.r+3) { damagePlayer(b->damage); b->alive=0; }
            } else for(int j=0;j<MAX_ENEMIES;j++) {
                Tank *t=&enemies[j];
                if(t->alive && distance(b->x,b->y,t->x,t->y)<t->r+3) { damageEnemy(t,b->damage); b->alive=0; break; }
            }
        }
    }
}
static void step(float dt) {
    if(mode!=PLAY) return;
    elapsed+=dt; invincible=fmaxf(0,invincible-dt); player.cool=fmaxf(0,player.cool-dt);
    int dir=-1;
    if(keys['W'] || keys[VK_UP]) dir=0;
    else if(keys['D'] || keys[VK_RIGHT]) dir=1;
    else if(keys['S'] || keys[VK_DOWN]) dir=2;
    else if(keys['A'] || keys[VK_LEFT]) dir=3;
    if(dir>=0) { player.dir=dir; moveTank(&player,dirX[dir]*player.speed*dt,dirY[dir]*player.speed*dt); }
    if(keys[VK_SPACE] || keys['J']) fire(&player,0);
    for(int i=0;i<MAX_ENEMIES;i++) {
        Tank *t=&enemies[i]; if(!t->alive) continue;
        t->cool=fmaxf(0,t->cool-dt); t->think-=dt;
        if(t->think<=0) { t->dir=rand()%5==0?rand()%4:aimAtPlayer(t); t->think=.45f+(rand()%80)/100.0f; }
        if(!moveTank(t,dirX[t->dir]*t->speed*dt,dirY[t->dir]*t->speed*dt)) {
            t->dir=(t->dir+(rand()%2?1:3))%4; t->think=.35f;
        }
        if(t->cool<=0) { t->dir=aimAtPlayer(t); fire(t,1); }
        if(distance(player.x,player.y,t->x,t->y)<player.r+t->r+4) damagePlayer(t->kind==3?20:10);
    }
    updateBullets(dt);
    for(int i=0;i<MAX_FX;i++) effects[i].life=fmaxf(0,effects[i].life-dt);
    if(mode==PLAY && kills>=total) { mode=level==3?WIN:CLEAR; ZeroMemory(keys,sizeof(keys)); }
}
static void bar(HDC dc,int x,int y,int w,int h,float amount,COLORREF color) {
    rect(dc,x,y,w,h,RGB(47,58,75),5); int fill=(int)(w*limit(amount,0,1)); if(fill>0) rect(dc,x,y,fill,h,color,5);
}
static void drawTank(HDC dc,Tank *t) {
    if(!t->alive) return;
    if(t==&player && invincible>0 && ((int)(elapsed*14)%2)) return;
    int x=MX+(int)t->x,y=MY+(int)t->y,r=(int)t->r;
    COLORREF c=t==&player?GREEN:t->kind==3?RGB(200,117,249):t->kind==2?RGB(255,166,86):RGB(247,105,112);
    BOOL vertical=t->dir%2==0;
    rect(dc,x-r-3,y-r-3,r*2+6,r*2+6,RGB(9,14,22),5);
    if(vertical) { rect(dc,x-r-2,y-r,6,r*2,RGB(89,103,119),2); rect(dc,x+r-4,y-r,6,r*2,RGB(89,103,119),2); }
    else { rect(dc,x-r,y-r-2,r*2,6,RGB(89,103,119),2); rect(dc,x-r,y+r-4,r*2,6,RGB(89,103,119),2); }
    rect(dc,x-r+4,y-r+4,r*2-8,r*2-8,c,5);
    rect(dc,x-r/2,y-r/2,r,r,c,4);
    int tipX=x+dirX[t->dir]*(r+7),tipY=y+dirY[t->dir]*(r+7);
    if(vertical) rect(dc,x-3,tipY<y?tipY:y,6,abs(tipY-y)+3,c,2);
    else rect(dc,tipX<x?tipX:x,y-3,abs(tipX-x)+3,6,c,2);
    if(t!=&player) bar(dc,x-r,y-r-12,r*2,4,t->hp/t->maxhp,c);
    if(t->kind==3) text(dc,x-24,y-11,48,22,L"BOSS",small,RGB(28,15,42),DT_CENTER);
}
static void render(HDC dc) {
    WCHAR s[100]; rect(dc,0,0,W,H,BG,0);
    text(dc,24,17,600,43,L"装 甲 突 击  /  TANK BATTLE",heading,WHITE,DT_LEFT);
    text(dc,26,62,640,24,L"突破防线，消灭敌军，完成三关挑战。",small,MUTED,DT_LEFT);
    rect(dc,MX-2,MY-2,MW*TILE+4,MH*TILE+4,RGB(54,70,89),8);
    rect(dc,MX,MY,MW*TILE,MH*TILE,RGB(24,34,43),0);
    for(int row=0;row<MH;row++) for(int col=0;col<MW;col++) {
        int x=MX+col*TILE,y=MY+row*TILE;
        if((col+row)%2==0) rect(dc,x,y,TILE,TILE,RGB(27,38,47),0);
        if(map[row][col]==1) {
            rect(dc,x+1,y+1,30,30,RGB(129,87,65),2);
            rect(dc,x+1,y+14,30,2,RGB(67,48,44),0);
            rect(dc,x+14,y+1,2,13,RGB(67,48,44),0); rect(dc,x+7,y+16,2,15,RGB(67,48,44),0);
        } else if(map[row][col]==2) {
            rect(dc,x+1,y+1,30,30,RGB(88,107,127),3);
            rect(dc,x+5,y+5,22,3,RGB(135,153,169),0); rect(dc,x+5,y+24,22,3,RGB(48,65,83),0);
        }
    }
    for(int i=0;i<MAX_ENEMIES;i++) drawTank(dc,&enemies[i]);
    drawTank(dc,&player);
    for(int i=0;i<MAX_BULLETS;i++) if(bullets[i].alive) {
        Bullet *b=&bullets[i]; rect(dc,MX+(int)b->x-3,MY+(int)b->y-3,6,6,b->enemy?RGB(255,133,111):RGB(253,230,137),3);
    }
    for(int i=0;i<MAX_FX;i++) if(effects[i].life>0) {
        Effect *e=&effects[i]; int size=(int)(26*(1-e->life/e->maxlife))+5;
        COLORREF c=e->color==1?RGB(255,202,112):e->color==2?RGB(188,146,100):RGB(255,108,102);
        for(int j=0;j<8;j++) { float a=j*.785398f; rect(dc,MX+(int)(e->x+cosf(a)*size)-2,MY+(int)(e->y+sinf(a)*size)-2,4,4,c,2); }
    }
    int px=690;
    rect(dc,px,96,286,130,RGB(24,35,52),14);
    swprintf(s,100,L"第 %d / 3 关",level); text(dc,px+18,109,250,30,s,normal,GREEN,DT_LEFT);
    LPCWSTR names[]={L"",L"前线交锋 · 小兵",L"重装防线 · 大兵",L"终极决战 · BOSS"};
    text(dc,px+18,146,250,32,names[level],heading,WHITE,DT_LEFT);
    swprintf(s,100,L"已消灭 %d / %d    积分 %d",kills,total,points);
    text(dc,px+18,189,250,24,s,small,MUTED,DT_LEFT);
    rect(dc,px,242,286,91,RGB(24,35,52),14);
    swprintf(s,100,L"装甲耐久    %d / 100",(int)player.hp); text(dc,px+18,250,250,31,s,normal,WHITE,DT_LEFT);
    bar(dc,px+18,294,250,14,player.hp/100,player.hp>30?GREEN:RGB(255,110,112));
    rect(dc,px,349,286,146,RGB(24,35,52),14);
    text(dc,px+18,360,250,28,L"作战提示",normal,WHITE,DT_LEFT);
    LPCWSTR hint1=level==1?L"6 辆轻型小兵，2 发炮弹可击毁。":level==2?L"8 辆重装大兵，5 发炮弹可击毁。":L"Boss + 4 名护卫，全歼即可通关。";
    text(dc,px+18,401,250,24,hint1,small,MUTED,DT_LEFT);
    text(dc,px+18,430,250,24,level==3?L"Boss 低血量狂暴：五向弹幕！":L"砖墙可击碎，钢墙可以挡住炮弹。",small,MUTED,DT_LEFT);
    text(dc,px+18,459,250,24,L"每关补满耐久，利用掩体迂回。",small,MUTED,DT_LEFT);
    rect(dc,px,511,286,161,RGB(24,35,52),14);
    text(dc,px+18,523,250,28,L"操作指南",normal,WHITE,DT_LEFT);
    text(dc,px+18,560,250,24,L"WASD / 方向键    移动与瞄准",small,MUTED,DT_LEFT);
    text(dc,px+18,589,250,24,L"空格 / J 连射    P 暂停",small,MUTED,DT_LEFT);
    text(dc,px+18,618,250,24,L"Enter 开始 / 下一关    R 重开",small,MUTED,DT_LEFT);
    text(dc,px+18,644,250,22,L"Esc 退出    切出窗口自动暂停",small,MUTED,DT_LEFT);
    text(dc,24,694,950,25,L"绿色：我方  /  红色：小兵  /  橙色：大兵  /  紫色：Boss     ·     离线单机 · 无需安装",small,MUTED,DT_LEFT);
    if(level==3 && enemies[0].alive && mode==PLAY) {
        rect(dc,MX+100,MY+8,440,38,RGB(17,22,34),8);
        text(dc,MX+110,MY+10,100,30,L"BOSS",small,RGB(214,160,255),DT_LEFT);
        bar(dc,MX+177,MY+22,350,9,enemies[0].hp/enemies[0].maxhp,RGB(195,115,246));
    }
    if(mode!=PLAY) {
        int x=MX+48,y=MY+172; rect(dc,x-2,y-2,548,234,RGB(69,91,114),14); rect(dc,x,y,544,230,RGB(15,24,39),14);
        LPCWSTR title=mode==TITLE?L"装甲突击 · 三关挑战":mode==PAUSE?L"战斗已暂停":mode==CLEAR?L"防线突破！":mode==WIN?L"任务完成，指挥官！":L"坦克被击毁";
        text(dc,x,y+26,544,55,title,heading,mode==LOSE?RGB(255,132,126):WHITE,DT_CENTER);
        if(mode==TITLE) lstrcpyW(s,L"第一关小兵 → 第二关大兵 → 第三关 Boss");
        else if(mode==PAUSE) lstrcpyW(s,L"按 P 或 Enter 继续，保持你的火力。");
        else if(mode==CLEAR) swprintf(s,100,L"第 %d 关完成！下一关补满耐久，积分保留。",level);
        else if(mode==WIN) swprintf(s,100,L"已击败 Boss 并清除全部护卫！总积分：%d",points);
        else swprintf(s,100,L"到达第 %d 关 · 积分 %d · 按 R 重新挑战",level,points);
        text(dc,x+12,y+98,520,35,s,small,MUTED,DT_CENTER);
        text(dc,x+12,y+155,520,40,mode==TITLE?L"按 Enter 开始战斗":mode==CLEAR?L"按 Enter 进入下一关":mode==WIN?L"按 R 再次出击":mode==LOSE?L"按 Enter 重试本关 / R 从头开始":L"按 P / Enter 继续",normal,GREEN,DT_CENTER);
    }
}
static void togglePause(void) { if(mode==PLAY) mode=PAUSE; else if(mode==PAUSE) mode=PLAY; ZeroMemory(keys,sizeof(keys)); }
static void freeFrames(void) {
    if(frames.scene) { SelectObject(frames.scene,frames.oldScene); DeleteObject(frames.sceneBitmap); DeleteDC(frames.scene); }
    if(frames.output) { SelectObject(frames.output,frames.oldOutput); DeleteObject(frames.outputBitmap); DeleteDC(frames.output); }
    ZeroMemory(&frames,sizeof(frames));
}
/* Compose every pixel, including letterbox bars, off screen. Only the final
   BitBlt in WM_PAINT touches the window: no visible clear between frames. */
static HDC composeFrame(HDC reference,int width,int height) {
    if(width<=0 || height<=0) return NULL;
    if(!frames.scene) {
        HDC dc=CreateCompatibleDC(reference); HBITMAP bmp=CreateCompatibleBitmap(reference,W,H);
        if(!dc || !bmp) { if(dc) DeleteDC(dc); if(bmp) DeleteObject(bmp); return NULL; }
        frames.scene=dc; frames.sceneBitmap=bmp; frames.oldScene=SelectObject(dc,bmp);
    }
    if(!frames.output || frames.width!=width || frames.height!=height) {
        HDC dc=CreateCompatibleDC(reference); HBITMAP bmp=CreateCompatibleBitmap(reference,width,height);
        if(!dc || !bmp) { if(dc) DeleteDC(dc); if(bmp) DeleteObject(bmp); return NULL; }
        if(frames.output) { SelectObject(frames.output,frames.oldOutput); DeleteObject(frames.outputBitmap); DeleteDC(frames.output); }
        frames.output=dc; frames.outputBitmap=bmp; frames.oldOutput=SelectObject(dc,bmp);
        frames.width=width; frames.height=height;
    }
    render(frames.scene);
    RECT bounds={0,0,width,height}; FillRect(frames.output,&bounds,(HBRUSH)GetStockObject(BLACK_BRUSH));
    float scale=fminf((float)width/W,(float)height/H);
    int scaledW=(int)(W*scale),scaledH=(int)(H*scale);
    if(scaledW>0 && scaledH>0) {
        SetStretchBltMode(frames.output,HALFTONE); SetBrushOrgEx(frames.output,0,0,NULL);
        StretchBlt(frames.output,(width-scaledW)/2,(height-scaledH)/2,scaledW,scaledH,frames.scene,0,0,W,H,SRCCOPY);
    }
    return frames.output;
}
static LRESULT CALLBACK windowProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_CREATE: gameWindow=hwnd; QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&lastTime); SetTimer(hwnd,1,16,NULL); return 0;
    case WM_GETMINMAXINFO: { RECT r={0,0,800,592}; AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE); MINMAXINFO *m=(MINMAXINFO*)lp; m->ptMinTrackSize.x=r.right-r.left; m->ptMinTrackSize.y=r.bottom-r.top; return 0; }
    case WM_TIMER: {
        LARGE_INTEGER now; QueryPerformanceCounter(&now); double dt=(double)(now.QuadPart-lastTime.QuadPart)/frequency.QuadPart; lastTime=now;
        if(mode==PLAY && !IsIconic(hwnd)) {
            sampleControls(GetForegroundWindow()==hwnd,GetAsyncKeyState);
            accumulator+=dt>.1?.1:dt;
            while(accumulator>=1.0/60) { step(1.0f/60); accumulator-=1.0/60; }
            InvalidateRect(hwnd,NULL,FALSE);
        } else accumulator=0;
        return 0;
    }
    case WM_SIZE:
        if(wp==SIZE_MINIMIZED && mode==PLAY) { mode=PAUSE; ZeroMemory(keys,sizeof(keys)); }
        InvalidateRect(hwnd,NULL,FALSE); return 0;
    case WM_KILLFOCUS: case WM_ENTERSIZEMOVE:
        if(mode==PLAY) mode=PAUSE;
        ZeroMemory(keys,sizeof(keys)); InvalidateRect(hwnd,NULL,FALSE); return 0;
    case WM_KEYUP: if(wp<256) keys[wp]=0; return 0;
    case WM_KEYDOWN:
        if(wp==VK_ESCAPE) { DestroyWindow(hwnd); return 0; }
        /* Never discard auto-repeat for held movement/shooting controls. */
        if(isControlKey(wp)) { if(mode==PLAY) keys[wp]=1; return 0; }
        if(lp & (1L<<30)) return 0;
        if(wp=='P') togglePause();
        else if(wp=='R') newGame();
        else if(wp==VK_RETURN) {
            if(mode==TITLE || mode==WIN) newGame(); else if(mode==CLEAR) loadLevel(level+1);
            else if(mode==LOSE) loadLevel(level); else if(mode==PAUSE) togglePause();
        } else if(wp<256 && mode==PLAY) keys[wp]=1;
        InvalidateRect(hwnd,NULL,FALSE); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc=BeginPaint(hwnd,&ps); RECT r; GetClientRect(hwnd,&r);
        HDC complete=composeFrame(dc,r.right,r.bottom);
        if(complete) BitBlt(dc,0,0,r.right,r.bottom,complete,0,0,SRCCOPY);
        EndPaint(hwnd,&ps); return 0;
    }
    case WM_DESTROY: KillTimer(hwnd,1); freeFrames(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
#ifndef TANK_TEST
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,PWSTR command,int show) {
    (void)previous; (void)command; SetProcessDPIAware(); srand((unsigned)time(NULL)); initFonts();
    loadLevel(1); mode=TITLE;
    WNDCLASSW wc={0}; wc.hInstance=instance; wc.lpfnWndProc=windowProc; wc.lpszClassName=L"TankBattleNative";
    wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.hIcon=LoadIcon(NULL,IDI_APPLICATION); if(!RegisterClassW(&wc)) return 1;
    RECT r={0,0,W,H}; AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
    HWND hwnd=CreateWindowExW(0,wc.lpszClassName,L"坦克大战 · 装甲突击（连射修复版）",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,NULL,NULL,instance,NULL);
    if(!hwnd) { freeFonts(); return 1; } ShowWindow(hwnd,show); UpdateWindow(hwnd);
    MSG msg; int result; while((result=GetMessageW(&msg,NULL,0,0))>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    freeFonts(); return result<0?1:(int)msg.wParam;
}
#endif
