/*
 * m68kcheck: check that ACK object files contain only instructions a 68010
 * runs in user mode (T6).
 *
 *	m68kcheck [-s] file.o|lib.a ...
 *
 * The text section of each object (also inside archives) is disassembled
 * with Musashi's disassembler as a 68010. Reported are F-line words (68881
 * and other coprocessor instructions: a 68010 traps on them), A-line words,
 * opcodes a 68010 does not have, and privileged instructions (move to/from
 * SR, andi/ori/eori to SR, move USP, movec, moves, reset, stop, rte); -s
 * allows the privileged ones, for kernel and boot code. Each finding is
 * printed as "file[:member]: text+offset: opcode  disassembly  (reason)".
 *
 * Decoding is linear, so it can get out of step on data or padding in the
 * text section: zero words after a jump or return are skipped as padding,
 * and decoding starts again at every text symbol. An F-line instruction is
 * decoded as a 68040 would, to get its length.
 *
 * Exit status: 0 if nothing was found, 1 if something was, 2 on errors.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "out.h"
#include "arch.h"
#include "ranlib.h"
#include "object.h"
#include "m68k.h"

static const char* filename;
static const char* member;
static int supervisor;
static int found;

static unsigned char* text;
static unsigned long textlen;

/* Musashi's disassembler reads the code through these (m68kconf.h maps
 * m68k_read_memory_8/16/32 and m68k_read_disassembler_16/32 to them, as for
 * the emulator). */

unsigned int cpu_read_byte(unsigned int a)
{
	return a < textlen ? text[a] : 0;
}

unsigned int cpu_read_word(unsigned int a)
{
	return (cpu_read_byte(a) << 8) | cpu_read_byte(a + 1);
}

unsigned int cpu_read_long(unsigned int a)
{
	return (cpu_read_word(a) << 16) | cpu_read_word(a + 2);
}

unsigned int cpu_read_word_dasm(unsigned int a)
{
	return cpu_read_word(a);
}

unsigned int cpu_read_long_dasm(unsigned int a)
{
	return cpu_read_long(a);
}

static int starts_with(const char* s, const char* word)
{
	size_t n = strlen(word);
	return !strncmp(s, word, n) && (s[n] == ' ' || s[n] == '.' || s[n] == '\0');
}

/* Is the disassembled instruction privileged on a 68010? */
static int privileged(const char* s)
{
	static const char* const ops[] = { "reset", "stop", "rte", "movec", "moves", NULL };
	int i;

	for (i = 0; ops[i]; i++)
		if (starts_with(s, ops[i]))
			return 1;
	/* move to/from SR, andi/ori/eori to SR, move to/from USP */
	return strstr(s, "SR") != NULL || strstr(s, "USP") != NULL;
}

static int ends_flow(const char* s)
{
	return starts_with(s, "rts") || starts_with(s, "rtd") || starts_with(s, "rte")
	    || starts_with(s, "rtr") || starts_with(s, "jmp") || starts_with(s, "bra");
}

void rd_fatal(void)
{
	fprintf(stderr, "m68kcheck: %s%s%s: read error\n", filename,
	    member ? ":" : "", member ? member : "");
	exit(2);
}

static int cmp_ulong(const void* a, const void* b)
{
	unsigned long x = *(const unsigned long*)a;
	unsigned long y = *(const unsigned long*)b;

	return x < y ? -1 : x > y;
}

static void* xmalloc(size_t n)
{
	void* p = malloc(n ? n : 1);

	if (!p)
	{
		fprintf(stderr, "m68kcheck: out of memory\n");
		exit(2);
	}
	return p;
}

