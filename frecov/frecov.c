#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <wchar.h>
#include <locale.h>

/**
 * @brief Macro
 * 
 */
#define DIR_BOUND 10
#define IS_DIR 3
#define HEADER_BOUND 20000
#define LAST_LONG_ENTRY 0x40
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08 
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_NAME_MASK (ATTR_READ_ONLY |\
                               ATTR_HIDDEN |\
                               ATTR_SYSTEM |\
                               ATTR_VOLUME_ID |\
                               ATTR_DIRECTORY |\
                               ATTR_ARCHIVE)
#define ATTR_LONG_NAME (ATTR_READ_ONLY | ATTR_HIDDEN | \
                            ATTR_SYSTEM | ATTR_VOLUME_ID)
#define STRATEGY_NEW 1 
// #define TRACE

#define RED "\e[0;31m"
#define GREEN "\e[0;32m"
#define YELLOW "\e[0;33m"
#define BLUE "\e[0;34m"
#define RESET "\e[0m"

#define LONG_NAME_1_BYTES 10
#define LONG_NAME_2_BYTES 12
#define LONG_NAME_3_BYTES 4
#define LONG_NAME_BYTES (LONG_NAME_1_BYTES + LONG_NAME_2_BYTES + LONG_NAME_3_BYTES)

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

// Copied from the manual
struct fat32hdr {
  u8  BS_jmpBoot[3];
  u8  BS_OEMName[8];
  u16 BPB_BytsPerSec;
  u8  BPB_SecPerClus;
  u16 BPB_RsvdSecCnt;
  u8  BPB_NumFATs;
  u16 BPB_RootEntCnt;
  u16 BPB_TotSec16;
  u8  BPB_Media;
  u16 BPB_FATSz16;
  u16 BPB_SecPerTrk;
  u16 BPB_NumHeads;
  u32 BPB_HiddSec;
  u32 BPB_TotSec32;
  u32 BPB_FATSz32;
  u16 BPB_ExtFlags;
  u16 BPB_FSVer;
  u32 BPB_RootClus;
  u16 BPB_FSInfo;
  u16 BPB_BkBootSec;
  u8  BPB_Reserved[12];
  u8  BS_DrvNum;
  u8  BS_Reserved1;
  u8  BS_BootSig;
  u32 BS_VolID;
  u8  BS_VolLab[11];
  u8  BS_FilSysType[8];
  u8  __padding_1[420];
  u16 Signature_word;
} __attribute__((packed));

struct DIR {
  char DIR_Name[11];
  u8 DIR_Attr;
  u8 DIR_NTRes;
  u8 DIR_CrtTimeTenth;
  u16 DIR_CrtTime;
  u16 DIR_CrtDate;
  u16 DIR_LstAccDate;
  u16 DIR_FstClusHI;
  u16 DIR_WrtTime;
  u16 DIR_WrtDate;
  u16 DIR_FstClusLO;
  u32 DIR_FileSize;
} __attribute__((packed));

struct LDIR {
  u8 LDIR_Ord;
  char LDIR_Name1[10];
  u8 LDIR_Attr;
  u8 LDIR_Type;
  u8 LDIR_Chksum;
  char LDIR_Name2[12];
  u16 LDIR_FstClusLO;
  char LDIR_Name3[4];
} __attribute__((packed));

u32 countOfClusters;
u32 rootDirSectors;
struct fat32hdr *hdr;
u32 dirs[DIR_BOUND];
u32 headers[HEADER_BOUND];
u32 n_dirs, n_headers;

void *map_disk(const char *fname);
void *getClusterByID(u32 cid);
void snapShot();
void dispatch(u32 cid);
void parseDIR(u32 cid);
void convertUtf2Asc(char *utf, char *asc);
int checkLongDir(struct LDIR *dir);
int checkLongLast(struct LDIR *dir);
void *readEntry(void *begin, void *end);
void cpyName(char *filename, u8 idx, struct LDIR *dir);
void *extractLongName(struct LDIR *last, void *bound, char *filename);
void convertUtf2Asc(char *utf, char *asc);
void parseDIR_old(u32 cid);
void displayEntry(struct LDIR *dir);
void displayList(void *begin, void *end);
void xxd(void *ptr, size_t bytes, const char* color);

