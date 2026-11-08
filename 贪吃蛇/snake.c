#define _WIN32_WINNT 0x0601
#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Native Win32 / GDI game. No runtime or external assets required. */
enum { WIDTH=920, HEIGHT=710, MAX_SIDE=48, MAX_SNAKE=MAX_SIDE*MAX_SIDE };
static int COLS=24, ROWS=24, CELL=24, BX=28, BY=96, PANEL=634;
static int clientWidth=WIDTH, clientHeight=HEIGHT, difficulty=1;
static const WCHAR *difficultyNames[]={L"简单",L"中等",L"困难"};
typedef struct { int x,y; } Point;
static Point snake[MAX_SNAKE], food;
static int length, dx,dy,nextX,nextY, score,best=0, state=0;
static BOOL inputUsed=FALSE;
static HWND window;
static HFONT titleFont,normalFont,smallFont,scoreFont;
static const COLORREF background=RGB(14,20,32), board=RGB(20,29,44);

static void layout(int width,int height) {
    clientWidth=width; clientHeight=height; PANEL=width-286;
    int availableWidth=PANEL-56, availableHeight=height-134;
    CELL=availableWidth/COLS;
    if(CELL>availableHeight/ROWS) CELL=availableHeight/ROWS;
    if(CELL<1) CELL=1;
    BX=28+(availableWidth-COLS*CELL)/2;
    BY=96+(availableHeight-ROWS*CELL)/2;
}

