#define GOMOKU_TEST
#include "gomoku.c"
#include <assert.h>

static void assertAILeavesBoardUnchanged(void) {
    int saved[N][N]; CopyMemory(saved,board,sizeof(board));
    Cell c=chooseAI(); assert(inside(c.x,c.y) && !board[c.y][c.x]);
    assert(memcmp(saved,board,sizeof(board))==0);
}
static void savePreview(HDC dc,const char *path) {
    BITMAPINFO info={0}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=W;
    info.bmiHeader.biHeight=H; info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
    void *pixels=NULL; HDC out=CreateCompatibleDC(dc);
    HBITMAP bmp=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0); assert(bmp && pixels);
    HGDIOBJ old=SelectObject(out,bmp); render(out); GdiFlush();
    BITMAPFILEHEADER header={0}; header.bfType=0x4D42; header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER); header.bfSize=header.bfOffBits+W*H*4;
    FILE *f=fopen(path,"wb"); assert(f); fwrite(&header,sizeof(header),1,f); fwrite(&info.bmiHeader,sizeof(BITMAPINFOHEADER),1,f); fwrite(pixels,W*H*4,1,f); fclose(f);
    SelectObject(out,old); DeleteObject(bmp); DeleteDC(out);
}
int main(void) {
    resetGame(); assert(moveCount==0 && turn==1 && !winner);
    assert(!placeStone(-1,0) && !placeStone(15,0));
    assert(placeStone(7,7) && turn==2); assert(!placeStone(7,7));
    undo(); assert(moveCount==0 && turn==1 && !board[7][7]);
    for(int d=0;d<4;d++) for(int side=1;side<=2;side++) {
        resetGame(); int sx=d==3?3:1,sy=d==3?10:1;
        for(int i=0;i<4;i++) board[sy+i*directions[d][1]][sx+i*directions[d][0]]=side;
        turn=side; assert(placeStone(sx+4*directions[d][0],sy+4*directions[d][1]));
        assert(winner==side && !placeStone(14,14));
        assert(winStart.x==sx && winStart.y==sy);
    }
    resetGame(); for(int x=0;x<5;x++) board[0][x]=1;
    turn=1; assert(placeStone(5,0) && winner==1); /* Long lines also win. */
    resetGame(); board[0][0]=board[0][1]=board[0][2]=1;
    turn=1; assert(placeStone(3,0) && !winner);

    /* Computer must block immediate threats, including a gap and the edge. */
    for(int d=0;d<4;d++) {
        resetGame(); moveCount=4; int sx=0,sy=d==3?4:0;
        for(int i=0;i<4;i++) board[sy+i*directions[d][1]][sx+i*directions[d][0]]=1;
        Cell c=chooseAI(); assert(c.x==sx+4*directions[d][0] && c.y==sy+4*directions[d][1]);
        assertAILeavesBoardUnchanged();
    }
    resetGame(); moveCount=4; board[7][3]=board[7][4]=board[7][6]=board[7][7]=1;
    Cell c=chooseAI(); assert(c.x==5 && c.y==7);
    resetGame(); moveCount=8;
    for(int x=0;x<4;x++) { board[0][x]=1; board[2][x]=2; }
    c=chooseAI(); assert(c.x==4 && c.y==2); /* Win before blocking. */

    resetGame(); humanMove(7,7); assert(aiPending && moveCount==1);
    humanMove(8,7); assert(moveCount==1); /* Ignore clicks during AI turn. */
    undo(); assert(moveCount==0 && !aiPending && turn==1); runAI(); assert(moveCount==0);
    humanMove(7,7); runAI(); assert(moveCount==2 && turn==1 && !aiPending);
    undo(); assert(moveCount==0 && turn==1);
    humanMove(7,7); setMode(0); assert(!moveCount && !aiPending && !computerMode);
    humanMove(4,4); humanMove(5,5); assert(moveCount==2 && board[4][4]==1 && board[5][5]==2);
    undo(); assert(moveCount==1 && turn==2 && !board[5][5]);
    setMode(1); assert(moveCount==0 && turn==1);
    for(int x=0;x<4;x++) { humanMove(x,0); runAI(); if(winner) break; }
    assert(moveCount>0); resetGame();

    /* Full-board fixture has no five-in-a-row along any axis. */
    for(int y=0;y<N;y++) for(int x=0;x<N;x++) board[y][x]=(x+2*y)%4<2?1:2;
    for(int y=0;y<N;y++) for(int x=0;x<N;x++) assert(!winningAt(x,y,board[y][x],NULL,NULL));
    board[14][14]=0; moveCount=224; turn=2; assert(placeStone(14,14) && winner==3);
    c=chooseAI(); assert(c.x==-1);

    /* Hit tests at corners, outside the grid, and between intersections. */
    c=cellAt(GX,GY); assert(c.x==0 && c.y==0);
    c=cellAt(GX+14*GAP,GY+14*GAP); assert(c.x==14 && c.y==14);
    assert(cellAt(GX-21,GY).x==-1 && cellAt(GX+20,GY).x==-1);
    initFonts(); resetGame(); HDC reference=GetDC(NULL); assert(reference);
    assert(prepareFrame(reference,W,H)); HBITMAP reused=frameBitmap;
    DWORD resources=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    for(int i=0;i<30;i++) assert(prepareFrame(reference,W,H) && frameBitmap==reused);
    assert(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)==resources);
    assert(prepareFrame(reference,1400,900));
    POINT p=logicalPoint(offsetX+(int)((GX+7*GAP)*scale+.5f),offsetY+(int)((GY+7*GAP)*scale+.5f));
    c=cellAt(p.x,p.y); assert(c.x==7 && c.y==7);
    assert(prepareFrame(reference,832,640));
    p=logicalPoint(offsetX+(int)(GX*scale+.5f),offsetY+(int)(GY*scale+.5f)); c=cellAt(p.x,p.y); assert(c.x==0 && c.y==0);
    assert(!prepareFrame(reference,0,0));
    resetGame(); savePreview(reference,"preview-start.bmp");
    computerMode=0; resetGame();
    for(int i=0;i<4;i++) { placeStone(4+i,7); placeStone(5+i,8); }
    savePreview(reference,"preview-playing.bmp"); placeStone(8,7);
    assert(winner==1); savePreview(reference,"preview-win.bmp");
    freeBuffers(); ReleaseDC(NULL,reference); freeFonts();
    puts("PASS: four win axes, both sides, overline, draw, illegal moves, AI win/block/gap, board preservation, undo, mode switch, pending AI cancellation, scaled mouse hits, rendering, GDI resources.");
    return 0;
}
