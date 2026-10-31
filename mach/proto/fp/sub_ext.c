/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Id$ */

/*
	SUBTRACT 2 EXTENDED FORMAT NUMBERS
*/

#include "FP_types.h"

void
sub_ext(e1,e2)
EXTEND	*e1,*e2;
{
	/* e1 - e2 == e1 + (-e2): add_ext() knows the special cases
	 * (infinity, NaN, signed zeros). e2 is the caller's copy. */
	e2->sign = e2->sign ? 0 : 0x8000;	/* as extend() sets it */
	add_ext(e1, e2);
}
