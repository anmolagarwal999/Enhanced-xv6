struct stat;
struct rtcdate;

/*
docs
The system calls that user programs in xv6 can use are defined in
the file user.h, which is the header file for the
userspace C library of xv6 (xv6 doesn’t have the standard glibc).


user.h contains the system call definitions in xv6. You will need to add code here for your
new system calls
*/

// system calls
int fork(void);
int exit(void) __attribute__((noreturn));
int wait(void);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);
int getyear(void);

int waitx(int*, int*);
int ps(void);
int set_priority(int,int); // this is what is called from the user-program

//---------------------------------------------------------------------------------------------

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void printf(int, const char*, ...);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);