static void box(HDC dc,int x,int y,int w,int h,COLORREF color,int radius) {
    HBRUSH b=CreateSolidBrush(color);
    HGDIOBJ oldB=SelectObject(dc,b),oldP=SelectObject(dc,GetStockObject(NULL_PEN));
    RoundRect(dc,x,y,x+w,y+h,radius,radius);
    SelectObject(dc,oldB); SelectObject(dc,oldP); DeleteObject(b);
}
static void label(HDC dc,int x,int y,int w,int h,LPCWSTR value,HFONT font,COLORREF color,UINT align) {
    RECT r={x,y,x+w,y+h}; HGDIOBJ old=SelectObject(dc,font);
    SetTextColor(dc,color); SetBkMode(dc,TRANSPARENT);
    DrawTextW(dc,value,-1,&r,DT_SINGLELINE|DT_VCENTER|align);
    SelectObject(dc,old);
}
static BOOL occupied(int x,int y,int count) {
    for(int i=0;i<count;i++) if(snake[i].x==x && snake[i].y==y) return TRUE;
    return FALSE;
}
static void placeFood(void) {
    if(length>=COLS*ROWS) return;
    int freeIndex=rand()%(COLS*ROWS-length);
    for(int y=0;y<ROWS;y++) for(int x=0;x<COLS;x++) {
        if(!occupied(x,y,length) && freeIndex--==0) { food=(Point){x,y}; return; }
    }
}
static int speedMs(void) {
    const int base[]={220,140,85}, minimum[]={140,85,50};
    int ms=base[difficulty]-(score/50)*5;
    return ms<minimum[difficulty]?minimum[difficulty]:ms;
}
static void setSpeed(void) {
    if(window) SetTimer(window,1,speedMs(),NULL);
}
static void resetGame(BOOL start) {
    length=4; score=0; dx=1; dy=0; nextX=1; nextY=0; inputUsed=FALSE;
    for(int i=0;i<length;i++) snake[i]=(Point){COLS/3-i,ROWS/2};
    state=start?1:0; placeFood(); setSpeed();
    if(window) InvalidateRect(window,NULL,FALSE);
}
static void chooseDifficulty(int value) {
    difficulty=value; setSpeed();
    if(window) InvalidateRect(window,NULL,FALSE);
}
static void resizeBoard(int cols,int rows) {
    if(cols<12 || cols>MAX_SIDE || rows<12 || rows>MAX_SIDE) return;
    if(cols==COLS && rows==ROWS) return;
    COLS=cols; ROWS=rows; layout(clientWidth,clientHeight); resetGame(FALSE);
}
static void tick(void) {
    if(state!=1) return;
    dx=nextX; dy=nextY; inputUsed=FALSE;
    Point head={snake[0].x+dx,snake[0].y+dy};
    BOOL eat=head.x==food.x && head.y==food.y;
    if(head.x<0 || head.x>=COLS || head.y<0 || head.y>=ROWS || occupied(head.x,head.y,length-(eat?0:1))) {
        state=3; if(window) InvalidateRect(window,NULL,FALSE); return;
    }
    int newLength=length+(eat?1:0);
    for(int i=newLength-1;i>0;i--) snake[i]=snake[i-1];
    snake[0]=head; length=newLength;
    if(eat) { score+=10; if(score>best) best=score; setSpeed();
        if(length==COLS*ROWS) state=4; else placeFood(); }
    if(window) InvalidateRect(window,NULL,FALSE);
}
static void direction(int x,int y) {
    if(state==0) state=1;
    if(state!=1 || inputUsed || (x==-dx && y==-dy)) return;
    nextX=x; nextY=y; inputUsed=TRUE;
    if(window) InvalidateRect(window,NULL,FALSE);
}
static void button(HDC dc,int x,int y,int w,int h,LPCWSTR text,BOOL active,BOOL enabled) {
    box(dc,x,y,w,h,active?RGB(39,109,83):RGB(38,52,72),8);
    label(dc,x,y,w,h,text,smallFont,enabled?(active?RGB(151,255,200):RGB(224,234,247)):RGB(88,105,126),DT_CENTER);
}
static void paint(HDC dc) {
    WCHAR text[80];
    box(dc,0,0,clientWidth,clientHeight,background,0);
    label(dc,28,19,350,46,L"S N A K E  /  贪吃蛇",titleFont,RGB(238,245,255),DT_LEFT);
    label(dc,30,62,530,23,L"选择难度与棋盘大小，也可拖动窗口边缘缩放。",smallFont,RGB(141,158,182),DT_LEFT);
    box(dc,BX-2,BY-2,COLS*CELL+4,ROWS*CELL+4,RGB(43,58,78),14);
    box(dc,BX,BY,COLS*CELL,ROWS*CELL,board,12);
    for(int y=0;y<ROWS;y++) for(int x=0;x<COLS;x++)
        if((x+y)%2==0) box(dc,BX+x*CELL,BY+y*CELL,CELL,CELL,RGB(22,32,48),0);
    if(state!=4) {
        int inset=CELL/5;
        box(dc,BX+food.x*CELL+inset,BY+food.y*CELL+inset,CELL-2*inset,CELL-2*inset,RGB(255,116,112),10);
    }
    for(int i=length-1;i>=0;i--) {
        int inset=CELL/12+1;
        int x=BX+snake[i].x*CELL+inset, y=BY+snake[i].y*CELL+inset;
        int size=CELL-2*inset;
        box(dc,x,y,size,size,i==0?RGB(117,245,181):RGB(46,184,135),7);
        if(i==0) {
            int ex1,ey1,ex2,ey2;
            int eye=size/7; if(eye<1) eye=1;
            if(dx) { ex1=ex2=x+(dx>0?size*3/4:size/5); ey1=y+size/5; ey2=y+size*2/3; }
            else { ey1=ey2=y+(dy>0?size*3/4:size/5); ex1=x+size/5; ex2=x+size*2/3; }
            box(dc,ex1,ey1,eye,eye,background,2); box(dc,ex2,ey2,eye,eye,background,2);
        }
    }
    box(dc,PANEL,96,258,128,RGB(25,36,53),16);
    label(dc,PANEL+22,111,210,28,L"当前得分",normalFont,RGB(153,173,196),DT_LEFT);
    swprintf(text,80,L"%03d",score);
    label(dc,PANEL+19,139,220,72,text,scoreFont,RGB(117,245,181),DT_LEFT);
    swprintf(text,80,L"本次最高 %d",best);
    label(dc,PANEL+133,110,120,28,text,smallFont,RGB(153,173,196),DT_LEFT);
    box(dc,PANEL,236,258,94,RGB(25,36,53),16);
    swprintf(text,80,L"难度 · 每步 %d 毫秒",speedMs());
    label(dc,PANEL+15,242,235,29,text,smallFont,RGB(173,189,211),DT_LEFT);
    for(int i=0;i<3;i++) button(dc,PANEL+15+i*78,280,72,34,difficultyNames[i],difficulty==i,TRUE);
    box(dc,PANEL,342,258,146,RGB(25,36,53),16);
    label(dc,PANEL+15,347,230,28,L"棋盘大小（12—48 格）",normalFont,RGB(235,243,253),DT_LEFT);
    swprintf(text,80,L"宽度  %d 格",COLS);
    label(dc,PANEL+15,380,133,32,text,smallFont,RGB(173,189,211),DT_LEFT);
    button(dc,PANEL+151,380,38,30,L"−",FALSE,COLS>12);
    button(dc,PANEL+201,380,38,30,L"+",FALSE,COLS<48);
    swprintf(text,80,L"高度  %d 格",ROWS);
    label(dc,PANEL+15,418,133,32,text,smallFont,RGB(173,189,211),DT_LEFT);
    button(dc,PANEL+151,418,38,30,L"−",FALSE,ROWS>12);
    button(dc,PANEL+201,418,38,30,L"+",FALSE,ROWS<48);
    label(dc,PANEL+15,455,236,25,L"改格数重开；缩放保留进度",smallFont,RGB(153,173,196),DT_LEFT);
    box(dc,PANEL,500,258,118,RGB(25,36,53),16);
    label(dc,PANEL+15,508,230,25,L"方向键 / WASD 移动",smallFont,RGB(173,189,211),DT_LEFT);
    label(dc,PANEL+15,535,230,25,L"R / Enter 重开 · Esc 退出",smallFont,RGB(173,189,211),DT_LEFT);
    label(dc,PANEL+15,562,230,25,L"数字 1 / 2 / 3 快速切换难度",smallFont,RGB(173,189,211),DT_LEFT);
    label(dc,PANEL+15,589,230,25,L"空格 / P 暂停 · 每 50 分加速",smallFont,RGB(173,189,211),DT_LEFT);
    box(dc,PANEL,630,258,42,RGB(30,64,56),12);
    label(dc,PANEL,630,258,42,state==1?L"●  游戏进行中":state==2?L"Ⅱ  已暂停":state>=3?L"按 Enter 再来一局":L"按方向键开始",normalFont,RGB(117,245,181),DT_CENTER);
    label(dc,28,clientHeight-28,clientWidth-56,24,L"撞墙或碰到身体即结束  ·  无需安装  ·  离线运行",smallFont,RGB(105,125,150),DT_LEFT);
    if(state!=1) {
        int w=PANEL-72; if(w>456) w=456;
        int x=28+(PANEL-56-w)/2,y=96+(clientHeight-134-174)/2;
        box(dc,x-2,y-2,w+4,178,RGB(61,84,109),18);
        box(dc,x,y,w,174,RGB(18,28,43),16);
        LPCWSTR heading=state==0?L"准备好出发了吗？":state==2?L"休息一下":state==4?L"恭喜，填满整个棋盘！":L"游戏结束";
        label(dc,x,y+23,w,47,heading,titleFont,RGB(238,245,255),DT_CENTER);
        if(state>=3) { swprintf(text,80,L"本局得分：%d  ·  按 Enter 或 R 再来一局",score); }
        else if(state==2) lstrcpyW(text,L"按空格或 P 继续游戏");
        else lstrcpyW(text,L"按方向键、WASD 或空格开始");
        label(dc,x+10,y+88,w-20,35,text,smallFont,RGB(161,185,209),DT_CENTER);
    }
}
static LRESULT CALLBACK WindowProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_CREATE: window=hwnd; layout(WIDTH,HEIGHT); resetGame(FALSE); return 0;
    case WM_GETMINMAXINFO: {
        RECT r={0,0,840,710};
        AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
        MINMAXINFO *info=(MINMAXINFO*)lp;
        info->ptMinTrackSize.x=r.right-r.left;
        info->ptMinTrackSize.y=r.bottom-r.top;
        return 0;
    }
    case WM_SIZE:
        if(wp!=SIZE_MINIMIZED) { layout(LOWORD(lp),HIWORD(lp)); InvalidateRect(hwnd,NULL,FALSE); }
        else if(state==1) state=2;
        return 0;
    case WM_ENTERSIZEMOVE:
        if(state==1) { state=2; InvalidateRect(hwnd,NULL,FALSE); } return 0;
    case WM_LBUTTONDOWN: {
        int x=GET_X_LPARAM(lp)-PANEL,y=GET_Y_LPARAM(lp);
        if(y>=280 && y<314) {
            for(int i=0;i<3;i++) if(x>=15+i*78 && x<87+i*78) chooseDifficulty(i);
        }
        int change=x>=151 && x<189?-2:x>=201 && x<239?2:0;
        if(y>=380 && y<410 && change) resizeBoard(COLS+change,ROWS);
        if(y>=418 && y<448 && change) resizeBoard(COLS,ROWS+change);
        return 0;
    }
    case WM_TIMER: tick(); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_KILLFOCUS:
        if(state==1) { state=2; InvalidateRect(hwnd,NULL,FALSE); } return 0;
    case WM_KEYDOWN:
        if(wp==VK_ESCAPE) { DestroyWindow(hwnd); return 0; }
        if((lp & (1L<<30)) && (wp==VK_SPACE || wp=='P' || wp=='R' || wp==VK_RETURN)) return 0;
        switch(wp) {
        case '1': case '2': case '3': chooseDifficulty((int)wp-'1'); break;
        case VK_LEFT: case 'A': direction(-1,0); break;
        case VK_RIGHT: case 'D': direction(1,0); break;
        case VK_UP: case 'W': direction(0,-1); break;
        case VK_DOWN: case 'S': direction(0,1); break;
        case VK_SPACE: case 'P':
            if(state==0 || state==2) { state=1; setSpeed(); }
            else if(state==1) state=2;
            break;
        case 'R': case VK_RETURN: resetGame(TRUE); break;
        }
        InvalidateRect(hwnd,NULL,FALSE); return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc=BeginPaint(hwnd,&ps);
        HDC buffer=CreateCompatibleDC(dc); HBITMAP bitmap=CreateCompatibleBitmap(dc,clientWidth,clientHeight);
        HGDIOBJ old=SelectObject(buffer,bitmap); paint(buffer);
        BitBlt(dc,0,0,clientWidth,clientHeight,buffer,0,0,SRCCOPY);
        SelectObject(buffer,old); DeleteObject(bitmap); DeleteDC(buffer); EndPaint(hwnd,&ps); return 0;
    }
    case WM_DESTROY: KillTimer(hwnd,1); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
