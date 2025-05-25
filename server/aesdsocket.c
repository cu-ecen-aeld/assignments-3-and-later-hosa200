#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>

#define MYPORT "9000" // the port users will be connecting to
#define BACKLOG 10    // how many pending connections queue will hold
#define LEN 100
#define FILE_NAME "/var/tmp/aesdsocketdata"
#define MAX_BUFFER_LEN 50000

int sockfd;
int new_sockfd;

/* handler for SIGINT and SIGTERM */
static void signal_handler(int signo)
{
    int status;

    if ((signo == SIGINT) || (signo == SIGTERM))
    {
        close(new_sockfd);
        close(sockfd);
        status = remove(FILE_NAME);
        if (status == -1)
        {
            /* error */
            printf("Error 16 %s\n", strerror(errno));
            syslog(LOG_INFO, "Error deleting the file\n");
            closelog();
            exit(1);
        }
        syslog(LOG_INFO, "Caught signal, exiting\n");
        closelog();
    }
    else
    {
        /* this should never happen */
        printf("Error 17 %s\n", strerror(errno));
        syslog(LOG_INFO, "Error signal\n");
        closelog();
        exit(1);
    }
    exit(0);
}

int main(int argc, char *argv[])
{
    struct addrinfo hints;
    struct addrinfo *res;
    int status;
    // struct sockaddr their_addr;
    // socklen_t their_addr_len;
    char recv_buf[MAX_BUFFER_LEN] = {'\0'};
    char send_buf[MAX_BUFFER_LEN] = {'\0'};
    int recv_len = 0;
    int send_len = 0;
    int file_fd;

    /* open log file */
    openlog(NULL, 0, LOG_INFO);

    /* Register a signal */
    if (signal(SIGINT, signal_handler) == SIG_ERR)
    {
        printf("Error 14 %s\n", strerror(errno));
        syslog(LOG_INFO, "Can't handle SIGINT or SIGTERM");
        closelog();
        exit(1);
    }
    if (signal(SIGTERM, signal_handler) == SIG_ERR)
    {
        printf("Error 15 %s\n", strerror(errno));
        syslog(LOG_INFO, "Can't handle SIGINT or SIGTERM");
        closelog();
        exit(1);
    }
    /* intialize hints for socket */
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    /* first, load up address structs with getaddrinfo(): */
    status = getaddrinfo(NULL,
                         MYPORT,
                         &hints,
                         &res);
    if (status != 0)
    {
        /* Error to handle */
        printf("Error 1 %s\n", strerror(errno));
        syslog(LOG_INFO, "Exit program due to getaddrinfo failure ID: %d\n", status);
        closelog();
        exit(1);
    }

    /* make a socket */
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1)
    {
        /* Error to handle */
        printf("Error 2 %s\n", strerror(errno));
        syslog(LOG_INFO, "Exit program due to socket failure ID: %d\n", sockfd);
        closelog();
        /* free the address info after using */
        freeaddrinfo(res);
        exit(1);
    }

    /* bind to that socket */
    status = bind(sockfd, res->ai_addr, res->ai_addrlen);
    if (status != 0)
    {
        /* Error to handle */
        printf("Error 3 %s\n", strerror(errno));
        syslog(LOG_INFO, "Exit program due to bind failure ID: %d\n", status);
        closelog();
        /* free the address info after using */
        freeaddrinfo(res);
        exit(1);
    }

    /* free the address info after using */
    freeaddrinfo(res);

    /* run as a daemon if needed*/
    if (argc > 1 && *argv[1] == 45)
    {
        status = daemon(0, 0);
        if (status != 0)
        {
            /* Error to handle */
            printf("Error 18 %s\n", strerror(errno));
            syslog(LOG_INFO, "Exit program due to failing to run as a demon");
            closelog();
            /* free the address info after using */
            freeaddrinfo(res);
            exit(1);
        }
    }

    /* listen to that socket */
    status = listen(sockfd, BACKLOG);
    if (status != 0)
    {
        /* Error to handle */
        printf("Error 4 %s\n", strerror(errno));
        syslog(LOG_INFO, "Exit program due to listen failure ID: %d\n", status);
        closelog();
        exit(1);
    }

    /* accept incoming connection */
    // their_addr_len = sizeof(their_addr);
    while (1)
    {
        new_sockfd = accept(sockfd, res->ai_addr, &res->ai_addrlen);
        // new_sockfd = accept(sockfd, (struct sockaddr *)&their_addr, &their_addr_len);
        if (new_sockfd == -1)
        {
            /* Error to handle */
            printf("Error 5 %s\n", strerror(errno));
            syslog(LOG_INFO, "Exit program due to accept failure ID: %d\n", new_sockfd);
            closelog();
            exit(1);
        }

        /* Logs message to the syslog with the IP address of the connected client*/
        syslog(LOG_INFO, "Accepted connection from %d", 1 /*inet_ntoa(their_addr.ss_family)*/);

        /* start receving */
        recv_len = recv(new_sockfd, (void *)recv_buf, MAX_BUFFER_LEN - 1, 0);
        if (recv_len < 0)
        {
            /* Error to handle */
            printf("Error 6 %s\n", strerror(errno));
            syslog(LOG_INFO, "Exit program due to recv failure ID: %d\n", status);
            closelog();
            exit(1);
        }
        else if (recv_len == 0)
        {
            /* no more reception */
            printf("Error 13 %s\n", strerror(errno));
            syslog(LOG_INFO, "stop receiving data from client\n");
            closelog();
            break;
        }
        else
        {
            /* recieved data */
            if (recv_buf[recv_len - 1] == '\n')
            {
                /* open file to recieve data */
                file_fd = open(FILE_NAME, O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (file_fd == -1)
                {
                    /* error */
                    printf("Error 7 %s\n", strerror(errno));
                    syslog(LOG_INFO, "Error opening or creating the file\n");
                    closelog();
                    exit(1);
                }
                /* write to the file */
                status = write(file_fd, (void *)recv_buf, (recv_len * sizeof(char)));
                if (status == -1)
                {
                    /* error */
                    printf("Error 8 %s\n", strerror(errno));
                    syslog(LOG_INFO, "Error wrting to the file: %s\n", FILE_NAME);
                    if (close(file_fd) == -1)
                        syslog(LOG_INFO, "Error closing the file: %s\n", FILE_NAME);
                    closelog();
                    exit(1);
                }
                else
                {
                    syslog(LOG_INFO, "Successfull wrting to the file: %s\n", FILE_NAME);
                    if (close(file_fd) == -1)
                        syslog(LOG_INFO, "Error closing the file: %s\n", FILE_NAME);
                }
                /* clear buffer */
                memset(recv_buf, '\0', sizeof(recv_buf));

                /* open file to read data */
                file_fd = open(FILE_NAME, O_RDONLY, 0644);
                if (file_fd == -1)
                {
                    /* error */
                    printf("Error 9 %s\n", strerror(errno));
                    syslog(LOG_INFO, "Error opening or creating the file\n");
                    closelog();
                    exit(1);
                }
                send_len = lseek(file_fd, 0, SEEK_END); // seek to end of file
                status = lseek(file_fd, 0, SEEK_SET);   // seek back to beginning of file
                status = read(file_fd, (void *)send_buf, send_len);
                if (status == -1)
                {
                    /* error */
                    printf("Error 10 %s\n", strerror(errno));
                    syslog(LOG_INFO, "Error reading file: %s\n", FILE_NAME);
                    closelog();
                    exit(1);
                }
                /* write to client */
                if (close(file_fd) == -1)
                    syslog(LOG_INFO, "Error closing the file: %s\n", FILE_NAME);
                status = send(new_sockfd, (void *)send_buf, send_len, 0); /*May be is not sending TODO:*/
                if (status == -1)
                {
                    /* error */
                    printf("Error 11 %s\n", strerror(errno));
                    syslog(LOG_INFO, "Error sending back to socket\n");
                    if (close(file_fd) == -1)
                        syslog(LOG_INFO, "Error closing the file: %s\n", FILE_NAME);
                    closelog();
                    exit(1);
                }

                /* clear buffer */
                memset(send_buf, '\0', sizeof(send_buf));
            }
            else
            {
                /* if data recieved doesn't have \n */
            }
        }
    }
}
