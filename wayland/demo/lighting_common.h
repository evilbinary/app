#ifndef LIGHTING_COMMON_H
#define LIGHTING_COMMON_H
/* per_fragment=0: 逐顶点光照（fplight/arbfplight）
 * per_fragment=1: 逐片元光照（fslight/arbfslight） */
int lighting_run(int per_fragment);
#endif
