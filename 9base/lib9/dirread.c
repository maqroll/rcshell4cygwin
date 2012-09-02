#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>
#include <sys/stat.h>
#include <dirent.h>

extern int _p9dir(struct stat*, struct stat*, char*, Dir*, char**, char*);

static int
mygetdents(int fd, struct dirent **buf, int n)
{
    int result = 0;
    struct dirent *p;
    struct dirent *nbuf;
    int reserved = 2056;

	*buf = malloc(reserved);

    if (*buf == nil)
        return -1;

    int fd2 = dup(fd);

    if (fd2 < 0)
        return -1;

    DIR *dir = fdopendir(fd);
    if (dir == nil)
        return -1;

    while ((p = readdir (dir))!=NULL /*&& result < n*/){
        if(sizeof(struct dirent)*(result+1) > reserved)
        {
            nbuf = realloc(*buf,reserved+2056);
            if(nbuf == nil)
            {
                closedir(dir);
                return -1;
}
            reserved += 2056;
            *buf = nbuf;
}
        (*buf)[result] = *p;
        result++;
}

    // Esto tambien cierra fd2
    closedir (dir);

    return result*sizeof(struct dirent);
}

static int
countde(char *p, int n)
{
	char *e;
	int m;
	struct dirent *de;

	e = p+n;
	m = 0;
	while(p < e){
		de = (struct dirent*)p;
		if(sizeof(struct dirent) <= 4+2+2+1 || p+sizeof(struct dirent) > e)
			break;
		if(de->d_name[0]=='.' && de->d_name[1]==0)
			de->d_name[0] = 0;
		else if(de->d_name[0]=='.' && de->d_name[1]=='.' && de->d_name[2]==0)
			de->d_name[0] = 0;
		m++;
		p += sizeof(struct dirent);
	}
	return m;
}

static int
dirpackage(int fd, char *buf, int n, Dir **dp)
{
	int oldwd;
	char *p, *str, *estr;
	int i, nstr, m;
	struct dirent *de;
	struct stat st, lst;
	Dir *d;

	n = countde(buf, n);
	if(n <= 0)
		return n;

	if((oldwd = open(".", O_RDONLY)) < 0)
		return -1;
	if(fchdir(fd) < 0)
		return -1;
		
	p = buf;
	nstr = 0;

	for(i=0; i<n; i++){
		de = (struct dirent*)p;
		memset(&lst, 0, sizeof lst);
		if(de->d_name[0] == 0)
			/* nothing */ {}
		else if(lstat(de->d_name, &lst) < 0)
			de->d_name[0] = 0;
		else{
			st = lst;
			if(S_ISLNK(lst.st_mode))
				stat(de->d_name, &st);
			nstr += _p9dir(&lst, &st, de->d_name, nil, nil, nil);
		}
		p += sizeof(struct dirent);
	}

	d = malloc(sizeof(Dir)*n+nstr);
	if(d == nil){
		fchdir(oldwd);
		close(oldwd);
		return -1;
	}
	str = (char*)&d[n];
	estr = str+nstr;

	p = buf;
	m = 0;
	for(i=0; i<n; i++){
		de = (struct dirent*)p;
		if(de->d_name[0] != 0 && lstat(de->d_name, &lst) >= 0){
			st = lst;
			if((lst.st_mode&S_IFMT) == S_IFLNK)
				stat(de->d_name, &st);
			_p9dir(&lst, &st, de->d_name, &d[m++], &str, estr);
		}
		p += sizeof(struct dirent);
	}

	fchdir(oldwd);
	close(oldwd);
	*dp = d;
	return m;
}

long
dirread(int fd, Dir **dp)
{
	char *buf;
	struct stat st;
	int n;

	*dp = 0;

	if(fstat(fd, &st) < 0)
		return -1;

    // TODO
	//if(st.st_blksize < 8192)
		//st.st_blksize = 8192;

    /*
	if(st.st_blksize < 30000)
		st.st_blksize = 30000;

	buf = malloc(st.st_blksize);
	if(buf == nil)
		return -1;
    */

	n = mygetdents(fd, (void*)(&buf), st.st_blksize);
	if(n < 0){
		free(buf);
		return -1;
	}
	n = dirpackage(fd, buf, n, dp);
	free(buf);
	return n;
}


long
dirreadall(int fd, Dir **d)
{
	uchar *buf, *nbuf;
	long n, ts;
	struct stat st;

	if(fstat(fd, &st) < 0)
		return -1;

	if(st.st_blksize < 8192)
		st.st_blksize = 8192;

	buf = nil;
	ts = 0;
	for(;;){
		nbuf = realloc(buf, ts+st.st_blksize);
		if(nbuf == nil){
			free(buf);
			return -1;
		}
		buf = nbuf;
		n = mygetdents(fd, (void*)(buf+ts), st.st_blksize);
		if(n <= 0)
			break;
		ts += n;
	}
	if(ts >= 0)
		ts = dirpackage(fd, (char*)buf, ts, d);
	free(buf);
	if(ts == 0 && n < 0)
		return -1;
	return ts;
}
