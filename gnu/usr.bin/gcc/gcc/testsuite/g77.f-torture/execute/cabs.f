      program cabs_1
      complex      z0
      real         r0
      complex*16   z1
      real*8       r1

      z0 = cmplx(3.,4.)
      r0 = cabs(z0)
      if (r0 .ne. 5.) call abort

      z1 = dcmplx(3.d0,4.d0)
      r1 = zabs(z1)
      if (r1 .ne. 5.d0) call abort
      end
