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
#include <pthread.h>
#include <sys/queue.h>
#include <time.h>
#include <stdbool.h>

/* Defines */
#define USE_AESD_CHAR_DEVICE 1
#define MYPORT "9000" // the port users will be connecting to
#define BACKLOG 10    // how many pending connections queue will hold
#define LEN 100

#if USE_AESD_CHAR_DEVICE
    #define FILE_NAME "/dev/aesdchar"
#else
    #define FILE_NAME "/var/tmp/aesdsocketdata"
#endif

#define MAX_BUFFER_LEN 50000

/* Types */
typedef struct node
{
    pthread_t thrd_id;
    bool thrd_comp;
    int sockfd;
    TAILQ_ENTRY(node)
    nodes_ptr;
} node_t;

typedef TAILQ_HEAD(head_s, node) head_t;

/* Globals */
int sockfd;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
struct addrinfo *res;

/* local functions */
/* handler for SIGINT and SIGTERM */
static void signal_handler(int signo)
{
#if USE_AESD_CHAR_DEVICE

#else
    int status = 0;
#endif
    if ((signo == SIGINT) || (signo == SIGTERM))
    {
        // close(new_sockfd);
        close(sockfd);
#if USE_AESD_CHAR_DEVICE

#else
        status = remove(FILE_NAME);
        if (status == -1)
        {
            /* error */
            printf("Error 16 %s\n", strerror(errno));
            syslog(LOG_INFO, "Error deleting the file\n");
            closelog();
            exit(1);
        }
#endif
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

/* registeration of signals */
static int signal_reg()
{
    if (signal(SIGINT, signal_handler) == SIG_ERR)
    {
        printf("Error 14 %s\n", strerror(errno));
        return -1;
    }

    if (signal(SIGTERM, signal_handler) == SIG_ERR)
    {
        printf("Error 15 %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

/* setup a new socket */
static int socket_setup(void)
{
    struct addrinfo hints;
    int status;

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
        printf("Error 1 %s\n", strerror(errno));
        return -1;
    }
    /* make a socket */
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1)
    {
        /* Error to handle */
        printf("Error 2 %s\n", strerror(errno));
        /* free the address info after using */
        freeaddrinfo(res);
        return -2;
    }

    /* bind to that socket */
    status = bind(sockfd, res->ai_addr, res->ai_addrlen);
    if (status != 0)
    {
        /* Error to handle */
        printf("Error 3 %s\n", strerror(errno));
        /* free the address info after using */
        freeaddrinfo(res);
        return -3;
    }

    return sockfd;
}
#if USE_AESD_CHAR_DEVICE

#else
static void write_clock_time(union sigval sv)
{
    time_t now;
    struct tm *tm_info;
    char timestamp[100];
    int file_fd;

    time(&now);
    tm_info = localtime(&now);
    int size = strftime(timestamp, sizeof(timestamp),
                        "timestamp:%a, %d %b %Y %H:%M:%S %z %n", tm_info);

    pthread_mutex_lock(&file_mutex);
    file_fd = open(FILE_NAME, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (file_fd == -1)
    {
        /* error */
        printf("Error can't open the file:  %s\n", strerror(errno));
        syslog(LOG_INFO, "Error opening or creating the file\n");
        /* release mutex */
        pthread_mutex_unlock(&file_mutex);
    }
    /* write to the file */
    if (write(file_fd, (void *)timestamp, size) == -1)
    {
        /* error */
        printf("Error couldn't write %s\n", strerror(errno));
        syslog(LOG_INFO, "Error wrting to the file: %s\n", FILE_NAME);
        if (close(file_fd) == -1)
            syslog(LOG_INFO, "Error closing the file: %s\n", FILE_NAME);
        /* release mutex */
        pthread_mutex_unlock(&file_mutex);
    }
    else
    {
        syslog(LOG_INFO, "Successfull wrting to the file: %s\n", FILE_NAME);
        if (close(file_fd) == -1)
            syslog(LOG_INFO, "Error closing the file: %s\n", FILE_NAME);
    }
    /* release mutex */
    pthread_mutex_unlock(&file_mutex);
}

void startTimer(int firstRun, int interval) // both arguments in seconds
{
    struct sigevent sev;
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = write_clock_time;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    sev.sigev_notify_attributes = &attr; // nullptr;
    sev.sigev_value.sival_ptr = NULL;
    timer_t timerId;
    timer_create(CLOCK_REALTIME, &sev, &timerId);

    struct itimerspec ts;
    ts.it_value.tv_sec = firstRun; // parametrize
    ts.it_value.tv_nsec = 0;

    ts.it_interval.tv_sec = interval; // parametrize
    ts.it_interval.tv_nsec = 0;
    timer_settime(timerId, 0, &ts, NULL);
}
#endif

/* socket main handler */
void *socket_main(void *node_addr)
{
    char recv_buf[MAX_BUFFER_LEN] = {'\0'};
    char send_buf[MAX_BUFFER_LEN] = {'\0'};
    int recv_len = 0;
    int send_len = 0;
    int file_fd;
    int status;
    int new_sockfd;
    pthread_t me;

    me = pthread_self();
    new_sockfd = ((node_t *)node_addr)->sockfd;
    printf("I am a new thread ID: %ld\n", me);
    /* start receving */
    recv_len = recv(new_sockfd, (void *)recv_buf, MAX_BUFFER_LEN - 1, 0);
    if (recv_len < 0)
    {
        /* Error to handle */
        printf("Error in reception %s\n", strerror(errno));
        syslog(LOG_INFO, "Exit program due to recv failure ID: %d\n", recv_len);
        ((node_t *)node_addr)->thrd_comp = true;
        return (void *)me;
    }
    else if (recv_len == 0)
    {
        /* no more reception */
        printf("stop %ld due to no more reception: %s\n", me, strerror(errno));
        syslog(LOG_INFO, "stop receiving data from client\n");
        ((node_t *)node_addr)->thrd_comp = true;
        return (void *)me;
    }
    else
    {
        /* recieved data */
        if (recv_buf[recv_len - 1] == '\n')
        {
            /* open file to recieve data */
            /* take mutex */
            pthread_mutex_lock(&file_mutex);
            file_fd = open(FILE_NAME, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (file_fd == -1)
            {
                /* error */
                printf("Error can't open the file:  %s\n", strerror(errno));
                syslog(LOG_INFO, "Error opening or creating the file\n");
                /* release mutex */
                pthread_mutex_unlock(&file_mutex);
                ((node_t *)node_addr)->thrd_comp = true;
                return (void *)me;
            }
            /* write to the file */
            status = write(file_fd, (void *)recv_buf, (recv_len * sizeof(char)));
            if (status == -1)
            {
                /* error */
                printf("Error couldn't write %s\n", strerror(errno));
                syslog(LOG_INFO, "Error wrting to the file: %s\n", FILE_NAME);
                if (close(file_fd) == -1)
                    syslog(LOG_INFO, "Error closing the file: %s\n", FILE_NAME);
                /* release mutex */
                pthread_mutex_unlock(&file_mutex);
                ((node_t *)node_addr)->thrd_comp = true;
                return (void *)me;
            }
            else
            {
                syslog(LOG_INFO, "Successfull wrting to the file: %s\n", FILE_NAME);
                if (close(file_fd) == -1)
                    syslog(LOG_INFO, "Error closing the file: %s\n", FILE_NAME);
            }
            /* release mutex */
            pthread_mutex_unlock(&file_mutex);

            /* clear buffer */
            memset(recv_buf, '\0', sizeof(recv_buf));

            /* open file to read data */
            /* take mutex */
            pthread_mutex_lock(&file_mutex);
            file_fd = open(FILE_NAME, O_RDONLY, 0644);
            if (file_fd == -1)
            {
                /* error */
                printf("Error 9 %s\n", strerror(errno));
                syslog(LOG_INFO, "Error opening or creating the file\n");
                /* release mutex */
                pthread_mutex_unlock(&file_mutex);
                ((node_t *)node_addr)->thrd_comp = true;
                return (void *)me;
            }
            #ifdef USE_AESD_CHAR_DEVICE
            send_len = 4096;
            while(read(file_fd, (void *)send_buf, send_len) > 0)
            {
                status = 0 ;
            }
            #else
            send_len = lseek(file_fd, 0, SEEK_END); // seek to end of file
            status = lseek(file_fd, 0, SEEK_SET);   // seek back to beginning of file
            status = read(file_fd, (void *)send_buf, send_len);
            #endif
            if (status == -1)
            {
                /* error */
                printf("Error 10 %s\n", strerror(errno));
                syslog(LOG_INFO, "Error reading file: %s\n", FILE_NAME);
                if (close(file_fd) == -1)
                    syslog(LOG_INFO, "Error closing the file: %s\n", FILE_NAME);
                /* release mutex */
                pthread_mutex_unlock(&file_mutex);
                ((node_t *)node_addr)->thrd_comp = true;
                return (void *)me;
            }
            if (close(file_fd) == -1)
                syslog(LOG_INFO, "Error closing the file: %s\n", FILE_NAME);
            /* release mutex */
            pthread_mutex_unlock(&file_mutex);

            /* write to client */
            status = send(new_sockfd, (void *)send_buf, send_len, 0); /*May be is not sending TODO:*/
            if (status == -1)
            {
                /* error */
                printf("Error 11 %s\n", strerror(errno));
                syslog(LOG_INFO, "Error sending back to socket\n");
                // if (close(file_fd) == -1)
                //     syslog(LOG_INFO, "Error closing the file: %s\n", FILE_NAME);
                ((node_t *)node_addr)->thrd_comp = true;
                return (void *)me;
            }
            /* clear buffer */
            memset(send_buf, '\0', sizeof(send_buf));
        }
        else
        {
            /* if data recieved doesn't have \n */
        }
    }
    printf("Exiting thread: %ld\n", me);
    ((node_t *)node_addr)->thrd_comp = true;
    return (void *)me;
}

int main(int argc, char *argv[])
{
    int status;
    int new_sockfd;
    head_t tasks_head;
    node_t *tmp_node;
    struct sockaddr *ai_addr;
    socklen_t ai_addrlen;

    /* Intialize tasks_head */
    TAILQ_INIT(&tasks_head);

    /* open log file */
    openlog(NULL, 0, LOG_INFO);

    /* Register a signal */
    status = signal_reg();
    if (status == -1)
    {
        syslog(LOG_INFO, "Can't handle SIGINT or SIGTERM");
        closelog();
        exit(1);
    }

    status = socket_setup();
    if (status == -1)
    {
        /* Error to handle */
        syslog(LOG_INFO, "Exit program due to getaddrinfo failure ID: %d\n", status);
        closelog();
        exit(1);
    }
    else if (status == -2)
    {
        /* Error to handle */
        syslog(LOG_INFO, "Exit program due to socket failure ID: %d\n", status);
        closelog();
        exit(1);
    }
    else if (status == -3)
    {
        syslog(LOG_INFO, "Exit program due to bind failure ID: %d\n", status);
        closelog();
        exit(1);
    }
    else
    {
        sockfd = status;
    }

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

    ai_addr = res->ai_addr;
    ai_addrlen = res->ai_addrlen;

#if USE_AESD_CHAR_DEVICE
#else
    startTimer(1, 10);
#endif

    /* accept incoming connection */
    while (1)
    {
        new_sockfd = accept(sockfd, ai_addr, &ai_addrlen);
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

        tmp_node = malloc(sizeof(struct node));
        if (tmp_node == NULL)
        {
            /* Error to handle */
            printf("Error im malloc %s\n", strerror(errno));
            syslog(LOG_INFO, "Error im malloc");
            closelog();
            exit(1);
        }
        tmp_node->thrd_id = 0;
        tmp_node->thrd_comp = false;
        tmp_node->sockfd = new_sockfd;
        TAILQ_INSERT_TAIL(&tasks_head, tmp_node, nodes_ptr);
        pthread_create(&TAILQ_LAST(&tasks_head, head_s)->thrd_id, NULL, socket_main, (void *)TAILQ_LAST(&tasks_head, head_s));

        /* joining mechanism */
        while (!TAILQ_EMPTY(&tasks_head))
        {
            tmp_node = TAILQ_FIRST(&tasks_head);
            if (tmp_node->thrd_comp)
            {
                pthread_join(tmp_node->thrd_id, NULL);
                TAILQ_REMOVE(&tasks_head, tmp_node, nodes_ptr);
                free(tmp_node);
                tmp_node = NULL;
            }
        }
    }
    closelog();
}