static HFONT font(int size,int weight) {
    return CreateFontW(-size,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
}
#ifndef SNAKE_TEST
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,PWSTR command,int show) {
    (void)previous; (void)command;
    SetProcessDPIAware(); srand((unsigned)time(NULL));
    titleFont=font(27,FW_BOLD); normalFont=font(19,FW_SEMIBOLD);
    smallFont=font(15,FW_NORMAL); scoreFont=font(54,FW_BOLD);
    WNDCLASSW wc={0}; wc.lpfnWndProc=WindowProc; wc.hInstance=instance;
    wc.lpszClassName=L"SnakeNativeWindow"; wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.hIcon=LoadIcon(NULL,IDI_APPLICATION); RegisterClassW(&wc);
    RECT r={0,0,WIDTH,HEIGHT}; DWORD style=WS_OVERLAPPEDWINDOW;
    AdjustWindowRect(&r,style,FALSE);
    HWND hwnd=CreateWindowExW(0,wc.lpszClassName,L"贪吃蛇 · Snake",style,
        CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,NULL,NULL,instance,NULL);
    if(!hwnd) return 1;
    ShowWindow(hwnd,show); UpdateWindow(hwnd);
    MSG msg; int result;
    while((result=GetMessageW(&msg,NULL,0,0))>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    DeleteObject(titleFont); DeleteObject(normalFont); DeleteObject(smallFont); DeleteObject(scoreFont);
    return result<0?1:(int)msg.wParam;
}
#endif
