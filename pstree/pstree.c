#include <stdio.h>
#include <assert.h>
#include <getopt.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
pid_t *pid_pool;
unsigned capacity;
unsigned pool_size;
typedef struct node {
  pid_t pid;
  pid_t ppid;
  char name[128];
  unsigned capacity;
  unsigned size;
  struct node **children;
} Node;
Node *root;
Node *nodes;
int p = 0;
int n = 0;
int flag = 1;
#define INIT_NODE(X) (X)->capacity = 16, (X)->size = 0, (X)->children = (Node **)malloc(sizeof(Node *) * 16)
// helper functions.
// check whether str is a number. return 1 for true, 0 for false.
int
checkNumber(const char *str)
{
  const char* ptr = str;
  while (*ptr != '\0') {
    if (*ptr < '0' || *ptr > '9') {
      return 0;
    }
    ptr++;
  }
  return 1;
}
//enlarge pid pool when current is full.
void enlarge_pool() {
  pid_t *space = (pid_t *)malloc(sizeof(pid_t) * 2 * capacity);
  memcpy(space, pid_pool, capacity * sizeof(pid_t));
  capacity <<= 1;
  free(pid_pool);
  pid_pool = space;
}
//enlarge Node's children array when it's full.
void enlarge_children(Node* node) {
  Node** space = (Node **)malloc(sizeof(Node *) * 2 * node->capacity);
  memcpy(space, node->children, node->capacity * sizeof(Node *));
  node->capacity <<= 1;
  free(node->children);
  node->children = space;
}
//concatenate two strings.
char* concat(const char* str1, const char* str2) {
  char *result = malloc(strlen(str1) + strlen(str2) + 1);
  strcpy(result, str1);
  strcat(result, str2);
  return result;
}
//check whether a string matches the pattern of "PPid:[s]*[0-9]+". 
//if true, convert number to string and store it in ppid and return 1, otherwise return 0.
int check_match(const char* str, pid_t* ppid) {
  char *pattern = "PPid:";
  const char *ptr = str;
  while (*ptr != '\0') {
    if (*ptr++ != *pattern++) {
      return 0;
    }
    if (*pattern == '\0') {
      break;
    }
  }
  while (*ptr < '0' || *ptr > '9')
    ptr++;
  *ppid = (pid_t)strtol(ptr, NULL, 10);
  return 1;
}
int check_name(char* str, char* name) {
  char *pattern = "Name:";
  char *ptr = str;
  while (*ptr != '\0') {
    if (*ptr++ != *pattern++) {
      return 0;
    }
    if (*pattern == '\0') {
      break;
    }
  }
  while (*ptr < 'A' || *ptr > 'z')
    ptr++;
  char *tmp = ptr;
  while (*tmp++ != '\n')
    ;
  *(tmp - 1) = '\0';
  strcpy(name, ptr);
  return 1;
}
//push
void push_child(Node* node, Node* child) {
  if (node->size >= node->capacity) {
    enlarge_children(node);
  }
  node->children[node->size++] = child;
}
//build tree from (child, parent) pairs.
void swap(Node** q, int l, int r) {
  Node *tmp = q[l];
  q[l] = q[r];
  q[r] = tmp;
}
void quick_sort(Node** q, int l, int r) {
    if (l >= r) {
        return;
    }
    int pivot = q[(l + r) >> 1]->pid, i = l - 1, j = r + 1;
    while (i < j) {
        while (q[++i]->pid < pivot)
            ;
        while (q[--j]->pid > pivot)
            ;

        if (i < j)
            swap(q, i, j);
    }
    quick_sort(q, l, j), quick_sort(q, j + 1, r);
}
void build() {
  char *created = (char *)malloc(sizeof(char) * pool_size);
  Node* head[pool_size + 10];
  int begin = 0;
  int end = 1;
  head[begin] = root;
  memset(created, 0, pool_size);
  while (begin < end) {
    for (int i = 0; i < pool_size; i++) {
      if (!created[i] && nodes[i].ppid == head[begin]->pid)
      {
        push_child(head[begin], &nodes[i]);
        created[i] = 1;
        head[end++] = &nodes[i];
      }
    }
    begin++;
  }
  root = root->children[0];

}

