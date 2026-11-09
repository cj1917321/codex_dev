#define _WIN32_WINNT 0x0601
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Freestyle Gomoku: 15x15, black moves first, five or more wins, no forbidden moves. */
enum { N=15,MAX_MOVES=N*N,W=1040,H=800,GX=70,GY=144,GAP=40,PX=706 };
typedef struct { int x,y,side; } Move;
typedef struct { int x,y; } Cell;
static int board[N][N],turn=1,winner=0,moveCount=0,computerMode=1;
static Move history[MAX_MOVES];
static Cell winStart={-1,-1},winEnd={-1,-1},hover={-1,-1};
static BOOL aiPending=FALSE;
static HWND gameWindow;
static HFONT titleFont,normalFont,smallFont,bigFont;
static float scale=1;
static int offsetX=0,offsetY=0;
static HDC canvasDC,frameDC;
static HBITMAP canvasBitmap,frameBitmap;
static HGDIOBJ oldCanvas,oldFrame;
static int frameWidth,frameHeight;
static const COLORREF BG=RGB(14,22,35),CARD=RGB(24,37,55),WHITE=RGB(237,244,251),MUTED=RGB(155,175,196),GREEN=RGB(111,234,185);
static const int directions[4][2]={{1,0},{0,1},{1,1},{1,-1}};
static BOOL inside(int x,int y) { return x>=0 && x<N && y>=0 && y<N; }
static void box(HDC dc,int x,int y,int width,int height,COLORREF color,int radius) {
    HBRUSH b=CreateSolidBrush(color); HGDIOBJ ob=SelectObject(dc,b),op=SelectObject(dc,GetStockObject(NULL_PEN));
    if(radius) RoundRect(dc,x,y,x+width,y+height,radius,radius); else Rectangle(dc,x,y,x+width,y+height);
    SelectObject(dc,ob); SelectObject(dc,op); DeleteObject(b);
}
static void circle(HDC dc,int x,int y,int r,COLORREF color) {
    HBRUSH b=CreateSolidBrush(color); HGDIOBJ ob=SelectObject(dc,b),op=SelectObject(dc,GetStockObject(NULL_PEN));
    Ellipse(dc,x-r,y-r,x+r+1,y+r+1); SelectObject(dc,ob); SelectObject(dc,op); DeleteObject(b);
}
static void line(HDC dc,int x1,int y1,int x2,int y2,COLORREF color,int width) {
    HPEN p=CreatePen(PS_SOLID,width,color); HGDIOBJ old=SelectObject(dc,p);
    MoveToEx(dc,x1,y1,NULL); LineTo(dc,x2,y2); SelectObject(dc,old); DeleteObject(p);
}
static void label(HDC dc,int x,int y,int width,int height,LPCWSTR s,HFONT f,COLORREF color,UINT align) {
    RECT r={x,y,x+width,y+height}; HGDIOBJ old=SelectObject(dc,f); SetBkMode(dc,TRANSPARENT); SetTextColor(dc,color);
    DrawTextW(dc,s,-1,&r,DT_SINGLELINE|DT_VCENTER|align); SelectObject(dc,old);
}
static HFONT makeFont(int size,int weight) {
    return CreateFontW(-size,0,0,0,weight,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
}
static void initFonts(void) { titleFont=makeFont(29,FW_BOLD); normalFont=makeFont(20,FW_BOLD); smallFont=makeFont(15,FW_NORMAL); bigFont=makeFont(38,FW_BOLD); }
static void freeFonts(void) { DeleteObject(titleFont); DeleteObject(normalFont); DeleteObject(smallFont); DeleteObject(bigFont); }
static void refresh(void) { if(gameWindow) InvalidateRect(gameWindow,NULL,FALSE); }
static void cancelAI(void) { if(gameWindow) KillTimer(gameWindow,1); aiPending=FALSE; }
static void resetGame(void) {
    cancelAI(); ZeroMemory(board,sizeof(board)); moveCount=0; turn=1; winner=0;
    winStart=winEnd=hover=(Cell){-1,-1}; refresh();
}
static void setMode(int mode) { if(mode!=computerMode) { computerMode=mode; resetGame(); } }
static int countRay(int x,int y,int dx,int dy,int side) {
    int n=0; x+=dx; y+=dy;
    while(inside(x,y) && board[y][x]==side) { n++; x+=dx; y+=dy; }
    return n;
}
static BOOL winningAt(int x,int y,int side,Cell *start,Cell *end) {
    for(int d=0;d<4;d++) {
        int dx=directions[d][0],dy=directions[d][1];
        int a=countRay(x,y,dx,dy,side),b=countRay(x,y,-dx,-dy,side);
        if(1+a+b>=5) {
            if(start) *start=(Cell){x-dx*b,y-dy*b};
            if(end) *end=(Cell){x+dx*a,y+dy*a};
            return TRUE;
        }
    }
    return FALSE;
}
static BOOL placeStone(int x,int y) {
    if(winner || !inside(x,y) || board[y][x] || moveCount>=MAX_MOVES) return FALSE;
    board[y][x]=turn; history[moveCount++]=(Move){x,y,turn}; hover=(Cell){-1,-1};
    if(winningAt(x,y,turn,&winStart,&winEnd)) winner=turn;
    else if(moveCount==MAX_MOVES) winner=3;
    if(!winner) turn=3-turn;
    return TRUE;
}
static BOOL nearStone(int x,int y) {
    for(int dy=-2;dy<=2;dy++) for(int dx=-2;dx<=2;dx++)
        if(inside(x+dx,y+dy) && board[y+dy][x+dx]) return TRUE;
    return FALSE;
}
/* Evaluate open lines and single gaps. Immediate wins/blocks are handled first. */
static long shapeScore(int x,int y,int side) {
    long score=0; int threes=0,fours=0;
    board[y][x]=side;
    for(int d=0;d<4;d++) {
        int dx=directions[d][0],dy=directions[d][1];
        int a=countRay(x,y,dx,dy,side),b=countRay(x,y,-dx,-dy,side),n=1+a+b;
        int ax=x+dx*(a+1),ay=y+dy*(a+1),bx=x-dx*(b+1),by=y-dy*(b+1);
        int open=(inside(ax,ay) && !board[ay][ax])+(inside(bx,by) && !board[by][bx]);
        if(n>=5) score+=10000000;
        else if(n==4 && open) { score+=open==2?100000:20000; fours++; }
        else if(n==3 && open) { score+=open==2?6000:400; if(open==2) threes++; }
        else if(n==2 && open) score+=open==2?300:30;
        else if(open==2) score+=12;
        for(int offset=-4;offset<=0;offset++) {
            int stones=0,empty=0; BOOL valid=TRUE;
            for(int k=0;k<5;k++) {
                int cx=x+dx*(offset+k),cy=y+dy*(offset+k);
                if(!inside(cx,cy) || board[cy][cx]==3-side) { valid=FALSE; break; }
                if(board[cy][cx]==side) stones++; else empty++;
            }
            if(valid && stones==4 && empty==1) score+=12000;
            else if(valid && stones==3 && empty==2) score+=180;
        }
    }
    if(threes>=2) score+=28000;
    if(fours && threes) score+=40000;
    board[y][x]=0; return score;
}
static Cell chooseAI(void) {
    if(moveCount==0) return (Cell){7,7};
    Cell best={-1,-1},block={-1,-1}; long bestScore=-1;
    for(int y=0;y<N;y++) for(int x=0;x<N;x++) {
        if(board[y][x] || !nearStone(x,y)) continue;
        /* Win this turn before defending an opponent's threat. */
        if(winningAt(x,y,2,NULL,NULL)) return (Cell){x,y};
        if(winningAt(x,y,1,NULL,NULL)) block=(Cell){x,y};
        long attack=shapeScore(x,y,2),defend=shapeScore(x,y,1);
        long score=attack*10+defend*11+28-abs(x-7)-abs(y-7);
        if(score>bestScore) { bestScore=score; best=(Cell){x,y}; }
    }
    if(block.x>=0) return block;
    if(best.x>=0) return best;
    for(int y=0;y<N;y++) for(int x=0;x<N;x++) if(!board[y][x]) return (Cell){x,y};
    return (Cell){-1,-1};
}
static void scheduleAI(void) {
    if(computerMode && turn==2 && !winner) {
        aiPending=TRUE;
        if(gameWindow) SetTimer(gameWindow,1,320,NULL);
    }
}
static void humanMove(int x,int y) {
    if(aiPending || (computerMode && turn!=1)) return;
    if(placeStone(x,y)) { scheduleAI(); refresh(); }
}
static void runAI(void) {
    BOOL valid=aiPending && computerMode && turn==2 && !winner;
    cancelAI();
    if(valid) { Cell c=chooseAI(); if(c.x>=0) placeStone(c.x,c.y); }
    refresh();
}
static void undo(void) {
    if(!moveCount) return;
    int amount=1;
    if(computerMode && !aiPending && history[moveCount-1].side==2 && moveCount>=2) amount=2;
    cancelAI();
    for(int i=0;i<amount;i++) { Move m=history[--moveCount]; board[m.y][m.x]=0; turn=m.side; }
    winner=0; winStart=winEnd=hover=(Cell){-1,-1}; refresh();
}
static Cell cellAt(int x,int y) {
    int col=(int)floor((double)(x-GX)/GAP+.5),row=(int)floor((double)(y-GY)/GAP+.5);
    if(!inside(col,row) || abs(x-(GX+col*GAP))>18 || abs(y-(GY+row*GAP))>18) return (Cell){-1,-1};
    return (Cell){col,row};
}
static void stone(HDC dc,int x,int y,int side,BOOL preview) {
    if(preview) { circle(dc,x,y,12,side==1?RGB(132,122,105):RGB(228,219,198)); return; }
    circle(dc,x+2,y+3,17,RGB(153,125,86));
    circle(dc,x,y,17,side==1?RGB(11,19,30):RGB(147,153,155));
    circle(dc,x-1,y-1,15,side==1?RGB(32,43,56):RGB(244,246,240));
    circle(dc,x-5,y-6,4,side==1?RGB(60,74,87):RGB(255,255,253));
}
static void button(HDC dc,int x,int y,int width,int height,LPCWSTR s,BOOL active,BOOL enabled) {
    box(dc,x,y,width,height,!enabled?RGB(32,44,60):active?RGB(38,101,81):RGB(42,59,81),9);
    label(dc,x,y,width,height,s,normalFont,!enabled?RGB(99,116,137):active?GREEN:WHITE,DT_CENTER);
}
static void render(HDC dc) {
    WCHAR s[100]; box(dc,0,0,W,H,BG,0);
    label(dc,24,18,620,43,L"五 子 棋  /  G O M O K U",titleFont,WHITE,DT_LEFT);
    label(dc,26,64,650,23,L"十五路棋盘 · 黑棋先行 · 横、竖、斜任一方向连成五子即获胜",smallFont,MUTED,DT_LEFT);
    box(dc,24,96,652,652,RGB(210,180,132),16);
    box(dc,34,106,632,632,RGB(224,197,153),12);
    for(int i=0;i<N;i++) {
        line(dc,GX+i*GAP,GY,GX+i*GAP,GY+14*GAP,RGB(127,103,71),1);
        line(dc,GX,GY+i*GAP,GX+14*GAP,GY+i*GAP,RGB(127,103,71),1);
        swprintf(s,100,L"%c",L'A'+i); label(dc,GX+i*GAP-15,GY-31,30,22,s,smallFont,RGB(107,82,51),DT_CENTER);
        swprintf(s,100,L"%d",i+1); label(dc,GX-34,GY+i*GAP-12,25,24,s,smallFont,RGB(107,82,51),DT_CENTER);
    }
    const int stars[5][2]={{3,3},{11,3},{7,7},{3,11},{11,11}};
    for(int i=0;i<5;i++) circle(dc,GX+stars[i][0]*GAP,GY+stars[i][1]*GAP,4,RGB(103,79,50));
    if(winner==1 || winner==2) line(dc,GX+winStart.x*GAP,GY+winStart.y*GAP,GX+winEnd.x*GAP,GY+winEnd.y*GAP,RGB(241,148,57),7);
    for(int y=0;y<N;y++) for(int x=0;x<N;x++) if(board[y][x]) stone(dc,GX+x*GAP,GY+y*GAP,board[y][x],FALSE);
    if(hover.x>=0 && !winner && !aiPending && !(computerMode && turn==2) && !board[hover.y][hover.x]) stone(dc,GX+hover.x*GAP,GY+hover.y*GAP,turn,TRUE);
    if(moveCount) {
        Move last=history[moveCount-1]; int x=GX+last.x*GAP,y=GY+last.y*GAP;
        box(dc,x-3,y-3,6,6,RGB(245,103,89),2);
    }
    box(dc,PX,96,310,138,CARD,14);
    LPCWSTR status=winner==3?L"棋盘已满 · 和棋":winner==1?L"黑棋获胜！":winner==2?(computerMode?L"电脑获胜！":L"白棋获胜！"):aiPending?L"电脑思考中…":turn==1?L"轮到黑棋":L"轮到白棋";
    label(dc,PX+20,109,270,43,status,titleFont,winner?GREEN:WHITE,DT_LEFT);
    swprintf(s,100,L"已落 %d 子  /  第 %d 回合",moveCount,moveCount/2+1);
    label(dc,PX+20,164,270,27,s,normalFont,MUTED,DT_LEFT);
    label(dc,PX+20,197,270,24,computerMode?L"你执黑棋，电脑执白棋":L"两位玩家轮流落子",smallFont,MUTED,DT_LEFT);
    label(dc,PX+2,247,300,27,L"对弈模式（切换会重新开局）",smallFont,MUTED,DT_LEFT);
    button(dc,PX,282,149,44,L"人机对战 [1]",computerMode==1,TRUE);
    button(dc,PX+161,282,149,44,L"双人对弈 [2]",computerMode==0,TRUE);
    box(dc,PX,346,310,146,CARD,14);
    button(dc,PX+16,362,278,45,L"重新开始  [R]",TRUE,TRUE);
    button(dc,PX+16,424,278,45,L"悔棋  [U / Ctrl+Z]",FALSE,moveCount>0);
    box(dc,PX,512,310,236,CARD,14);
    label(dc,PX+20,526,270,31,L"落子指南",normalFont,WHITE,DT_LEFT);
    label(dc,PX+20,572,270,25,L"鼠标左键：在棋盘交叉点落子",smallFont,MUTED,DT_LEFT);
    label(dc,PX+20,604,270,25,L"红色标记：刚刚落下的棋子",smallFont,MUTED,DT_LEFT);
    label(dc,PX+20,636,270,25,L"人机悔棋：撤回最近一轮落子",smallFont,MUTED,DT_LEFT);
    label(dc,PX+20,668,270,25,L"自由规则：无禁手，长连也获胜",smallFont,MUTED,DT_LEFT);
    label(dc,PX+20,700,270,25,L"Esc 退出 · 窗口支持拖动缩放",smallFont,MUTED,DT_LEFT);
    label(dc,24,762,992,25,winner?L"本局结束。可悔棋继续推演，或按 R 开始新的一局。":L"点击空交叉点落子。人机模式下，电脑会优先争胜并防守你的连线。",smallFont,MUTED,DT_LEFT);
}
static void freeBuffers(void) {
    if(canvasDC) { SelectObject(canvasDC,oldCanvas); DeleteObject(canvasBitmap); DeleteDC(canvasDC); }
    if(frameDC) { SelectObject(frameDC,oldFrame); DeleteObject(frameBitmap); DeleteDC(frameDC); }
    canvasDC=frameDC=NULL; canvasBitmap=frameBitmap=NULL; frameWidth=frameHeight=0;
}
static BOOL prepareFrame(HDC reference,int width,int height) {
    if(width<=0 || height<=0) return FALSE;
    if(!canvasDC) {
        HDC dc=CreateCompatibleDC(reference); HBITMAP bmp=CreateCompatibleBitmap(reference,W,H);
        if(!dc || !bmp) { if(dc) DeleteDC(dc); if(bmp) DeleteObject(bmp); return FALSE; }
        canvasDC=dc; canvasBitmap=bmp; oldCanvas=SelectObject(dc,bmp);
    }
    if(!frameDC || frameWidth!=width || frameHeight!=height) {
        HDC dc=CreateCompatibleDC(reference); HBITMAP bmp=CreateCompatibleBitmap(reference,width,height);
        if(!dc || !bmp) { if(dc) DeleteDC(dc); if(bmp) DeleteObject(bmp); return FALSE; }
        if(frameDC) { SelectObject(frameDC,oldFrame); DeleteObject(frameBitmap); DeleteDC(frameDC); }
        frameDC=dc; frameBitmap=bmp; oldFrame=SelectObject(dc,bmp); frameWidth=width; frameHeight=height;
    }
    render(canvasDC); RECT r={0,0,width,height}; FillRect(frameDC,&r,(HBRUSH)GetStockObject(BLACK_BRUSH));
    scale=fminf((float)width/W,(float)height/H); int sw=(int)(W*scale),sh=(int)(H*scale);
    offsetX=(width-sw)/2; offsetY=(height-sh)/2;
    SetStretchBltMode(frameDC,HALFTONE); SetBrushOrgEx(frameDC,0,0,NULL);
    StretchBlt(frameDC,offsetX,offsetY,sw,sh,canvasDC,0,0,W,H,SRCCOPY); return TRUE;
}
static POINT logicalPoint(int x,int y) { return (POINT){(LONG)floor((x-offsetX)/scale),(LONG)floor((y-offsetY)/scale)}; }
static void clickAt(int x,int y) {
    if(x>=PX && x<PX+310) {
        if(y>=282 && y<326) { if(x<PX+149) setMode(1); else if(x>=PX+161) setMode(0); return; }
        if(x>=PX+16 && x<PX+294) {
            if(y>=362 && y<407) { resetGame(); return; }
            if(y>=424 && y<469) { undo(); return; }
        }
    }
    Cell c=cellAt(x,y); if(c.x>=0) humanMove(c.x,c.y);
}
static LRESULT CALLBACK windowProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_CREATE: gameWindow=hwnd; resetGame(); return 0;
    case WM_GETMINMAXINFO: {
        RECT r={0,0,832,640}; AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
        MINMAXINFO *m=(MINMAXINFO*)lp; m->ptMinTrackSize.x=r.right-r.left; m->ptMinTrackSize.y=r.bottom-r.top; return 0;
    }
    case WM_SIZE: refresh(); return 0;
    case WM_TIMER: if(wp==1) runAI(); return 0;
    case WM_LBUTTONDOWN: { POINT p=logicalPoint(GET_X_LPARAM(lp),GET_Y_LPARAM(lp)); clickAt(p.x,p.y); return 0; }
    case WM_MOUSEMOVE: {
        POINT p=logicalPoint(GET_X_LPARAM(lp),GET_Y_LPARAM(lp)); Cell c=cellAt(p.x,p.y);
        if(c.x!=hover.x || c.y!=hover.y) { hover=c; refresh(); }
        TRACKMOUSEEVENT track={sizeof(track),TME_LEAVE,hwnd,0}; TrackMouseEvent(&track); return 0;
    }
    case WM_MOUSELEAVE: hover=(Cell){-1,-1}; refresh(); return 0;
    case WM_KEYDOWN:
        if(wp==VK_ESCAPE) { DestroyWindow(hwnd); return 0; }
        if(lp & (1L<<30)) return 0;
        if(wp=='R') resetGame(); else if(wp=='U' || (wp=='Z' && (GetKeyState(VK_CONTROL)&0x8000))) undo();
        else if(wp=='1') setMode(1); else if(wp=='2') setMode(0);
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc=BeginPaint(hwnd,&ps); RECT r; GetClientRect(hwnd,&r);
        if(prepareFrame(dc,r.right,r.bottom)) BitBlt(dc,0,0,r.right,r.bottom,frameDC,0,0,SRCCOPY);
        EndPaint(hwnd,&ps); return 0;
    }
    case WM_DESTROY: cancelAI(); freeBuffers(); gameWindow=NULL; PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
#ifndef GOMOKU_TEST
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,PWSTR command,int show) {
    (void)previous; (void)command; SetProcessDPIAware(); initFonts();
    WNDCLASSW wc={0}; wc.hInstance=instance; wc.lpfnWndProc=windowProc; wc.lpszClassName=L"GomokuNativeGame";
    wc.hCursor=LoadCursor(NULL,IDC_HAND); wc.hIcon=LoadIcon(NULL,IDI_APPLICATION);
    if(!RegisterClassW(&wc)) { freeFonts(); return 1; }
    RECT r={0,0,W,H}; AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
    HWND hwnd=CreateWindowExW(0,wc.lpszClassName,L"五子棋 · Gomoku",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,NULL,NULL,instance,NULL);
    if(!hwnd) { freeBuffers(); freeFonts(); return 1; }
    ShowWindow(hwnd,show); UpdateWindow(hwnd);
    MSG msg; int result; while((result=GetMessageW(&msg,NULL,0,0))>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    freeFonts(); return result<0?1:(int)msg.wParam;
}
#endif
