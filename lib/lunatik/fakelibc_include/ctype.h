#include <linux/ctype.h>
#undef  isalnum
#define isalnum(c)      (((__ismask(c)&(_U|_L|_D)) != 0) && (c > 0))
#undef  isalpha
#define isalpha(c)      (((__ismask(c)&(_U|_L)) != 0) && (c > 0))
#undef  iscntrl
#define iscntrl(c)      (((__ismask(c)&(_C)) != 0) && (c > 0))
#undef  isdigit
#define isdigit(c)      (((__ismask(c)&(_D)) != 0) && (c > 0))
#undef  isspace
#define isspace(c)      (((__ismask(c)&(_S)) != 0) && (c > 0))
