#include <stdio.h>

#define DEVICE "/dev/kuredev"

int main(void)
{
	FILE *fp;
	char ch[100];

	fp = fopen(DEVICE, "w");
	if (fp == NULL) {
		perror(DEVICE);
		return -1;	
	}

	fgets(ch, sizeof(ch), stdin);
	fprintf(fp, "%s", ch);

	fclose(fp);

	return 0;
}
