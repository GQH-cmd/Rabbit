#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

extern char _end;
extern char __StackLimit;

static char *heap_end;

/* newlib-nano calls these hooks from __libc_init_array(). */
void _init(void) {}
void _fini(void) {}

void *_sbrk(ptrdiff_t increment)
{
    char *previous;

    if (heap_end == 0) {
        heap_end = &_end;
    }

    previous = heap_end;
    if (increment > 0 && (heap_end + increment) > &__StackLimit) {
        errno = ENOMEM;
        return (void *)-1;
    }

    heap_end += increment;
    return previous;
}

/* Minimal newlib hooks used only if a libc routine touches a file descriptor. */
int _close(int file) { (void)file; return -1; }
int _fstat(int file, struct stat *st)
{
    (void)file;
    st->st_mode = S_IFCHR;
    return 0;
}
int _isatty(int file) { (void)file; return 1; }
off_t _lseek(int file, off_t offset, int whence)
{
    (void)file; (void)offset; (void)whence;
    return 0;
}
ssize_t _read(int file, void *buffer, size_t length)
{
    (void)file; (void)buffer; (void)length;
    return 0;
}
int _getpid(void) { return 1; }
int _kill(int pid, int signal_number)
{
    (void)pid; (void)signal_number;
    errno = EINVAL;
    return -1;
}
