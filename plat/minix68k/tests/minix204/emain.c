/* The ",e," tests define _m_a_i_n and expect ACK's own start-off (boot.s)
 * to call it; with the Minix crtso, main() does. */
extern void _m_a_i_n(void);

int main(void)
{
	_m_a_i_n();
	return 1;
}
