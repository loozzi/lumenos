/* SPDX-License-Identifier: Apache-2.0 */
#include "caro.h"
#include <string.h>
void caro_reset(caro_t *g) { memset(g,0,sizeof(*g)); g->turn=1; }
bool caro_finished(const caro_t *g) { return g->winner || g->moves==CARO_CELLS; }
static int run(const caro_t *g, int r, int c, int dr, int dc) {
    int n=0;
    for(r+=dr,c+=dc; r>=0&&r<CARO_SIZE&&c>=0&&c<CARO_SIZE; r+=dr,c+=dc) {
        if(g->cells[r*CARO_SIZE+c]!=g->turn) break;
        ++n;
    }
    return n;
}
bool caro_play(caro_t *g, int r, int c) {
    if(r<0||r>=CARO_SIZE||c<0||c>=CARO_SIZE||caro_finished(g)) return false;
    int cell=r*CARO_SIZE+c;
    if(g->cells[cell]) return false;
    g->cells[cell]=g->turn;
    g->history[g->moves++]=(uint8_t)cell;
    static const int directions[4][2]={{0,1},{1,0},{1,1},{1,-1}};
    for(int i=0;i<4;++i) {
        int dr=directions[i][0],dc=directions[i][1];
        if(1+run(g,r,c,dr,dc)+run(g,r,c,-dr,-dc)>=5) g->winner=g->turn;
    }
    g->turn=3-g->turn;
    return true;
}
bool caro_undo(caro_t *g) {
    if(!g->moves) return false;
    int cell=g->history[--g->moves];
    g->turn=g->cells[cell];g->cells[cell]=0;g->winner=0;
    return true;
}
