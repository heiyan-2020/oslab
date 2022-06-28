#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <regex.h>
#include <string.h>
#include <sys/time.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/wait.h>
#define N 10000
#define TOP 5
#define FRAME 1000

typedef struct entry {
  char *name;
  double time_cost;
} Entry;

Entry syscall_pool[N];
double total_time;
int amount;
uint64_t pass_time;
void quick_sort(Entry *q, int l, int r)
{
  if (l >= r)
  {
    return;
  }
  double pivot = q[(l + r) >> 1].time_cost;
  int i = l - 1, j = r + 1;
  while (i < j)
  {
    while (q[++i].time_cost > pivot)
      ;
    while (q[--j].time_cost < pivot)
      ;

    if (i < j)
    {
      Entry tmp = q[i];
      q[i] = q[j];
      q[j] = tmp;
    }
  }
  quick_sort(q, l, j), quick_sort(q, j + 1, r);
}

int matchRegex(const char *pattern, const char *userString)
{
    int result = 0;
    regex_t regex;
    int regexInit = regcomp(&regex, pattern, REG_EXTENDED);
    if( regexInit )
    {
        assert(0);
    }
    else
    {
        int reti = regexec( &regex, userString, 0, NULL, 0 );
        if( REG_NOERROR != reti )
        {
            return 0;
        }
        else
        {
            result = 1;
        }
    }
    regfree( &regex );
    return result;
}

void output() {
  quick_sort(syscall_pool, 0, amount - 1);
  int limit = amount <= TOP ? amount : TOP;
  if (limit == 0) {
    return;
  }
  // printf("Prifile:\n");
  for (int i = 0; i < limit; i++) {
    for (int j = 0; syscall_pool[i].name[j]; j++) {
      syscall_pool[i].name[j] = tolower(syscall_pool[i].name[j]);
    }
    printf("%s(%.0lf%%)\n", syscall_pool[i].name, (syscall_pool[i].time_cost * 100 / total_time));
  }
  // printf("\033[%dA", limit);
  char arr[80];
  memset(arr, 0, sizeof arr);
  printf("%s", arr);
  fflush(stdout);
}

void insert(char *name, double time_stamp) {
  for (int i = 0; i < amount; i++) {
    if (strcmp(syscall_pool[i].name, name) == 0) {
      syscall_pool[i].time_cost += time_stamp;
      return;
    }
  }
  syscall_pool[amount].name = name;
  syscall_pool[amount].time_cost = time_stamp;
  amount++;
}

void handle(char *buf, int len) {
  //从前往后得到系统调用的名字
  char *name = buf;
  char correct_prefix[] = "^\\w.+\\(";
  char correct_suffix[] = "\\<[0-9]+\\.[0-9]+\\>";  
  if (matchRegex(correct_prefix, buf) && matchRegex(correct_suffix, buf)) {
    // printf("%s\n", buf);
    char *name = buf;
    while (*name != '(')
    {
      name++;
    }
    *name = '\0';
    char *time = buf + len - 1;
    while (*time != '>') {
      time--;
    }
    char *haha = time;
    while (*haha != '<')
    {
      haha--;
    }
    haha++;
    *time = '\0';
    char *endptr;
    double timestamp = strtod(haha, &endptr);
    total_time += timestamp;
    struct timeval te;
    gettimeofday(&te, NULL);
    uint64_t mill = te.tv_sec * 1000 + te.tv_usec / 1000;
    char *dup_buf = (char*) malloc(1024);
    strcpy(dup_buf, buf);
    insert(dup_buf, timestamp);
  }
}
extern char **environ;

char **parse_PATH(int *size)
{
  char **array;
  const char *original_path = getenv("PATH");
  char *path_var = strdup(original_path ? original_path : "");
  int colon_num = 0, len = strlen(path_var);

  for (int i = 0; i < len; i++) {
    if (path_var[i] == ':') {
      colon_num += 1;
      path_var[i] = '\0';
    }
  }
  array = (char **)malloc((colon_num + 1) * sizeof(char *));
  int current_colon = 0;
  array[current_colon] = strdup(path_var);
  for (int i = 0; i < len; i++)
  {
    if (path_var[i] == '\0') {
      current_colon++;
      array[current_colon] = strdup(path_var + i + 1);
      path_var[i] = ':'; // recovery
      if (array[current_colon][0] == '\0')
      {
        array[current_colon] = ".";
      }
    }
  }
  *size = colon_num + 1;
  return array;
}

int main(int argc, char *argv[]) {
  assert(argc >= 2);
  char **exec_argv = (char **)malloc((argc + 10) * sizeof(char **));
  for (int i = 1; i < argc; i++) {
    exec_argv[i + 2] = argv[i];
  }
  // while (strcmp(argv[1], "tree") == 0 || strcmp(argv[1], "python3") == 0 || strcmp(argv[1], "ls") == 0 || strcmp(argv[1], "./a.out") == 0);
  // char *exec_envp[] = {"PATH=/bin", NULL};
  exec_argv[0] = "strace";
  exec_argv[1] = "-T";
  exec_argv[argc + 2] = NULL;
  int flides[2];
  pipe(flides);
  size_t pid = fork();
  assert(pid >= 0);
  if (pid > 0) {
    //original process, read the output.
    close(flides[1]);
    char buf;
    char cache[1000];
    int cnt = 0;
    struct timeval te;
    gettimeofday(&te, NULL);
    pass_time = te.tv_sec * 1000 + te.tv_usec / 1000;
    int status;
    time_t cur = time(NULL);
    while (waitpid(-1, &status, WNOHANG) == 0 && read(flides[0], &buf, 1) > 0)
    {
      // write(STDOUT_FILENO, &buf, 1);
      cache[cnt++] = buf;
      if (buf == '\n')
      {
        cache[cnt - 1] = '\0';
        handle(cache, cnt - 1);
        cnt = 0;
      }
      if ((time(NULL) - cur) >= 1) {
        output();
        cur = time(NULL);
      }
    }
    output();
    // printf("\033[%dB", TOP);
    close(flides[0]);
  }
  else
  {
    //child process, execute strace
    close(flides[0]);
    int fd = open("/dev/null", O_WRONLY);
    char arg2[32] = "--output=/proc/";
    char pipe_fd[8];
    char pid_str[8];
    sprintf(pid_str, "%d", getpid());
    sprintf(pipe_fd, "%d", flides[1]);
    strcat(arg2, pid_str);
    strcat(arg2, "/fd/");
    strcat(arg2, pipe_fd);
    exec_argv[2] = arg2;
    dup2(fd, 2);
    dup2(fd, 1);
    int size = 0;
    char **path = parse_PATH(&size);
    char buf[1000];
    char *src = "/strace";
    // for (char **itr = exec_argv; *itr != NULL; itr ++) {
    //   printf("%s\n", *itr);
    // }
    for (int i = 0; i < size; i++)
    {
      strcpy(buf, path[i]);
      strcat(buf, src);
      execve(buf, exec_argv, environ);
    }
    perror(argv[0]);
    exit(EXIT_FAILURE);
  }
}