int main(int argc, char *argv[]) {
  setlocale(LC_CTYPE, "");
  if (argc < 2) {
    fprintf(stderr, "Usage: %s fs-image\n", argv[0]);
    exit(1);
  }

  setbuf(stdout, NULL);

  assert(sizeof(struct fat32hdr) == 512); // defensive
  assert(sizeof (struct DIR) == 32);
  assert(sizeof (struct LDIR) == 32);

  // map disk image to memory
  hdr = map_disk(argv[1]);
  
  // printf("cluster size = %d\n", hdr->BPB_BytsPerSec * hdr->BPB_SecPerClus);
  // TODO: frecov
  snapShot();

  // file system traversal
  munmap(hdr, hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec);
  #ifdef TRACE
  printf(RESET);
  #endif
}

void *map_disk(const char *fname) {
  int fd = open(fname, O_RDWR);

  if (fd < 0) {
    perror(fname);
    goto release;
  }

  off_t size = lseek(fd, 0, SEEK_END);
  if (size == -1) {
    perror(fname);
    goto release;
  }

  struct fat32hdr *hdr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (hdr == (void *)-1) {
    goto release;
  }

  close(fd);

  if (hdr->Signature_word != 0xaa55 ||
      hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec != size) {
    fprintf(stderr, "%s: Not a FAT file image\n", fname);
    goto release;
  }
  return hdr;

release:
  if (fd > 0) {
    close(fd);
  }
  exit(1);
}

void snapShot() {
  rootDirSectors = ((hdr->BPB_RootEntCnt * 32) + (hdr->BPB_BytsPerSec - 1)) / hdr->BPB_BytsPerSec;
  u32 totalSec = hdr->BPB_TotSec32;
  u32 dataSec = totalSec - (hdr->BPB_RsvdSecCnt + (hdr->BPB_NumFATs * hdr->BPB_FATSz32) + rootDirSectors);
  countOfClusters = dataSec / hdr->BPB_SecPerClus; //dataClusters
  // printf("# of clusters = %u\n", countOfClusters);

  for (u32 i = 2; i < countOfClusters + 2; i++) { //Cluster #0 and #1 are reversed.
    dispatch(i);
  }

  for (int i = 0; i < n_dirs; i++) {
#ifdef STRATEGY_NEW
    parseDIR(dirs[i]);
#else
    parseDIR_old(dirs[i]);
#endif
  }
} 

void *getClusterByID(u32 cid) {
  // assert(cid > 1);
  u32 fisrtDataSec = hdr->BPB_RsvdSecCnt + (hdr->BPB_NumFATs * hdr->BPB_FATSz32) + rootDirSectors;
  u32 start = ((cid - 2) * hdr->BPB_SecPerClus) + fisrtDataSec;
  void *buf = (void *)hdr + (hdr->BPB_BytsPerSec * start);
  assert(buf != NULL);
  return buf;
}

void dispatch(u32 cid) {
  void *cluster = getClusterByID(cid);
  if (*(u16 *)(cluster) == 0x4d42) {
    headers[n_headers++] = cid;
    return;
  }
  int cnt = 0;
  for (u32 i = 0; i + 2 < hdr->BPB_BytsPerSec * hdr->BPB_SecPerClus; i += 1) {
    char ch1 = *(char *)(cluster + i);
    char ch2 = *(char *)(cluster + i + 1);
    char ch3 = *(char *)(cluster + i + 2);
    if (ch1 == 'B' && ch2 == 'M' && ch3 == 'P') {
      cnt++;
    }
  }
  if (cnt > IS_DIR) {
    dirs[n_dirs++] = cid;
  }
}

int checkLongDir(struct LDIR *dir) {
  return (dir->LDIR_Attr & ATTR_LONG_NAME_MASK) == ATTR_LONG_NAME;
}

int checkLongLast(struct LDIR *dir) {
  return dir->LDIR_Ord > LAST_LONG_ENTRY;
}

