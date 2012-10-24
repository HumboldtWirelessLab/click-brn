/*
 * string_analysis.cc
 *
 *  Created on: 26.08.2011
 *      Author: aureliano
 */



int
count_strinstr(const char *str, const char *substr)
{
	const char *p;
	int count = 0;
	size_t lil_len = strlen(substr);

	/* you decide what to do here */
	if (lil_len == 0) return -1;

	p = strstr(str, substr);
	while (p) {
		count++;
		p = strstr(p + lil_len, substr);
	}
	return count;
}
