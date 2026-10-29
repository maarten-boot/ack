/*
 * A minimal Minix-ST (minix68k) process emulator for running the ACK test
 * suite: Musashi as a 68010 in user mode, plus just enough of MM and FS to
 * run small C programs.
 *
 *	emu [-t] <program> [args...]
 *
 * The program is the output of plat/minix68k/cv: a 32 byte Minix header,
 * text (with rom), data, and a GEMDOS style relocation table. It is loaded
 * at LOAD_BASE, not 0, so the relocation table is exercised.
 *
 * Every exception vector points to a stub of its own. trap #0 (sendrec) is
 * handled as a Minix system call; any other exception (address error,
 * illegal or 68020-only instruction, line A/F such as 68881 code, privilege
 * violation, ...) is reported and ends the run with a failure, as do user
 * accesses outside the process (NULL pointers) and writes into its text.
 *
 * -t traces every user instruction to stderr.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "sim.h"
#include "m68k.h"

#define RAM_SIZE    0x400000 /* 4 MB */
#define SUPER_STACK 0x8000 /* top of the supervisor stack */
#define STUB_BASE   0x400 /* vector n is handled at STUB_BASE + 4*n */
#define NVECTORS    256
#define LOAD_BASE   0x10000 /* where the program is loaded */

#define TRAP0_VECTOR 32
#define SAFETY_BYTES 1536 /* MM's gap between break and stack */

/* System call numbers (plat/minix/include/minix/callnr.h). */
#define EXIT   1
#define READ   3
#define WRITE  4
#define OPEN   5
#define CLOSE  6
#define CREAT  8
#define UNLINK 10
#define TIME   13
#define BRK    17
#define LSEEK  19
#define GETPID 20
#define ACCESS 33
#define SIGNAL 48
#define IOCTL  54

/* sendrec() function codes in d0 (plat/minix68k/libsys/_stsndrec.s). */
#define BOTH 3

/*
 * Message layout with 16 bit int, 32 bit pointers and long, 2 byte
 * alignment (plat/minix/include/minix/type.h).
 */
#define M_SOURCE 0
#define M_TYPE   2
#define M1_I1    4
#define M1_I2    6
#define M1_I3    8
#define M1_P1    10
#define M2_I1    4
#define M2_I2    6
#define M2_L1    10
#define M2_P1    18
#define M3_I2    6
#define M3_P1    8
#define MESS_SIZE 26

/* Minix open() flags (plat/minix/include/ack/fcntl.h). */
#define MX_O_ACCMODE  03
#define MX_O_CREAT    00100
#define MX_O_EXCL     00200
#define MX_O_NOCTTY   00400
#define MX_O_TRUNC    01000
#define MX_O_APPEND   02000
#define MX_O_NONBLOCK 04000

/* Minix errno values (plat/minix/include/sys/errno.h). */
#define MX_EIO     5
#define MX_ENOMEM  12
#define MX_EINVAL  22
#define MX_ENOTTY  25

static unsigned char ram[RAM_SIZE];

static uint32_t text_end; /* end of text+rom, start of data */
static uint32_t data_start; /* start of data */
static uint32_t bss_end; /* initial break */
static uint32_t proc_end; /* top of the process (top of stack) */
static uint32_t brk_pos;

static uint32_t last_user_pc;
static int tracing;

static void cleanup_and_exit(int status)
{
	fflush(stdout);
	fflush(stderr);
	exit(status);
}

static const char* vector_name(int vec)
{
	switch (vec)
	{
		case 2: return "bus error";
		case 3: return "address error (odd word or long access)";
		case 4: return "illegal instruction (68020-only code?)";
		case 5: return "zero divide";
		case 6: return "CHK";
		case 7: return "TRAPV";
		case 8: return "privilege violation";
		case 9: return "trace";
		case 10: return "line A instruction";
		case 11: return "line F instruction (68881 code?)";
		case 14: return "format error";
		case 15: return "uninitialised interrupt";
		case 24: return "spurious interrupt";
	}
	if (vec >= 25 && vec <= 31)
		return "interrupt";
	if (vec >= 33 && vec <= 47)
		return "trap #1-#15";
	return "unexpected exception";
}