void *readEntry(void *begin, void *end) {
  struct LDIR *headEntry = (struct LDIR*) begin;
  assert(begin < end);

  if (checkLongDir(headEntry) && checkLongLast(headEntry)) {
      //解析文件名
      char utfName[256], ascName[128], path[160], sha1Process[196], sha1sum[64];
      memset(utfName, 0, sizeof(utfName));
      memset(ascName, 0, sizeof(ascName));
      // memset(path, 0, sizeof(path));
      // memset(sha1Process, 0, sizeof(sha1Process));
      // memset(sha1sum, 0, sizeof(sha1sum));
      struct DIR* entry = (struct DIR*) extractLongName(headEntry, end, utfName);
      if (entry == NULL) {
        return end;
      }
      if ((u8)entry->DIR_Name[0] == 0xe5 || (u8)entry->DIR_Name[0] == 0) {
        return entry + 1;
      }
      #ifdef TRACE
      for (struct LDIR *itr = headEntry; itr <= (struct LDIR *) entry; itr++) {
        displayEntry(itr);
      }
      #endif
      assert(!checkLongDir((struct LDIR*) entry));
      convertUtf2Asc(utfName, ascName);
      sprintf(path, "/tmp/%s", ascName);
      sprintf(sha1Process, "sha1sum %s", path);

      //顺序读取文件内容
      u32 cid = entry->DIR_FstClusHI << 16 | entry->DIR_FstClusLO;
      void *cluster = getClusterByID(cid);
      FILE *fp = fopen(path, "w");
      fwrite(cluster, 1, entry->DIR_FileSize, fp);
      fclose(fp);
      u32 bytesPerCluster = hdr->BPB_BytsPerSec * hdr->BPB_SecPerClus;
      u32 clustersOfFile = (entry->DIR_FileSize + bytesPerCluster - 1) / bytesPerCluster;
      // printf(BLUE "=============================================================\n" RESET);
      // for (u32 i = 0; i < clustersOfFile; i++) {
      //   void *begin = getClusterByID(cid + i);
      //   void *end = begin + bytesPerCluster;
      //   xxd(begin, 32, RED);
      //   xxd(end - 32, 32, GREEN);
      // }

      //计算校验和
      fp = popen(sha1Process, "r");
      fscanf(fp, "%s", sha1sum);
      pclose(fp);

      //输出
      printf("%s %s\n", sha1sum, ascName);
      fflush(NULL);
      return entry + 1;
  }

  while ((void *) headEntry < end && checkLongDir(headEntry)) {
    headEntry++;
  }

  headEntry ++; //skip DIR entry.
  return (void *) headEntry;
}

void cpyName(char *filename, u8 idx, struct LDIR *dir) {
  memcpy(filename + idx, dir->LDIR_Name1, LONG_NAME_1_BYTES);
  memcpy(filename + idx + LONG_NAME_1_BYTES, dir->LDIR_Name2, LONG_NAME_2_BYTES);
  memcpy(filename + idx + LONG_NAME_1_BYTES + LONG_NAME_2_BYTES, dir->LDIR_Name3, LONG_NAME_3_BYTES);
}

/**
 * @brief parse a set of LONG_NAME_ENTRIES.
 * 
 * @param last 
 * @param bound 
 * @param filename 
 * @return NULL if interrupted.
 */
void *extractLongName(struct LDIR *last, void *bound, char *filename) {
  assert(checkLongDir(last) && checkLongLast(last));

  u8 idx = ((last->LDIR_Ord & ~LAST_LONG_ENTRY) - 1) * LONG_NAME_BYTES;
  cpyName(filename, idx, last);

  struct LDIR *itr = last + 1;
  while ((void *)itr < bound && checkLongDir(itr)) {
    idx = ((itr->LDIR_Ord) - 1) * LONG_NAME_BYTES;
    cpyName(filename, idx, itr);
    itr++;
  }

  assert((void *) (itr - 1) < bound);

  if (((void *) itr >= bound) && (checkLongDir(itr - 1))) {
    return NULL; //a set of LongDirEntries was interrupted.
  }

  assert(!checkLongDir(itr));

  return itr;
}

void convertUtf2Asc(char *utf, char *asc) {
  for (int i = 0; i < 256; i += 2) {
    if (*(u16 *)(utf + i) == 0) {
      break;
    }
    sprintf(asc + i / 2, "%lc", *(u16 *)(utf + i));
  }
}

