#define SNAKE_TEST
#include "snake.c"
#include <assert.h>

int main(void) {
    srand(1);
    resetGame(TRUE);
    food=(Point){20,20};
    tick();
    assert(snake[0].x==9 && snake[0].y==12 && length==4);
    direction(-1,0);
    assert(nextX==1 && nextY==0);
    direction(0,-1);
    direction(-1,0);
    assert(nextX==0 && nextY==-1);
    tick();
    assert(snake[0].x==9 && snake[0].y==11);

    resetGame(TRUE);
    food=(Point){9,12};
    tick();
    assert(length==5 && score==10 && !occupied(food.x,food.y,length));

    resetGame(TRUE);
    snake[0]=(Point){23,12}; food=(Point){20,20};
    tick();
    assert(state==3);

    resetGame(TRUE);
    snake[0]=(Point){5,5}; snake[1]=(Point){6,5};
    food=(Point){20,20}; tick();
    assert(state==3);

    resetGame(TRUE);
    snake[0]=(Point){5,5}; snake[1]=(Point){5,6};
    snake[2]=(Point){6,6}; snake[3]=(Point){6,5};
    food=(Point){20,20}; tick();
    assert(state==1 && snake[0].x==6 && snake[0].y==5);

    state=2; Point before=snake[0]; tick();
    assert(snake[0].x==before.x && snake[0].y==before.y);

    resetGame(TRUE);
    length=COLS*ROWS-1;
    snake[0]=(Point){0,0};
    int n=1;
    for(int y=0;y<ROWS;y++) for(int x=0;x<COLS;x++)
        if(!(y==0 && (x==0 || x==1))) snake[n++]=(Point){x,y};
    food=(Point){1,0}; tick();
    assert(state==4 && length==COLS*ROWS);

    for(int points=0;points<=3000;points+=50) {
        score=points;
        chooseDifficulty(0); int easy=speedMs();
        chooseDifficulty(1); int medium=speedMs();
        chooseDifficulty(2); int hard=speedMs();
        assert(easy>medium && medium>hard && hard>=50);
    }
    chooseDifficulty(1);
    resizeBoard(12,48);
    assert(COLS==12 && ROWS==48 && state==0 && length==4 && score==0);
    for(int i=0;i<length;i++) assert(snake[i].x>=0 && snake[i].x<COLS && snake[i].y<ROWS);
    assert(food.x<COLS && food.y<ROWS && !occupied(food.x,food.y,length));
    assert(CELL*COLS<=PANEL-56 && BY+CELL*ROWS<=clientHeight-38);
    Point saved=snake[0]; state=1; score=70;
    layout(1400,900);
    assert(state==1 && score==70 && snake[0].x==saved.x && snake[0].y==saved.y);
    assert(BX>=28 && BX+COLS*CELL<PANEL && BY+ROWS*CELL<=862);
    resizeBoard(10,48); assert(COLS==12 && ROWS==48);
    resizeBoard(48,12);
    assert(COLS==48 && ROWS==12 && state==0);
    resizeBoard(48,48);
    for(int i=0;i<1000;i++) { placeFood(); assert(food.x<48 && food.y<48 && !occupied(food.x,food.y,length)); }
    state=1; snake[0]=(Point){47,5}; dx=nextX=1; dy=nextY=0;
    tick(); assert(state==3);
    puts("PASS: original game logic, three difficulty speeds, board limits, rectangular/max boards, resize preserves progress, food placement.");
    return 0;
}
