/* $Id$ */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 *
 */

/*
 * Convert ACK a.out file to ST-Minix object format.
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include "out.h"
#include "warnings.h"
#include "object.h"

#if !defined O_BINARY
#define O_BINARY 0
#endif

struct outhead outhead;
struct outsect outsect[S_MAX];

#define TEXTSG 0
#define ROMSG  1
#define DATASG 2
#define BSSSG  3
#define LSECT  BSSSG + 1
#define NSECT  LSECT + 1

#define MIN_STACK 16384L /* default stack at least 16K (8k words) */

static char* output_file;
static int outputfile_created;

static char* program;

/* Output file definitions and such */

static int output;

static int unresolved;
static long textsize;
static long datasize;
static long bsssize;

static char* chmemstr;

static long chmem(char* str, long old);
static void emit_relo(void);
static void emits(struct outsect* section);

static void minixhead(void)
{
	long mh[8];
	long stack;
	int i;

	mh[0] = 0x04100301L;
	mh[1] = 0x00000020L;
	mh[2] = textsize;
	mh[3] = datasize;
	mh[4] = bsssize;
	mh[5] = 0;
	stack = 0x00010000L - (mh[3] + mh[4]);
	if ((mh[0] & 0x00200000L) == 0) /* not SEPARATE */
		stack -= mh[2];
	while (stack < 0)
		stack += 0x00010000L;
	/* The rest of the last 64K is the stack; just below a 64K boundary that
	 * is only a few bytes, so give at least MIN_STACK (a chmem argument
	 * still overrides). */
	if (stack < MIN_STACK)
		stack = MIN_STACK;
	if (chmemstr)
		stack = chmem(chmemstr, stack);
	mh[6] = stack + (mh[3] + mh[4]);
	if ((mh[0] & 0x00200000L) == 0) /* not SEPARATE */
		mh[6] += mh[2];
	mh[7] = 0;
	/* 32 big endian bytes, whatever the size of a long on the host */
	unsigned char hb[32];
	for (i = 0; i < 8; i++)
	{
		hb[4 * i] = mh[i] >> 24;
		hb[4 * i + 1] = mh[i] >> 16;
		hb[4 * i + 2] = mh[i] >> 8;
		hb[4 * i + 3] = mh[i];
	}

	if (write(output, (char*)hb, sizeof(hb)) != sizeof(hb))
		fatal("write error\n");
}

static long align(long a, long b)
{
	if (b == 0)
		return a;
	a += b - 1;
	return a - a % b;
}

static int follows(struct outsect* pa, struct outsect* pb)
{
	/* return 1 if pa follows pb */

	return pa->os_base == align(pb->os_base + pb->os_size, pa->os_lign);
}

int main(int argc, char* argv[])
{
	program = argv[0];
	if (argc > 1)
	{
		switch (argv[1][0])
		{
			case '-':
			case '+':
			case '=':
				chmemstr = argv[1];
				argc--;
				argv++;
		}
	}
	switch (argc)
	{
		case 3:
		 	output = open(argv[2], O_CREAT|O_RDWR|O_TRUNC|O_BINARY, 0755);
			if (output < 0)
				fatal("Can't write %s: %s\n", argv[2], strerror(errno));
			output_file = argv[2];
			outputfile_created = 1;
			if (!rd_open(argv[1]))
				fatal("Can't read %s: %s\n", argv[1], strerror(errno));
			break;
		default:
			fatal("Usage: %s [+-= amount] <ACK object> <ST-MINIX object>.\n", argv[0]);
	}
	rd_ohead(&outhead);
	if (BADMAGIC(outhead))
		fatal("Not an ack object file.\n");
	if (outhead.oh_flags & HF_LINK)
	{
		unresolved++;
		fatal("Contains unresolved references.\n");
	}
	if (outhead.oh_nsect != LSECT && outhead.oh_nsect != NSECT)
		fatal("Input file must have %d sections, not %ld\n", NSECT, outhead.oh_nsect);
	rd_sect(outsect, outhead.oh_nsect);
	/* A few checks */
	if (outsect[BSSSG].os_flen != 0)
		fatal("bss space contains initialized data\n");
	if (!follows(&outsect[BSSSG], &outsect[DATASG]))
		fatal("bss segment must follow data segment\n");
	textsize = (outsect[DATASG].os_base - outsect[TEXTSG].os_base);
	if (!follows(&outsect[ROMSG], &outsect[TEXTSG]))
		fatal("rom segment must follow text\n");
	if (!follows(&outsect[DATASG], &outsect[ROMSG]))
		fatal("data segment must follow rom\n");
	outsect[TEXTSG].os_size = outsect[ROMSG].os_base - outsect[TEXTSG].os_base;
	outsect[ROMSG].os_size = outsect[DATASG].os_base - outsect[ROMSG].os_base;
	outsect[DATASG].os_size = outsect[BSSSG].os_base - outsect[DATASG].os_base;
	datasize = outsect[DATASG].os_size;
	bsssize = outsect[BSSSG].os_size;
	if (outhead.oh_nsect == NSECT)
	{
		if (!follows(&outsect[LSECT], &outsect[BSSSG]))
			fatal("end segment must follow bss\n");
		if (outsect[LSECT].os_size != 0)
			fatal("end segment must be empty\n");
	}

	minixhead();
	emits(&outsect[TEXTSG]);
	emits(&outsect[ROMSG]);
	emits(&outsect[DATASG]);
	emit_relo();
	if (outputfile_created)
		chmod(argv[2], 0755);
	return 0;
}

