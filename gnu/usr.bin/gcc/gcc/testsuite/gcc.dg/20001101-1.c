/* ??? It'd be nice to run this for sparc32 as well, if we could know
   for sure that we're on an ultrasparc, rather than an older cpu.  */
/* { dg-do run { target sparcv9-*-* sparc64-*-* } } */
/* { dg-options "-O2 -m32 -mcpu=ultrasparc -mvis" } */

int foo(double a, int b, int c, double *d, int h)
{
  int f, g;
  double e;

l:
  f = (int) a;
  a -= (double) f;
  if (b == 1)
    {
      g = c;
      f += g;
      c -= g;
    }
  if (b == 2)
    {
      f++;
      h = c;
    }
  if (!h)
    {
      for (g = 0; g <= 10; g++)
        for (h = 0; h <= 10; h++)
          e += d [10 + g - h];
      goto l;
    }
  return f & 7;
}

int main()
{
  if (foo(0.1, 1, 3, 0, 1) != 3)
    abort ();
  exit (0);
}
