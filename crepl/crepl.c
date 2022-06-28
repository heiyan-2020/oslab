#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#define MAXLINE 4096
#define true 1
#define false 0
#define TEMPLATE_SIZE 32
typedef int (*function)();

const char *func_template = "/tmp/func_XXXXXX.c";
const char *wrapper_rtn = "int ";
const char *wrapper_name = "wrapper_XXXXXX";
const char *wrapper_prefix = "() { return ";
const char *wrapper_sufix = ";}";

#ifdef __x86_64__
const char *version = "-m64";
#else
const char *version = "-m32";
#endif

int is_function(const char *expr);
int handle_function(const char *expr);
void *dl_function(const char *expr);
void wrap_expr(const char *expr);
void alter_template(char *template, int num);

int main(int argc, char *argv[]) {
  static char line[MAXLINE];
  while (1) {
    printf("crepl> ");
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin)) {
      break;
    }
    // printf("Got %zu chars.\n", strlen(line)); // ??
    if (is_function(line)) {
      dl_function(line);
    } else {
      wrap_expr(line);
    }
  }
}

int is_function(const char *expr) {
  char header[4] = {0};
  strncpy(header, expr, 3);

  return strcmp(header, "int") == 0;
}

void alter_template(char *template, int num) {
  int idx = strlen(template) - 1;
  while (num > 0) {
    int digit = num % 10;
    num /= 10;
    template[idx--] = '0' + digit;
  }
}

void wrap_expr(const char *expr) {
  static int cnt = 0;
  cnt++;
  char func[MAXLINE], name[TEMPLATE_SIZE];
  strcpy(func, wrapper_rtn);
  strcpy(name, wrapper_name);
  alter_template(name, cnt);
  strcat(func, name);
  strcat(func, wrapper_prefix);
  strcat(func, expr);
  strcat(func, wrapper_sufix);
  void *handler = dl_function(func);
  if (handler == NULL) {
    printf("Compile Error\n");
    return ;
  }
  function foo = (function) dlsym(handler, name);
  printf("%d\n", foo());
}

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
    int fd = open("/dev/null",O_WRONLY);
    dup2(fd, STDERR_FILENO);
    dup2(fd, STDOUT_FILENO);
    execlp("gcc","gcc", version,"-Wno-implicit-function-declaration" ,"-Werror" , "-fPIC", "-shared", src_name, "-o", shared_name, NULL);
  }
  int status;
  wait(&status); //等待子进程编译完成
  struct stat sbuf;
  if (stat(shared_name, &sbuf) < 0) return NULL;
  void *handler = dlopen(shared_name, RTLD_LAZY | RTLD_GLOBAL);
  if (handler == NULL) {
    printf("%s\n", dlerror());
    exit(EXIT_FAILURE);
  }
  return handler;
}
