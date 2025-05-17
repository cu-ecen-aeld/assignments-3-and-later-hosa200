#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <string.h>

int fd;

int main(int argc, char *argv[])
{
    openlog(NULL,0,LOG_USER);
    if (argc < 3)
    {
        syslog(LOG_ERR,"Invalid number \(%d\) of arguments, need 2 arguments\n",argc);
        syslog(LOG_ERR,"The arguments shall be:\n");
        syslog(LOG_ERR,"1\) A full path to a file \(including filename\) on the filesystem\n");
        syslog(LOG_ERR,"2\) A text string which will be written within this file\n");
        closelog();
        return 1;
    }
    else
    {
        fd = open (argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1)
        {
            /* error */
            syslog(LOG_ERR,"Error opening or creating the file");
            closelog();
            return 1;
        }
        else
        {
        ssize_t nr;
        /* write the string in 'buf' to 'fd' */
        nr = write (fd, argv[2], strlen (argv[2]));
        if (nr == -1)
           { 
            /* error */ 
             syslog(LOG_ERR,"Error wrting to the file: %s",argv[1]);
             closelog();
            return 1;
            }
            else{
            syslog(LOG_DEBUG,"Successfull wrting to the file: %s",argv[1]);
            if (close (fd) == -1)
               syslog(LOG_ERR,"Error closing the file: %s",argv[1]);

            closelog();
            return 0;
            }
        }
    }
}