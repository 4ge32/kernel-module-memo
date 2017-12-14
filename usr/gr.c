#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>

#define fname "/dev/gread"
#define LOOP 1

void __read(int fd) {
	char buf[4096];
	ssize_t len;

	len = read(fd, buf, sizeof(buf));
	if (len == -1) {
		perror("read operation failed");
		return;
	}
	//printf("R:Initial message: %s\n", buf);
}

int main ( int argc, char **argv )
{
	int configfd;

	printf("%s\n", fname);

	configfd = open(fname, O_RDWR);
	if(configfd < 0)
	{
		perror("Open call failed");
		return -1;
	}

	clock_t ssart = clock();
	for (int i = 0; i < LOOP; i++)
		__read(configfd);
	clock_t eed = clock();

	printf("read%lu\n", eed - ssart);

	close(configfd);
	return 0;
}
