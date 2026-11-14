/* SPDX-License-Identifier: Apache-2.0 */
#include "caro.h"
#include <assert.h>
#include <stdio.h>
static void win(int r,int c,int dr,int dc,int count) {
    caro_t g;caro_reset(&g);
    for(int i=0;i<count;++i) {
        assert(caro_play(&g,r+i*dr,c+i*dc));
        if(i+1<count)assert(caro_play(&g,14,i*2));
    }
    assert(g.winner==1);assert(caro_finished(&g));
    assert(!caro_play(&g,8,8));
    assert(caro_undo(&g));assert(!g.winner);assert(g.turn==1);
    assert(caro_play(&g,r+(count-1)*dr,c+(count-1)*dc));assert(g.winner==1);
}
int main(void) {
    caro_t g;caro_reset(&g);assert(!caro_undo(&g));
    assert(!caro_play(&g,-1,0));assert(!caro_play(&g,15,0));assert(!caro_play(&g,0,15));
    assert(caro_play(&g,0,0));assert(g.turn==2);assert(!caro_play(&g,0,0));assert(g.moves==1);
    assert(caro_play(&g,14,14));assert(caro_undo(&g));assert(g.turn==2);
    assert(caro_undo(&g));assert(g.turn==1);assert(!g.moves);
    win(0,0,0,1,5);win(0,0,1,0,5);win(0,0,1,1,5);win(0,14,1,-1,5);
    caro_reset(&g);
    for(int i=0;i<5;++i){assert(caro_play(&g,14,i*2));assert(caro_play(&g,0,i));}
    assert(g.winner==2);assert(caro_undo(&g));assert(g.turn==2&&!g.winner);
    // A middle stone joins two separated runs; six in a row also wins.
    caro_reset(&g);
    const int cols[]={0,1,2,4,5,3};
    for(int i=0;i<6;++i){assert(caro_play(&g,3,cols[i]));if(i<5)assert(caro_play(&g,14,i*2));}
    assert(g.winner==1);
    // Fill an alternating two-diagonal pattern with no five-stone lines.
    caro_reset(&g);
    for(int move=0;move<CARO_CELLS;++move) {
        bool played=false;
        for(int i=0;i<CARO_CELLS;++i) {
            int player=((i/CARO_SIZE+2*(i%CARO_SIZE))%4<2)?1:2;
            if(!g.cells[i]&&player==g.turn){assert(caro_play(&g,i/CARO_SIZE,i%CARO_SIZE));played=true;break;}
        }
        assert(played);assert(!g.winner);
    }
    assert(caro_finished(&g));assert(!caro_play(&g,0,0));
    assert(caro_undo(&g));assert(!caro_finished(&g));
    int last=g.history[g.moves];
    assert(caro_play(&g,last/CARO_SIZE,last%CARO_SIZE));assert(caro_finished(&g));
    caro_reset(&g);assert(g.turn==1&&!g.moves&&!g.winner);
    puts("caro rules: passed");
}
