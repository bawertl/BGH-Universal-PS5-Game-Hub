// Host-only regression: metadata denied, actual file reading allowed.
#include <cerrno>
#include <sys/stat.h>
extern "C" int __wrap_lstat(const char *, struct stat *) { errno=EPERM; return -1; }