/* Report a problem in the user program and stop with a failure. */
static void fail(const char* fmt, ...)
{
	static int failing;
	char buf[100];
	va_list ap;

	if (failing)
		cleanup_and_exit(EXIT_FAILURE);
	failing = 1;

	fflush(stdout);
	fprintf(stderr, "minix68k emu: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	if (last_user_pc)
	{
		m68k_disassemble(buf, last_user_pc, M68K_CPU_TYPE_68010);
		fprintf(stderr, "  last user instruction at 0x%06x (text+0x%04x): %s\n",
		    last_user_pc, last_user_pc - LOAD_BASE, buf);
	}
	printf("@@FAIL in minix68k emulator\n");
	cleanup_and_exit(EXIT_FAILURE);
}

static int user_mode(void)
{
	return !(m68k_get_reg(NULL, M68K_REG_SR) & 0x2000);
}

/*
 * Every memory access by the CPU goes through here. In user mode the
 * program may only touch its own memory, and may not write its text.
 */
static uint32_t check_access(uint32_t address, int size, int writing)
{
	address &= 0xffffff; /* 24 address lines on a 68010 */
	if (address + size > RAM_SIZE)
		fail("access to 0x%06x outside RAM", address);
	if (user_mode())
	{
		if (address < LOAD_BASE)
			fail("%s of low memory at 0x%06x (NULL pointer?)",
			    writing ? "write" : "read", address);
		if (address + size > proc_end)
			fail("%s at 0x%06x beyond the process (top 0x%06x)",
			    writing ? "write" : "read", address, proc_end);
		if (writing && address < text_end)
			fail("write into text or rom at 0x%06x", address);
	}
	return address;
}

static uint32_t rd8(uint32_t a)
{
	return ram[a];
}

static uint32_t rd16(uint32_t a)
{
	return (ram[a] << 8) | ram[a + 1];
}

static uint32_t rd32(uint32_t a)
{
	return ((uint32_t)ram[a] << 24) | (ram[a + 1] << 16) | (ram[a + 2] << 8) | ram[a + 3];
}

static void wr8(uint32_t a, uint32_t v)
{
	ram[a] = v;
}

static void wr16(uint32_t a, uint32_t v)
{
	ram[a] = v >> 8;
	ram[a + 1] = v;
}

static void wr32(uint32_t a, uint32_t v)
{
	ram[a] = v >> 24;
	ram[a + 1] = v >> 16;
	ram[a + 2] = v >> 8;
	ram[a + 3] = v;
}

unsigned int cpu_read_byte(unsigned int address)
{
	return rd8(check_access(address, 1, 0));
}

unsigned int cpu_read_word(unsigned int address)
{
	return rd16(check_access(address, 2, 0));
}

unsigned int cpu_read_long(unsigned int address)
{
	return rd32(check_access(address, 4, 0));
}

void cpu_write_byte(unsigned int address, unsigned int value)
{
	wr8(check_access(address, 1, 1), value);
}

void cpu_write_word(unsigned int address, unsigned int value)
{
	wr16(check_access(address, 2, 1), value);
}

void cpu_write_long(unsigned int address, unsigned int value)
{
	wr32(check_access(address, 4, 1), value);
}

/* The disassembler reads without side effects or checks. */
unsigned int cpu_read_word_dasm(unsigned int address)
{
	address &= 0xffffff;
	return address + 2 <= RAM_SIZE ? rd16(address) : 0;
}

unsigned int cpu_read_long_dasm(unsigned int address)
{
	address &= 0xffffff;
	return address + 4 <= RAM_SIZE ? rd32(address) : 0;
}

/* A range of process memory passed to a system call. */
static unsigned char* user_buffer(uint32_t address, uint32_t size, int writing)
{
	if (address < LOAD_BASE || address > proc_end || size > proc_end - address)
		fail("system call buffer 0x%06x+%u outside the process", address, size);
	if (writing && address < text_end)
		fail("system call writes into text at 0x%06x", address);
	return ram + address;
}

static const char* user_string(uint32_t address)
{
	uint32_t a;

	if (address < LOAD_BASE || address >= proc_end)
		fail("system call string at 0x%06x outside the process", address);
	for (a = address; a < proc_end; a++)
		if (ram[a] == 0)
			return (const char*)ram + address;
	fail("system call string at 0x%06x not terminated", address);
	return NULL;
}

/* Translate a host errno to a (negative) Minix one. */
static int minix_error(int e)
{
	/* Minix order: index i is Minix errno i. */
	static const int host[] = {
		0, EPERM, ENOENT, ESRCH, EINTR, EIO, ENXIO, E2BIG, ENOEXEC,
		EBADF, ECHILD, EAGAIN, ENOMEM, EACCES, EFAULT, ENOTBLK, EBUSY,
		EEXIST, EXDEV, ENODEV, ENOTDIR, EISDIR, EINVAL, ENFILE, EMFILE,
		ENOTTY, ETXTBSY, EFBIG, ENOSPC, ESPIPE, EROFS, EMLINK, EPIPE,
		EDOM, ERANGE, EDEADLK, ENAMETOOLONG, ENOLCK, ENOSYS, ENOTEMPTY,
	};
	unsigned i;

	for (i = 1; i < sizeof(host) / sizeof(host[0]); i++)
		if (host[i] == e)
			return -(int)i;
	return -MX_EIO;
}

static int host_open_flags(int f)
{
	int h = 0;

	switch (f & MX_O_ACCMODE)
	{
		case 0: h = O_RDONLY; break;
		case 1: h = O_WRONLY; break;
		case 2: h = O_RDWR; break;
		default: return -1;
	}
	if (f & MX_O_CREAT) h |= O_CREAT;
	if (f & MX_O_EXCL) h |= O_EXCL;
	if (f & MX_O_NOCTTY) h |= O_NOCTTY;
	if (f & MX_O_TRUNC) h |= O_TRUNC;
	if (f & MX_O_APPEND) h |= O_APPEND;
	if (f & MX_O_NONBLOCK) h |= O_NONBLOCK;
	return h;
}

/* Result of a host call: the value, or a negative Minix errno. */
static int result(long r)
{
	if (r < 0)
		return minix_error(errno);
	return r > 0x7fff ? 0x7fff : (int)r;
}

static int16_t msg_int(uint32_t m, int off)
{
	return (int16_t)rd16(m + off);
}

/*
 * trap #0: _sendrec(who, &message). MM and FS reply in the same message,
 * with the result (or a negative errno) in m_type; d0 = 0 means the
 * message itself was delivered.
 */
static void minix_syscall(void)
{
	int function = m68k_get_reg(NULL, M68K_REG_D0) & 0xffff;
	int who = m68k_get_reg(NULL, M68K_REG_D1) & 0xffff;
	uint32_t m = m68k_get_reg(NULL, M68K_REG_A0);
	int call, r;

	if (function != BOTH)
		fail("trap #0 with function %d (only sendrec is emulated)", function);
	if (m & 1)
		fail("message at odd address 0x%06x", m);
	user_buffer(m, MESS_SIZE, 1);
	call = msg_int(m, M_TYPE);

	switch (call)
	{
		case EXIT:
			cleanup_and_exit(msg_int(m, M1_I1) & 0xff);

		case READ:
		{
			uint32_t n = (uint16_t)msg_int(m, M1_I2);
			uint32_t p = rd32(m + M1_P1);
			if (n > 0x7fff)
				n = 0x7fff; /* the count comes back in a 16 bit int */
			r = result(read(msg_int(m, M1_I1), user_buffer(p, n, 1), n));
			break;
		}

		case WRITE:
		{
			uint32_t n = (uint16_t)msg_int(m, M1_I2);
			uint32_t p = rd32(m + M1_P1);
			if (n > 0x7fff)
				n = 0x7fff;
			fflush(stdout);
			r = result(write(msg_int(m, M1_I1), user_buffer(p, n, 0), n));
			break;
		}

		case OPEN:
		{
			/* Flags are m1_i2 = m3_i2. With O_CREAT the message is
			 * type 1 (name in m1_p1, mode in m1_i3), otherwise type 3
			 * (name in m3_p1). */
			int flags = msg_int(m, M3_I2);
			int h = host_open_flags(flags);
			const char* name;

			if (h < 0)
			{
				r = -MX_EINVAL;
				break;
			}
			if (flags & MX_O_CREAT)
			{
				name = user_string(rd32(m + M1_P1));
				r = result(open(name, h, (uint16_t)msg_int(m, M1_I3)));
			}
			else
			{
				name = user_string(rd32(m + M3_P1));
				r = result(open(name, h));
			}
			break;
		}

		case CREAT:
			r = result(open(user_string(rd32(m + M3_P1)),
			    O_WRONLY | O_CREAT | O_TRUNC, (uint16_t)msg_int(m, M3_I2)));
			break;

		case CLOSE:
		{
			int fd = msg_int(m, M1_I1);
			/* Keep the emulator's own stdin/stdout/stderr open. */
			r = fd <= 2 ? 0 : result(close(fd));
			break;
		}

		case UNLINK:
			r = result(unlink(user_string(rd32(m + M3_P1))));
			break;

		case ACCESS:
			r = result(access(user_string(rd32(m + M3_P1)), msg_int(m, M3_I2)));
			break;

		case LSEEK:
		{
			off_t pos = lseek(msg_int(m, M2_I1), (int32_t)rd32(m + M2_L1), msg_int(m, M2_I2));
			if (pos < 0)
				r = minix_error(errno);
			else if (pos > 0x7fffffff)
				r = -MX_EINVAL;
			else
			{
				wr32(m + M2_L1, (uint32_t)pos);
				r = 0;
			}
			break;
		}

		case TIME:
			wr32(m + M2_L1, (uint32_t)time(NULL));
			r = 0;
			break;

		case BRK:
		{
			/* As MM: the break may not go below the data segment, nor
			 * come closer than SAFETY_BYTES to the stack. */
			uint32_t addr = rd32(m + M1_P1);
			uint32_t sp = m68k_get_reg(NULL, M68K_REG_USP);
			if (addr < data_start || addr > sp || sp - addr < SAFETY_BYTES)
				r = -MX_ENOMEM;
			else
			{
				brk_pos = addr;
				wr32(m + M2_P1, addr);
				r = 0;
			}
			break;
		}

		case GETPID:
			r = 7; /* host pids do not fit a 16 bit int */
			break;

		case SIGNAL:
			r = 0; /* no signals are ever delivered */
			break;

		case IOCTL:
			r = -MX_ENOTTY;
			break;

		default:
			fail("unsupported system call %d to %s", call,
			    who == 0 ? "MM" : who == 1 ? "FS" : "?");
	}

	wr16(m + M_SOURCE, who);
	wr16(m + M_TYPE, (uint16_t)r);
	m68k_set_reg(M68K_REG_D0, 0);
}

void cpu_instr_callback(int pc)
{
	uint32_t upc = (uint32_t)pc & 0xffffff;

	if (upc >= STUB_BASE && upc < STUB_BASE + 4 * NVECTORS)
	{
		int vec = (upc - STUB_BASE) / 4;

		if (vec == TRAP0_VECTOR)
		{
			minix_syscall();
			return; /* the stub's rte returns to the program */
		}
		if (vec == 2 || vec == 3)
		{
			/* Musashi stacks a 68000 style group 0 frame: the
			 * access address is at ssp+2. */
			uint32_t ssp = m68k_get_reg(NULL, M68K_REG_A7);
			fail("%s, access address 0x%06x", vector_name(vec), rd32(ssp + 2) & 0xffffff);
		}
		fail("exception %d: %s", vec, vector_name(vec));
	}

	if (upc >= LOAD_BASE)
	{
		last_user_pc = upc;
		if (tracing)
		{
			char buf[100];
			m68k_disassemble(buf, upc, M68K_CPU_TYPE_68010);
			fprintf(stderr, "%06x  %-30s d0=%08x d1=%08x a0=%08x sp=%08x\n",
			    upc, buf, m68k_get_reg(NULL, M68K_REG_D0),
			    m68k_get_reg(NULL, M68K_REG_D1), m68k_get_reg(NULL, M68K_REG_A0),
			    m68k_get_reg(NULL, M68K_REG_A7));
		}
	}
}

static void load_error(const char* what, const char* file)
{
	fprintf(stderr, "minix68k emu: %s: %s\n", file, what);
	printf("@@FAIL in minix68k emulator\n");
	cleanup_and_exit(EXIT_FAILURE);
}

/*
 * Load the program and apply its relocation table: a long L, the offset
 * of the first long to relocate (0: none), then bytes: 0 ends the table,
 * 1 adds 254 to the offset, an even B adds B and relocates that long.
 */
static void load_program(const char* file)
{
	FILE* f = fopen(file, "rb");
	unsigned char h[32];
	uint32_t text, data, bss, total, size, pos;
	int c;

	if (!f)
		load_error(strerror(errno), file);
	if (fread(h, 1, sizeof(h), f) != sizeof(h))
		load_error("no Minix header", file);
	if (h[2] != 0x03 || h[3] != 0x01 || h[7] != 0x20)
		load_error("not a Minix-ST executable", file);
	text = (h[8] << 24) | (h[9] << 16) | (h[10] << 8) | h[11];
	data = (h[12] << 24) | (h[13] << 16) | (h[14] << 8) | h[15];
	bss = (h[16] << 24) | (h[17] << 16) | (h[18] << 8) | h[19];
	total = (h[24] << 24) | (h[25] << 16) | (h[26] << 8) | h[27];
	size = text + data;

	if (total < size + bss || total > RAM_SIZE - LOAD_BASE)
		load_error("bad sizes in the header", file);
	if (fread(ram + LOAD_BASE, 1, size, f) != size)
		load_error("short text or data", file);

	text_end = data_start = LOAD_BASE + text;
	bss_end = brk_pos = LOAD_BASE + size + bss;
	proc_end = LOAD_BASE + total;

	if (fread(h, 1, 4, f) != 4)
		load_error("no relocation table", file);
	pos = (h[0] << 24) | (h[1] << 16) | (h[2] << 8) | h[3];
	if (pos != 0)
	{
		for (;;)
		{
			if ((pos & 1) || pos + 4 > size)
				load_error("relocation outside text and data", file);
			wr32(LOAD_BASE + pos, rd32(LOAD_BASE + pos) + LOAD_BASE);

			do
			{
				if ((c = getc(f)) == EOF)
					load_error("relocation table not terminated", file);
				if (c == 1)
					pos += 254;
			} while (c == 1);
			if (c == 0)
				break;
			if (c & 1)
				load_error("odd step in the relocation table", file);
			pos += c;
		}
	}
	fclose(f);
}

/*
 * The stack as plat/minix68k/boot.s expects it: argc (long), the argv
 * pointers, NULL, the envp pointers, NULL, and the strings right after.
 */
static uint32_t build_stack(int argc, char** argv)
{
	uint32_t strings = 0, sp, p, s;
	int i;

	for (i = 0; i < argc; i++)
		strings += strlen(argv[i]) + 1;
	strings = (strings + 3) & ~3;
	sp = proc_end - strings - 4 * (1 + argc + 1 + 1);
	if (sp < bss_end + SAFETY_BYTES)
		load_error("arguments do not fit", argv[0]);

	p = sp;
	wr32(p, argc);
	p += 4;
	s = sp + 4 * (1 + argc + 1 + 1);
	for (i = 0; i < argc; i++)
	{
		wr32(p, s);
		p += 4;
		strcpy((char*)ram + s, argv[i]);
		s += strlen(argv[i]) + 1;
	}
	wr32(p, 0); /* end of argv */
	wr32(p + 4, 0); /* empty environment */
	return sp;
}

int main(int argc, char* argv[])
{
	uint32_t sp;
	int i;

	while (argc > 1 && strcmp(argv[1], "-t") == 0)
	{
		tracing = 1;
		argc--;
		argv++;
	}
	if (argc < 2)
	{
		fprintf(stderr, "usage: emu [-t] <minix68k program> [args...]\n");
		exit(EXIT_FAILURE);
	}

	load_program(argv[1]);
	sp = build_stack(argc - 1, argv + 1);

	/* Reset vectors, and a stub of its own for every other vector. */
	wr32(0, SUPER_STACK);
	wr32(4, LOAD_BASE);
	for (i = 2; i < NVECTORS; i++)
	{
		wr32(4 * i, STUB_BASE + 4 * i);
		wr16(STUB_BASE + 4 * i, 0x4e73); /* rte */
		wr16(STUB_BASE + 4 * i + 2, 0x4e71); /* nop */
	}

	m68k_set_cpu_type(M68K_CPU_TYPE_68010);
	m68k_init();
	m68k_pulse_reset();

	/* Run the program in user mode, on its own stack. Setting SR does not
	 * swap the stack pointers in Musashi: switch first, then set a7 (now
	 * the user sp) and the saved supervisor sp. */
	m68k_set_reg(M68K_REG_SR, 0x0000);
	m68k_set_reg(M68K_REG_SP, sp);
	m68k_set_reg(M68K_REG_ISP, SUPER_STACK);

	for (;;)
		m68k_execute(100000);
}
