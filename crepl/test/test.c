#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/wait.h>
#include <sys/types.h>
#define MAXLINE 4096
#define true 1
#define false 0
#define TEMPLATE_SIZE 32
typedef int (*func)();

const char *func_template = "./func_XXXXXX.c";
const char *wrapper_head = "int wrapper_XXXXXX";
const char *wrapper_prefix = "() { return ";
const char *wrapper_sufix = ";}";

int is_function(const char *expr);
int handle_function(const char *expr);
void *dl_function(const char *expr);
void * wrap_expr(const char *expr);
void alter_template(char *template, int num);

void *dl_function(const char *expr) {
  char src_name[TEMPLATE_SIZE], shared_name[TEMPLATE_SIZE];
  strcpy(src_name, func_template);
  int fd = mkstemps(src_name, 2);
  assert(fd > 2);

  write(fd, expr, strlen(expr));
  close(fd);

  strcpy(shared_name, src_name);
  size_t len = strlen(shared_name);
  shared_name[len - 1] = 's', shared_name[len] = 'o', shared_name[len + 1] = '\0';
  if (fork() == 0) {
    execlp("gcc","gcc", "-fPIC", "-shared", src_name, "-o", shared_name, NULL);
  }
  int status;
  wait(&status);
  printf("shared object: %s\n", shared_name);
  void * ptr = dlopen(shared_name, RTLD_LAZY);
  func foo = (func)dlsym(ptr, "foo");
  assert(ptr != NULL);
  printf("result = %d\n", foo());
  return NULL;
}

int main() {
    const char *foo = "int foo() {return 1 + 1;}";
    dl_function(foo);
}