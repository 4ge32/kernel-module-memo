#define _XOPEN_SOURCE 700
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> /* uintmax_t */
#include <string.h>
#include <sys/mman.h>
#include <unistd.h> /* sysconf */

//#include "common.h" /* virt_to_phys_user */
typedef struct {
	uint64_t pfn : 54;
	unsigned int soft_dirty : 1;
	unsigned int file_page : 1;
	unsigned int swapped : 1;
	unsigned int present : 1;
} PagemapEntry;

int pagemap_get_entry(PagemapEntry *entry, int pagemap_fd, uintptr_t vaddr)
{
	size_t nread;
	ssize_t ret;
	uint64_t data;

	nread = 0;
	while (nread < sizeof(data)) {
		ret = pread(pagemap_fd, &data, sizeof(data),
				(vaddr / sysconf(_SC_PAGE_SIZE)) * sizeof(data) + nread);
		nread += ret;
		if (ret <= 0) {
			return 1;
		}
	}
	entry->pfn = data & (((uint64_t)1 << 54) - 1);
	entry->soft_dirty = (data >> 54) & 1;
	entry->file_page = (data >> 61) & 1;
	entry->swapped = (data >> 62) & 1;
	entry->present = (data >> 63) & 1;
	return 0;
}

int virt_to_phys_user(uintptr_t *paddr, pid_t pid, uintptr_t vaddr)
{
	char pagemap_file[BUFSIZ];
	int pagemap_fd;

	snprintf(pagemap_file, sizeof(pagemap_file), "/proc/%ju/pagemap", (uintmax_t)pid);
	pagemap_fd = open(pagemap_file, O_RDONLY);
	if (pagemap_fd < 0) {
		return 1;
	}
	PagemapEntry entry;
	if (pagemap_get_entry(&entry, pagemap_fd, vaddr)) {
		return 1;
	}
	close(pagemap_fd);
	*paddr = (entry.pfn * sysconf(_SC_PAGE_SIZE)) + (vaddr % sysconf(_SC_PAGE_SIZE));
	return 0;
}

enum { BUFFER_SIZE = 4 };

int main(int argc, char **argv)
{
	int fd;
	long page_size;
	char *address1, *address2;
	char buf[BUFFER_SIZE];
	uintptr_t paddr;

	if (argc < 2) {
		printf("Usage: %s <mmap_file>\n", argv[0]);
		return EXIT_FAILURE;
	}
	page_size = sysconf(_SC_PAGE_SIZE);
	printf("open pathname = %s\n", argv[1]);
	fd = open(argv[1], O_RDWR | O_SYNC);
	if (fd < 0) {
		perror("open");
		assert(0);
	}
	printf("fd = %d\n", fd);

	/* mmap twice for double fun. */
	puts("mmap 1");
	address1 = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (address1 == MAP_FAILED) {
		perror("mmap");
		assert(0);
	}
	puts("mmap 2");
	address2 = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (address2 == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}
	assert(address1 != address2);

	/* Read and modify memory. */
	puts("access 1");
	assert(!strcmp(address1, "asdf"));
	/* vm_fault */
	puts("access 2");
	assert(!strcmp(address2, "asdf"));
	/* vm_fault */
	strcpy(address1, "qwer");
	/* Also modified. So both virtual addresses point to the same physical address. */
	assert(!strcmp(address2, "qwer"));

	/* Check that the physical addresses are the same.
	 * They are, but TODO why virt_to_phys on kernel gives a different value? */
	assert(!virt_to_phys_user(&paddr, getpid(), (uintptr_t)address1));
	printf("paddr1 = 0x%jx\n", (uintmax_t)paddr);
	assert(!virt_to_phys_user(&paddr, getpid(), (uintptr_t)address2));
	printf("paddr2 = 0x%jx\n", (uintmax_t)paddr);

	/* Check that modifications made from userland are also visible from the kernel. */
	read(fd, buf, BUFFER_SIZE);
	assert(!memcmp(buf, "qwer", BUFFER_SIZE));

	/* Modify the data from the kernel, and check that the change is visible from userland. */
	write(fd, "zxcv", 4);
	assert(!strcmp(address1, "zxcv"));
	assert(!strcmp(address2, "zxcv"));

	/* Cleanup. */
	puts("munmap 1");
	if (munmap(address1, page_size)) {
		perror("munmap");
		assert(0);
	}
	puts("munmap 2");
	if (munmap(address2, page_size)) {
		perror("munmap");
		assert(0);
	}
	puts("close");
	close(fd);
	return EXIT_SUCCESS;
}