void parseDIR(u32 cid) {
  void *dir = getClusterByID(cid);
  void *end = dir + hdr->BPB_BytsPerSec * hdr->BPB_SecPerClus;
  // displayList(dir, end);
  while (dir < end) {
    dir = readEntry(dir, end);
  }
}

void parseDIR_old(u32 cid) {
  void *dir = getClusterByID(cid);
  void *end = dir + hdr->BPB_BytsPerSec * hdr->BPB_SecPerClus;
  // displayList(dir, end);
  char buf[256];
  int cnt = 0;
  for (struct DIR *entry = (struct DIR*)dir; (void*)entry < end; entry++) {
    cnt++;
    struct LDIR* tmp = (struct LDIR *)entry;
    #ifdef TRACE
    displayEntry(tmp);
    #endif
    if (tmp->LDIR_FstClusLO == 0) {
      if (tmp->LDIR_Ord > 0x40) {
        u8 idx = ((tmp->LDIR_Ord & ~0x40) - 1) * 26;
        memcpy(buf + idx, tmp->LDIR_Name1, 10);
        memcpy(buf + idx + 10, tmp->LDIR_Name2, 12);
        memcpy(buf + idx + 22, tmp->LDIR_Name3, 4);
      } else {
        u8 idx = ((tmp->LDIR_Ord) - 1) * 26;
        memcpy(buf + idx, tmp->LDIR_Name1, 10);
        memcpy(buf + idx + 10, tmp->LDIR_Name2, 12);
        memcpy(buf + idx + 22, tmp->LDIR_Name3, 4);
      }
    } else {
      // printf("%d\n", cnt);
      if (entry->DIR_Name[0] == '.' || (u8)entry->DIR_Name[0] == 0xe5) {
        continue;
      }
      while (buf[0] == '\0') {
        // printf("crazy\n");
      }
      char filename[256];
      for (int i = 0; i < 256; i += 2) {
        if (*(u16 *)(buf + i) == 0) {
          break;
        }
        sprintf(filename + i / 2, "%lc", *(u16 *)(buf + i));
      }
      char path[512] = "./images/";
      strcat(path, filename);
      u32 cid = entry->DIR_FstClusHI << 16 | entry->DIR_FstClusLO;
      void *cluster = getClusterByID(cid);
      FILE *fp = fopen(path, "w");
      fwrite(cluster, 1, entry->DIR_FileSize, fp);
      fclose(fp);
      char *sha1 = (char *) malloc(strlen(path) + 16);
      strcpy(sha1, "sha1sum ");
      strcat(sha1, path);
      fp = popen(sha1, "r");
      char buffer[128];
      fscanf(fp, "%s", buffer); // Get it!
      pclose(fp);
      printf("%s ", buffer);
      for (int i = 0; i < 256; i += 2) {
        if (*(u16 *)(buf + i) == 0) {
          break;
        }
        printf("%lc", *(u16 *)(buf + i));
      }
      printf("\n");
      memset(buf, 0, sizeof(buf));
    }
  }
}

void displayEntry(struct LDIR *dir) {
  if (!checkLongDir(dir)) {
    struct DIR *entry = (struct DIR *) dir;
    printf(RESET "==============" BLUE "DIR" RESET "==============" "\n         ");
    printf("%s\n", entry->DIR_Name);
  } else {
    printf(RESET "==============" RED "LONG" RESET "==============" "\n         ");
    for (int i = 0; i < LONG_NAME_1_BYTES; i += 2) {
      printf("%lc", *(u16 *)(dir->LDIR_Name1 + i));
    }
    for (int i = 0; i < LONG_NAME_2_BYTES; i += 2) {
      printf("%lc", *(u16 *)(dir->LDIR_Name2 + i));
    }
    for (int i = 0; i < LONG_NAME_3_BYTES; i += 2) {
      printf("%lc", *(u16 *)(dir->LDIR_Name3 + i));
    }
    printf("\n");
  }
}

void displayList(void *begin, void *end) {
  assert(begin < end);
  while (begin < end) {
    displayEntry((struct LDIR*) begin);
    begin += sizeof(struct LDIR);
  }
}

void xxd(void *ptr, size_t bytes, const char* color) {
  for (size_t i = 0; i < bytes; i++) {
    printf("%s%02x", color, *(u8*)(ptr + i));
  }
  printf(RESET "\n");
}