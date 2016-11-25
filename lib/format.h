#include <bits/types.h>

struct tm;

char* fmtchar(char* dst, char* end, char c);

char* fmti32(char* buf, char* end,  int32_t num);
char* fmtu32(char* buf, char* end, uint32_t num);

char* fmti64(char* buf, char* end,  int64_t num);
char* fmtu64(char* buf, char* end, uint64_t num);

char* fmtlong(char* buf, char* end, long num);
char* fmtulong(char* buf, char* end, unsigned long num);
char* fmtpad(char* p, char* e, int width, char* q);
char* fmtpad0(char* p, char* e, int width, char* q);

char* fmtsize(char* p, char* e, uint64_t n);
char* fmtstr(char* dst, char* end, const char* src);
char* fmtstrn(char* dst, char* end, const char* src, int len);

char* fmttm(char* buf, char* end, struct tm* tm);
char* fmtulp(char* buf, char* end, unsigned long num, int pad);

char* parseint(char* buf, int* np);
char* parselong(char* buf, long* np);
char* parseulong(char* buf, unsigned long* np);