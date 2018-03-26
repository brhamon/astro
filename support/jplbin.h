/*
 * This file was adapted from Dr. Dybczynski's original source, downloaded
 * from: ftp://ftp.astro.amu.edu.pl/pub/jpleph/version_1.4/jplbin.h
 *
 * Thanks to the original author for placing these sources in the public
 * domain.
 */
/***************************************************************************
*******                  JPLBIN.H   v.1.4                          *********
****************************************************************************
**  This header file is used by TESTEPH program.                          **
**  It is NECESSARY TO ADJUST IT MANUALLY for different ephemerides.      **
****************************************************************************
**  Written: May 28, 1997 by PAD  **  Last modified: March 13,2014 by PAD **
****************************************************************************
**  PAD: dr. Piotr A. Dybczynski,          e-mail: dybol@amu.edu.pl       **
**   Astronomical Observatory of the A.Mickiewicz Univ., Poznan, Poland   **
***************************************************************************/

/* UNCOMMENT ONE AND ONLY ONE OF THE FOLLOWING DENUM DEFINITIONS: */
/* Note: de430 and de431 both work fine with DENUM==430 */

/*#define DENUM 200*/
/*#define DENUM 403*/
/*#define DENUM 404*/
/*#define DENUM 405*/
/*#define DENUM 406*/
/*#define DENUM 421*/
/*#define DENUM 422*/
#define DENUM 430
/* #define DENUM 431 */


#if   DENUM==200
#define KSIZE 1652
#elif DENUM==403 || DENUM==405 || DENUM==421 || DENUM==422 || DENUM==430 || DENUM==431
#define KSIZE 2036
#elif DENUM==404 || DENUM==406
#define KSIZE 1456
#endif

#define NRECL 4
#define RECSIZE (NRECL*KSIZE)
#define NCOEFF (KSIZE/2)
#define TRUE 1
#define FALSE 0

/* the followin two definitions adapt the software for larger number of constants 
(eg. for DE430 and DE431) */

#include <inttypes.h>

#define NMAX 1000
#define OLDMAX 400

/*
 * We use int32_t to assure compatibility with the JPL binary ephemeris
 * published by JPL in the 'Linux' directory.
 */

struct rec1{
         char ttl[3][84];
         char cnam[OLDMAX][6];
         double ss[3];
         int32_t ncon;
         double au;
         double emrat;
         int32_t ipt[12][3];
         int32_t numde;
         int32_t lpt[3];
         char cnam2[(NMAX-OLDMAX)][6];
       } __attribute__((packed));

 struct rec2{
         double cval[NMAX];
       } __attribute__((packed));

