#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <regex.h>
#include <sys/wait.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>

extern char **environ;
#define N 10000
#define TOP 5
#define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))

int flides[2];
int amount;
typedef struct entry {
  char *name;
  double time_cost;
} Entry;
Entry syscall_pool[N];

char ** parse_PATH(int *size);
void handle(char *buf);
void display();
void insert(char *name, double time_stamp);
void quick_sort(Entry *q, int l, int r);
void handle_alter(char *buf);


int main(int argc, char *argv[]) {
    assert(argc >= 2);
    if (pipe(flides)) {
        assert(0);
    }
    pid_t pid = fork();
    if (pid == 0)
    {
      close(flides[0]);
      char **exec_argv = (char **)malloc((argc + 10) * sizeof(char **));
      memcpy(exec_argv + 3, argv, sizeof(char*) * argc);
      char pipe_file[64];
      sprintf(pipe_file, "/proc/%d/fd/%d", getpid(), flides[1]);
      exec_argv[0] = "strace";
      exec_argv[1] = "-o";
      exec_argv[2] = pipe_file;
      exec_argv[3] = "-T";
      exec_argv[argc + 3] = NULL;

      int null_fd = open("/dev/null", O_WRONLY);
      dup2(null_fd, STDOUT_FILENO);
      dup2(null_fd, STDERR_FILENO);
      int size = 0;
      char **path = parse_PATH(&size);
      char buf[1000];
      char *src = "/strace";
      for (int i = 0; i < size; i++) {
        strcpy(buf, path[i]);
        strcat(buf, src);
        execve(buf, exec_argv, environ);
      }
      perror(argv[0]);
      exit(EXIT_FAILURE);
    }
    else
    {
      close(flides[1]);
      char buf[1024], tmp;
      time_t cur = time(NULL);
      int status = 0,  cnt = 0;
      while (waitpid(-1, &status, WNOHANG) == 0 && read(flides[0], &tmp, 1) > 0) {
        buf[cnt++] = tmp;
        if (tmp == '\n') {
            buf[cnt] = '\0';
            cnt = 0;
            handle_alter(buf);
        }
        if ((time(NULL) - cur) >= 1) {
            display();
            cur = time(NULL);
        }
      }
      display();
      close(flides[0]);
    }
}

char ** parse_PATH(int *size) {
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


void handle(char *buf) {
  char* s = buf;
  char re[] = "(\\w+)[\\(].*[\\)].*<(.*)>\n";
  regex_t     regex;
  regmatch_t  pmatch[3];
  if (regcomp(&regex, re, REG_EXTENDED)) exit(EXIT_FAILURE);
    regoff_t    len;
  
  if (!regexec(&regex, s, ARRAY_SIZE(pmatch), pmatch, 0)) {
    len = pmatch[1].rm_eo - pmatch[1].rm_so;
    char* name = (char*)malloc(sizeof(char) * len);
    strncpy(name, s + pmatch[1].rm_so, len);

    len = pmatch[2].rm_eo - pmatch[2].rm_so;
    char* time_s = (char*)malloc(sizeof(char) * len);
    strncpy(time_s, s + pmatch[2].rm_so, len);
    
    double time = atof(time_s);
    insert(name, time);
  }
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

void handle_alter(char *buf) {
  char *name = buf;
  int len = strlen(buf);
  char correct_prefix[] = "^[a-zA-Z0-9_]+\\(";
  char correct_suffix[] = "\\<[0-9]+\\.[0-9]+\\>";  
  if (matchRegex(correct_prefix, buf) && matchRegex(correct_suffix, buf)) {
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
    char *dup_buf = (char*) malloc(1024);
    strcpy(dup_buf, buf);
    insert(dup_buf, timestamp);
  }
}

void display() {
    quick_sort(syscall_pool, 0, amount - 1);
    double sum = 0;
    for (int i = 0; i < amount; i++) {
      sum += syscall_pool[i].time_cost;
    }
    int limit = amount <= 5 ? amount : 5;
    for (int i = 0; i < limit; i++) {
        printf("%s (%d%%)\n", syscall_pool[i].name, (int)((syscall_pool[i].time_cost / sum) * 100));
    }
    for (int i = 0; i < 80; i++) {
        printf("%c", 0);
    }
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