#include <stdio.h>

#define CALC_CLOCK(code, n) do {	\
		int i;	\
		clock_t t = clock();	\
		for (i=0;i<n;i++) { code; }	\
		printf("%d\n", clock() - t);	\
} while (0)

int main()
{
	CALC_CLOCK();
	CALC_CLOCK();
}
