/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Id$ */

/*
	COMPARE	DOUBLES (CMF 8)
*/

#include	"FP_types.h"
#include	"get_put.h"

int
cmf8(d1,d2)
DOUBLE	d1,d2;
{
#define	SIGN(x)	(((x) < 0) ? -1 : 1)
		/*
		 * return ((d1 < d2) ? 1 : (d1 > d2) ? -1 : 0))
		 */
	long	l1,l2;
	unsigned long	n1,n2;	/* the low halves */
	int	sign1,sign2;
	int	rv;

#if FL_MSL_AT_LOW_ADDRESS
	l1 = get4((char *)&d1);
	l2 = get4((char *)&d2);
#else
	l1 = get4(((char *)&d1+4));
	l2 = get4(((char *)&d2+4));
#endif
	/* A NaN is unordered: not equal to anything, itself included.
	 * EM's cmf has no "unordered" result, so < or > will hold. */
#if FL_MSL_AT_LOW_ADDRESS
	n1 = get4(((char *)&d1 + 4));
	n2 = get4(((char *)&d2 + 4));
#else
	n1 = get4((char *)&d1);
	n2 = get4((char *)&d2);
#endif
	if (((l1 & 0x7fffffffL) | (n1 != 0)) > 0x7ff00000L ||
	    ((l2 & 0x7fffffffL) | (n2 != 0)) > 0x7ff00000L)
		return 1;
	sign1 = SIGN(l1);
	sign2 = SIGN(l2);
	if (sign1 != sign2) {
		l1 &= 0x7fffffff;
		l2 &= 0x7fffffff;
		/* the low halves count too (tiny subnormals); only
		 * +0 and -0 are equal */
		if (l1 != 0 || l2 != 0 || n1 != 0 || n2 != 0) {
			return ((sign1 > 0) ? -1 : 1);
		}
	}
	if (l1 != l2)	{	/* we can decide here */
		rv = l1 < l2 ? 1 : -1;
	}
	else	{ 		/* decide in 2nd half */
		if (n1 == n2)
			return(0);
		if (n1 < n2) rv = 1;
		else rv = -1;
	}
	return sign1 * rv;
}