void print_tree(Node* node, char* blank) {
  if (node->size == 0) {
    printf("%s%s", blank, node->name);
    if (p) {
      printf("(%d)", node->pid);
    }
    printf("\n");
    return;
  }
  printf("%s%s", blank, node->name);
  if (p) {
      printf("(%d)", node->pid);
  }
  printf("\n");
  if (n)
  {
    quick_sort(node->children, 0, node->size - 1);
  }
  for (int i = 0; i < node->size; i++) {
    print_tree(node->children[i], concat(blank, " "));
  }
}

void get_pid() {
  DIR *dir;
  struct dirent *ptr;
  if ((dir = opendir("/proc")) == NULL) {
    exit(1);
  }
  unsigned cnt = 0;
  while ((ptr = readdir(dir)) != NULL)
  {
    if (ptr->d_type == 4 && checkNumber(ptr->d_name))
    {
      if (cnt >= capacity) {
        enlarge_pool();
      }
      pid_pool[cnt++] = (pid_t)strtol(ptr->d_name, NULL, 10);
    }
  }
  pool_size = cnt;
}

void get_ppid() {
 // ppid_pool = (pid_t *)malloc(sizeof(pid_t) * pool_size);
  nodes = (Node *)malloc(sizeof(Node) * pool_size);
  FILE *fptr;
  for (int i = 0; i < pool_size; i++)
  {
    //num2str
    char pid_str[16];
    sprintf(pid_str, "%d", pid_pool[i]);
    //concatenate path string
    char *path = "/proc/";
    path = concat(path, pid_str);
    path = concat(path, "/status");
    //read /proc/pid/status
    fptr = fopen(path, "r");
    if (fptr == NULL) {
      exit(1);
    }
    char *line = NULL;
    size_t len = 0;
    pid_t ppid;
    INIT_NODE(&nodes[i]);
    while (getline(&line, &len, fptr) != -1)
    {
      if (check_match(line, &ppid) == 1) {
        //ppid_pool[i] = ppid;
        nodes[i].pid = pid_pool[i];
        nodes[i].ppid = ppid;
      } else {
        check_name(line, nodes[i].name);
      }
    }
    fclose(fptr);
  }
}

int main(int argc, char *argv[]) {
//  for (int i = 0; i < argc; i++) {
//    assert(argv[i]);
//    printf("argv[%d] = %s\n", i, argv[i]);
//  }
  assert(!argv[argc]);
  const struct option table[] = {
    {"show-pids", no_argument, NULL, 'p'},
    {"numeric-sort", no_argument, NULL, 'n'},
    {"version", no_argument, NULL, 'V'}
  };
  //get options
  int o;
  while ((o = getopt_long(argc, argv, "-pnV", table, NULL)) != -1) {
      switch (o) {
          case 'p':p=1;break;
          case 'n':n=1;break;
          case 'V':flag=0;fprintf(stderr, "pstree (PSmisc) UNKNOWN\n"
"Copyright (C) 1993-2019 Werner Almesberger and Craig Small\n\n"
"PSmisc comes with ABSOLUTELY NO WARRANTY.\n"
"This is free software, and you are welcome to redistribute it under\n"
"the terms of the GNU General Public License.\n"
"For more information about these matters, see the files named COPYING.\n");break;
          default:printf("Unknowable commands");assert(0);
      }
  }
  //initialize pid pool with size of 256.
  capacity = 256;
  pid_pool = (pid_t *)malloc(sizeof(pid_t) * capacity);
  root = (Node *)malloc(sizeof(Node));
  INIT_NODE(root);
  root->pid = 0;
  root->ppid = -1;
  get_pid();
  get_ppid();
  build();
  if (flag) {
    print_tree(root, "");
  }
  return 0;
}
