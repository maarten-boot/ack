/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Id$ */

/*
	ROUTINE TO MULTIPLY TWO EXTENDED FORMAT NUMBERS
*/

# include "FP_bias.h"
# include "FP_trap.h"
# include "FP_types.h"
# include "FP_shift.h"

void
mul_ext(e1,e2)
EXTEND	*e1,*e2;
{
	int	i,j;		/* loop control	*/
	unsigned short	mp[4];		/* multiplier */
	unsigned short	mc[4];		/* multipcand */
	unsigned short	result[8];	/* result */
	unsigned short *pres;

	/* first save the sign (XOR)			*/
	e1->sign ^= e2->sign;

	if (e1->exp == EXT_INFNAN || e2->exp == EXT_INFNAN) {
		if (EXT_ISNAN(e1))
			return;
		if (EXT_ISNAN(e2)) {
			*e1 = *e2;
			return;
		}
		/* infinity * 0 is NaN, else infinity */
		e1->m1 = EXT_ISZERO(e1) || EXT_ISZERO(e2) ? EXT_QNAN : NORMBIT;
		e1->m2 = 0;
		e1->exp = EXT_INFNAN;
		return;
	}
	if (EXT_ISZERO(e1) || EXT_ISZERO(e2)) {
		e1->m1 = e1->m2 = 0L;	/* zero, with the sign */
		e1->exp = 0;
		return;
	}

	/* compute new exponent */
	e1->exp += e2->exp + 1;
	/* 128 bit multiply of mantissas			*/

		/* assign unknown long formats		*/
		/* to known unsigned word formats	*/
	mp[0] = e1->m1 >> 16;
	mp[1] = (unsigned short) e1->m1;
	mp[2] = e1->m2 >> 16;
	mp[3] = (unsigned short) e1->m2;
	mc[0] = e2->m1 >> 16;
	mc[1] = (unsigned short) e2->m1;
	mc[2] = e2->m2 >> 16;
	mc[3] = (unsigned short) e2->m2;
	for (i = 8; i--;) {
		result[i] = 0;
	}
	/*
	 *	fill registers with their components
	 */
	for(i=4, pres = &result[4];i--;pres--) if (mp[i]) {
		unsigned short k = 0;
		unsigned long mpi = mp[i];
		for(j=4;j--;) {
			unsigned long tmp = (unsigned long)pres[j] + k;
			if (mc[j]) tmp += mpi * mc[j];
			pres[j] = tmp;
			k = tmp >> 16;
		}
		pres[-1] = k;
	}
        if (! (result[0] & 0x8000)) {
                e1->exp--;
                for (i = 0; i <= 3; i++) {
                        result[i] <<= 1;
                        if (result[i+1]&0x8000) result[i] |= 1;
                }
                result[4] <<= 1;
        }

	/*
	 *	combine the registers to a total
	 */
	e1->m1 = ((unsigned long)(result[0]) << 16) + result[1];
	e1->m2 = ((unsigned long)(result[2]) << 16) + result[3];
	/* the low half only as a sticky bit: rounding is done once, by
	 * compact() */
	if (result[4] | result[5] | result[6] | result[7])
		e1->m2 |= 1;

					/* check for overflow	*/
	if (e1->exp >= EXT_MAX)	{
		trap(EFOVFL);
			/* if caught 			*/
			/* return signed infinity	*/
		e1->exp = EXT_INFNAN;
		e1->m1 = NORMBIT;
		e1->m2 = 0L;
		return;
	}
				/* check for underflow	*/
	if (e1->exp < EXT_MIN)	{
		trap(EFUNFL);
		e1->exp = EXT_MIN;
		e1->m1 = e1->m2 = 0L;
		return;
	}
}
