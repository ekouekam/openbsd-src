#include <stdio.h>

static class sss {
public:
  char * m;
  virtual int f (int i) {return i;}
} sss;

#define _offsetof(st,f) ((char *)&((st *) 16)->f - (char *) 16)

int main (void) {
  printf ("+++Class starting with pointer and containing virtual function:\n");
  printf ("size=%d,align=%d\n", sizeof (sss), __alignof__ (sss));
  printf ("offset-m=%d,align-m=%d\n",
          _offsetof (class sss, m), __alignof__ (sss.m));
}
