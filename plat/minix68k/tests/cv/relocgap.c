/* F2: pointers separated by large initialised arrays, so the relocation
 * table needs many skip bytes (one per 254 bytes of gap). Before the fix
 * cv overflowed its table buffer and aborted. */
char a1[30000] = { 1 };
char* p1 = a1;
char a2[30000] = { 2 };
char* p2 = a2;
char* p3 = a1 + 29999;

int main(void)
{
	return *p1 + *p2 + *p3 != 3;
}
