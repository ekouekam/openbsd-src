#include <stdio.h>

class c{
public:
  char * f;
};


static class sss: public c{
public:
  float m;
} sss;

#define _offsetof(st,f) ((char *)&((st *) 16)->f - (char *) 16)

int main (void) {
  printf ("++Class with float inhereting class with pointer:\n");
  printf ("size=%d,align=%d\n", sizeof (sss), __alignof__ (sss));
  printf ("offset-pointer=%d,offset-float=%d,\nalign-pointer=%d,align-float=%d\n",
          _offsetof (class sss, f), _offsetof (class sss, m),
          __alignof__ (sss.f), __alignof__ (sss.m));
  return 0;
}