static void check_object(FILE* fd)
{
	struct outhead head;
	struct outsect sect[S_MAX - S_MIN + 1];
	unsigned long* starts;
	unsigned nstarts = 0;
	unsigned long pc;
	unsigned i;
	int after_flow = 0;
	char buf[256];

	rd_fdopen(fd);
	rd_ohead(&head);
	if (BADMAGIC(head) || head.oh_nsect == 0 || head.oh_nsect > S_MAX - S_MIN + 1)
	{
		fprintf(stderr, "m68kcheck: %s%s%s: not an ACK object file\n", filename,
		    member ? ":" : "", member ? member : "");
		exit(2);
	}
	rd_sect(sect, head.oh_nsect);

	/* Section 0 is .text. */
	textlen = sect[0].os_flen;
	text = xmalloc(textlen);
	rd_outsect(0);
	rd_emit((char*)text, textlen);

	starts = xmalloc((head.oh_nname + 1) * sizeof(*starts));
	if (head.oh_nname)
	{
		struct outname* names = xmalloc(head.oh_nname * sizeof(*names));

		rd_name(names, head.oh_nname);
		for (i = 0; i < head.oh_nname; i++)
			if ((names[i].on_type & S_TYP) == S_MIN && names[i].on_valu < textlen)
				starts[nstarts++] = names[i].on_valu;
		free(names);
	}
	qsort(starts, nstarts, sizeof(*starts), cmp_ulong);

	for (pc = 0; pc < textlen;)
	{
		unsigned int op = cpu_read_word(pc);
		unsigned int len;
		unsigned long next = textlen;
		const char* why = NULL;

		if (op == 0 && after_flow)
		{
			pc += 2;
			continue;
		}

		/* A 68010 decodes an F-line word as one word; a 68040 knows the
		 * FPU, so its decoding gives the real length and a readable text. */
		if ((op & 0xf000) == 0xf000)
			len = m68k_disassemble(buf, pc, M68K_CPU_TYPE_68040);
		else
			len = m68k_disassemble(buf, pc, M68K_CPU_TYPE_68010);
		if (len == 0)
			len = 2;
		for (i = 0; i < nstarts; i++)
			if (starts[i] > pc)
			{
				next = starts[i];
				break;
			}
		if (pc + len > next)
		{
			/* Runs into a symbol: out of step. Start again there. */
			pc = next;
			after_flow = 0;
			continue;
		}

		/* Musashi counts F-line words as valid (they trap), so test first. */
		if ((op & 0xf000) == 0xf000)
			why = "F-line: FPU or 68020+";
		else if ((op & 0xf000) == 0xa000)
			why = "A-line";
		else if (!m68k_is_valid_instruction(op, M68K_CPU_TYPE_68010))
			why = "not a 68010 instruction";
		else if (!supervisor && privileged(buf))
			why = "privileged";
		if (why)
		{
			printf("%s%s%s: text+0x%lx: %04x  %s  (%s)\n", filename,
			    member ? ":" : "", member ? member : "", pc, op, buf, why);
			found++;
		}
		after_flow = ends_flow(buf);
		pc += len;
	}
	free(starts);
	free(text);
}

static void check_file(const char* name)
{
	FILE* fd = fopen(name, "rb");
	unsigned int magic;

	filename = name;
	member = NULL;
	if (!fd)
	{
		perror(name);
		exit(2);
	}
	magic = rd_unsigned2(fd);
	if (magic == ARMAG || magic == AALMAG)
	{
		struct ar_hdr ah;
		static char memname[sizeof(ah.ar_name) + 1];

		while (rd_arhdr(fd, &ah))
		{
			long next = ftell(fd) + ah.ar_size;

			if (next & 1)
				next++;
			strncpy(memname, ah.ar_name, sizeof(ah.ar_name));
			member = memname;
			if (strcmp(memname, SYMDEF))
				check_object(fd);
			fseek(fd, next, SEEK_SET);
		}
	}
	else
	{
		fseek(fd, 0L, SEEK_SET);
		check_object(fd);
	}
	fclose(fd);
}

int main(int argc, char** argv)
{
	int i = 1;

	if (i < argc && !strcmp(argv[i], "-s"))
	{
		supervisor = 1;
		i++;
	}
	if (i >= argc)
	{
		fprintf(stderr, "usage: m68kcheck [-s] file.o|lib.a ...\n");
		return 2;
	}
	for (; i < argc; i++)
		check_file(argv[i]);
	return found ? 1 : 0;
}
