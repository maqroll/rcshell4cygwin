#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

void
p9longjmp(p9jmp_buf buf, int val)
{
    /*
    sigjmp_buf *s; 
    s = (void *)&buf;
    */
    sigjmp_buf s;
    memcpy(s,buf,sizeof(sigjmp_buf));
    siglongjmp(s, val); 
}

void
p9notejmp(void *x, p9jmp_buf buf, int val)
{
    /*
    sigjmp_buf *s; 
    s = (void *)&buf;
    */
	USED(x);
    sigjmp_buf s;
    memcpy(s,buf,sizeof(sigjmp_buf));
    siglongjmp(s, val); 
}

