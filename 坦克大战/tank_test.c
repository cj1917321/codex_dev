#define TANK_TEST
#include "tank.c"
#include <assert.h>

static int bulletCount(void) { int n=0; for(int i=0;i<MAX_BULLETS;i++) n+=bullets[i].alive; return n; }
static SHORT physicalKeys[256];
static SHORT WINAPI simulatedKeyState(int key) { return physicalKeys[key]; }
static void testContinuousFire(void) {
    loadLevel(1); ZeroMemory(map,sizeof(map)); ZeroMemory(enemies,sizeof(enemies));
    ZeroMemory(physicalKeys,sizeof(physicalKeys));
    /* A held space must work even if WM_KEYDOWN was consumed/missed. */
    physicalKeys[VK_SPACE]=(SHORT)0x8000;
    int shots=0;
    for(int i=0;i<120;i++) {
        if(i==30) ZeroMemory(keys,sizeof(keys)); /* Lost cached input state. */
        sampleControls(TRUE,simulatedKeyState);
        ZeroMemory(bullets,sizeof(bullets)); step(1.0f/60); shots+=bulletCount();
    }
    assert(shots>=9 && shots<=10);
    physicalKeys[VK_SPACE]=0;
    for(int i=0;i<60;i++) {
        sampleControls(TRUE,simulatedKeyState); ZeroMemory(bullets,sizeof(bullets)); step(1.0f/60);
        assert(bulletCount()==0);
    }
    /* Moving and firing simultaneously; J is also a continuous fire key. */
    physicalKeys['J']=(SHORT)0x8000; physicalKeys['D']=(SHORT)0x8000;
    float x=player.x; shots=0;
    for(int i=0;i<60;i++) {
        sampleControls(TRUE,simulatedKeyState); ZeroMemory(bullets,sizeof(bullets)); step(1.0f/60); shots+=bulletCount();
    }
    assert(shots>=4 && player.x>x);
    sampleControls(FALSE,simulatedKeyState);
    assert(!keys['J'] && !keys['D'] && !keys[VK_SPACE]);
    /* Repeat must restore a cleared held key; pause must not accept it. */
    windowProc(NULL,WM_KEYDOWN,VK_SPACE,(LPARAM)1<<30); assert(keys[VK_SPACE]);
    windowProc(NULL,WM_KEYUP,VK_SPACE,0); assert(!keys[VK_SPACE]);
    togglePause(); windowProc(NULL,WM_KEYDOWN,VK_SPACE,(LPARAM)1<<30); assert(!keys[VK_SPACE]);
    ZeroMemory(physicalKeys,sizeof(physicalKeys));
}
static void preview(const char *path) {
    BITMAPINFO info={0}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=W;
    info.bmiHeader.biHeight=H; info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
    void *pixels=NULL; HDC dc=CreateCompatibleDC(NULL);
    HBITMAP bmp=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0); assert(bmp && pixels);
    HGDIOBJ old=SelectObject(dc,bmp); render(dc); GdiFlush();
    BITMAPFILEHEADER header={0}; header.bfType=0x4D42; header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);
    header.bfSize=header.bfOffBits+W*H*4;
    FILE *f=fopen(path,"wb"); assert(f); fwrite(&header,sizeof(header),1,f); fwrite(&info.bmiHeader,sizeof(BITMAPINFOHEADER),1,f);
    fwrite(pixels,W*H*4,1,f); fclose(f); SelectObject(dc,old); DeleteObject(bmp); DeleteDC(dc);
}
int main(void) {
    srand(10);
    testContinuousFire();
    for(int n=1;n<=3;n++) {
        loadLevel(n); assert(mode==PLAY && level==n && player.hp==100 && kills==0);
        assert(total==(n==1?6:n==2?8:5)); assert(!wallCollision(player.x,player.y,player.r));
        for(int i=0;i<total;i++) assert(enemies[i].alive && !wallCollision(enemies[i].x,enemies[i].y,enemies[i].r));
    }
    loadLevel(1); ZeroMemory(map,sizeof(map)); ZeroMemory(enemies,sizeof(enemies));
    float x=player.x; keys['D']=1; step(1.0f/60); assert(player.x>x); keys['D']=0;
    togglePause(); x=player.x; step(1); assert(mode==PAUSE && player.x==x); togglePause(); assert(mode==PLAY);
    invincible=0; damagePlayer(9); assert(player.hp==91); damagePlayer(9); assert(player.hp==91);
    invincible=0; damagePlayer(100); assert(mode==LOSE && player.hp==0);

    loadLevel(1); ZeroMemory(map,sizeof(map)); ZeroMemory(bullets,sizeof(bullets));
    map[5][5]=1; bullets[0]=(Bullet){170,170,0,0,1,20,0,1}; updateBullets(.016f);
    assert(map[5][5]==0 && !bullets[0].alive);
    map[5][5]=2; bullets[0]=(Bullet){170,170,0,0,1,20,0,1}; updateBullets(.016f);
    assert(map[5][5]==2 && !bullets[0].alive);
    assert(!moveTank(&player,-1000,0));
    for(int i=0;i<total;i++) { damageEnemy(&enemies[i],20); assert(enemies[i].alive); damageEnemy(&enemies[i],20); assert(!enemies[i].alive); }
    step(.016f); assert(mode==CLEAR && points==600);
    loadLevel(2); assert(points==600 && enemies[0].hp==100);
    for(int i=0;i<total;i++) damageEnemy(&enemies[i],100);
    step(.016f); assert(mode==CLEAR);
    loadLevel(3); assert(enemies[0].kind==3 && enemies[0].hp==700);
    ZeroMemory(bullets,sizeof(bullets)); enemies[0].cool=0; fire(&enemies[0],1); assert(bulletCount()==3);
    ZeroMemory(bullets,sizeof(bullets)); enemies[0].cool=0; enemies[0].hp=300; fire(&enemies[0],1); assert(bulletCount()==5);
    damageEnemy(&enemies[0],700); step(.016f); assert(mode==PLAY);
    for(int i=1;i<total;i++) damageEnemy(&enemies[i],100);
    step(.016f); assert(mode==WIN);
    newGame(); assert(level==1 && points==0 && mode==PLAY);

    /* Render the same GDI code as the executable without opening a window. */
    initFonts(); mode=TITLE;
    /* Regression: test the complete presentation buffer, not just the scene. */
    HDC reference=GetDC(NULL); assert(reference);
    HDC frame=composeFrame(reference,W,H); assert(frame);
    assert(GetPixel(frame,0,0)==BG);
    COLORREF titlePixel=GetPixel(frame,100,280);
    assert(titlePixel!=RGB(0,0,0));
    HBITMAP reused=frames.outputBitmap;
    DWORD resourceCount=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    for(int i=0;i<120;i++) {
        assert(composeFrame(reference,W,H)==frame && frames.outputBitmap==reused);
        assert(GetPixel(frame,100,280)==titlePixel && GetPixel(frame,0,0)==BG);
    }
    assert(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)==resourceCount);
    frame=composeFrame(reference,1200,740); assert(frame && GetPixel(frame,0,370)==RGB(0,0,0));
    assert(GetPixel(frame,100,0)==BG);
    frame=composeFrame(reference,800,800); assert(frame && GetPixel(frame,400,0)==RGB(0,0,0));
    assert(GetPixel(frame,0,104)==BG);
    assert(!composeFrame(reference,0,0));
    freeFrames(); assert(!frames.scene && !frames.output); ReleaseDC(NULL,reference);
    preview("preview-title.bmp");
    loadLevel(1); invincible=0; preview("preview-level1.bmp");
    loadLevel(2); invincible=0; preview("preview-level2.bmp");
    loadLevel(3); invincible=0; preview("preview-boss.bmp"); freeFonts();
    puts("PASS: held Space/J continuous fire, key release, lost input recovery, move+fire, focus/pause safety; game logic; stable presentation and resize.");
    return 0;
}
