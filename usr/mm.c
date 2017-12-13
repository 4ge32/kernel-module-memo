#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>

#define PAGE_SIZE 4096
#define fname "/dev/mmap_example"
#define LOOP 4096

static char *address = NULL;
static int count = 0;

void __mmap(int fd) {
	if (count)
		return;

	address = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	count++;
	if (address == MAP_FAILED)
	{
		perror("mmap operation failed");
		return;
	}
	//printf("M:Initial message: %s\n", address);
	//memcpy(address + 11 , "*user*", 6);
	//printf("Changed message: %s\n", address + 11);
}

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

	clock_t start = clock();
	for (int i = 0; i < LOOP; i++)
		__mmap(configfd);
	clock_t end = clock();

	clock_t ssart = clock();
	for (int i = 0; i < LOOP; i++)
		__read(configfd);
	clock_t eed = clock();

	printf("mmap:%lu, read%lu\n", end - start, eed - ssart);

	close(configfd);
	return 0;
}