/*
 * Transfer the emitted byted from one file to another.
 */
static void emits(struct outsect* section)
{
	char* p;
	long sz = section->os_flen;

	rd_outsect(section - outsect);
	while (sz)
	{
		unsigned int i = (sz >= 0x4000 ? 0x4000 : sz);
		if (!(p = malloc(0x4000)))
		{
			fatal("No memory.\n");
		}
		rd_emit(p, (long)i);
		if (write(output, p, (int)i) < i)
		{
			fatal("write error.\n");
		}
		free(p);
		sz -= i;
	}

	sz = section->os_size - section->os_flen;
	if (sz)
	{
		if (!(p = calloc(0x4000, 1)))
		{
			fatal("No memory.\n");
		}
		while (sz)
		{
			unsigned int i = (sz >= 0x4000 ? 0x4000 : sz);
			if (write(output, p, (int)i) < i)
			{
				fatal("write error.\n");
			}
			sz -= i;
		}
		free(p);
	}
}

static int compare(const void* pa, const void* pb)
{
	const struct outrelo* a = pa;
	const struct outrelo* b = pb;

	if (a->or_sect < b->or_sect)
		return -1;
	if (a->or_sect > b->or_sect)
		return 1;
	if (a->or_addr < b->or_addr)
		return -1;
	if (a->or_addr > b->or_addr)
		return 1;
	return 0;
}

/*
 * The relocation table is written through a small buffer, one byte at a
 * time: its length depends on the distances between relocations (one extra
 * byte per 254 bytes of gap), so it cannot be sized in advance.
 */
static unsigned char relobuf[512];
static int relofill;

static void relo_flush(void)
{
	if (relofill && write(output, relobuf, relofill) != relofill)
		fatal("write error.\n");
	relofill = 0;
}

static void relo_put1(int b)
{
	if (relofill == sizeof(relobuf))
		relo_flush();
	relobuf[relofill++] = b;
}

static void relo_put4(uint32_t l)
{
	relo_put1(l >> 24);
	relo_put1(l >> 16);
	relo_put1(l >> 8);
	relo_put1(l);
}

static void emit_relo(void)
{
	struct outrelo* ACKrelo;
	struct outrelo* ap;
	unsigned int cnt = outhead.oh_nrelo;
	long last, curr, base;
	int sect;
	int first = 1;

	ACKrelo = ap = (struct outrelo*)calloc(cnt ? cnt : 1, sizeof(struct outrelo));
	if (!ap)
	{
		fatal("No memory.\n");
	}
	rd_relo(ap, cnt);
	qsort((char*)ap, (int)cnt, sizeof(struct outrelo), compare);
	
	/*
	 * read relocation, modify to GEMDOS format, and write.
	 * Only longs can be relocated.
	 *
	 * The GEMDOS format starts with a long L: the offset to the
	 * beginning of text for the first long to be relocated.
	 * If L==0 then no relocations have to be made.
	 *
	 * The long is followed by zero or more bytes. Each byte B is
	 * processed separately, in one of the following ways:
	 *
	 * B==0:
	 *      end of relocation
	 * B==1:
	 *      no relocation, but add 254 to the current offset
	 * B==0bWWWWWWW0:
	 *      B is added to the current offset and the long addressed
	 *      is relocated. Note that 00000010 means 1 word distance.
	 * B==0bXXXXXXX1:
	 *      illegal
	 */

	last = 0;
	curr = 0;
	for (sect = S_MIN; sect <= S_MIN + 2; sect++)
	{
		base = outsect[sect - S_MIN].os_base;
		for (; cnt > 0 && ap->or_sect == sect; ap++, cnt--)
		{
			if (ap->or_type & RELPC || ap->or_nami == outhead.oh_nname)
				continue;

			assert(ap->or_type & RELO4);
			curr = base + ap->or_addr;
			if (curr & 1)
				fatal("relocation at odd address 0x%lx\n", curr);
			if (first)
			{
				/* L==0 would mean "no relocations" */
				if (curr == 0)
					fatal("relocation at address 0\n");
				relo_put4(curr);
				first = 0;
			}
			else
			{
				/* B==0 would end the table */
				if (curr <= last)
					fatal("relocations out of order at 0x%lx\n", curr);
				while (curr - last > 254)
				{
					relo_put1(1);
					last += 254;
				}
				relo_put1(curr - last);
			}
			last = curr;
		}
		assert(cnt == 0 || ap->or_sect > sect);
	}
	assert(cnt == 0);
	if (first)
		relo_put4(0); /* no relocations */
	else
		relo_put1(0); /* end of table */
	relo_flush();
	free((char*)ACKrelo);
}

static long chmem(char* str, long old)
{
	long num, new;

	num = atol(str + 1);
	if (num == 0)
		fatal("bad chmem amount %s\n", str + 1);
	switch (str[0])
	{
		case '-':
			new = old - num;
			break;
		case '+':
			new = old + num;
			break;
		case '=':
			new = num;
			break;
	}
	return (new);
}

void rd_fatal(void)
{
	fatal("read error.\n");
}
