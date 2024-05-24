#include "sys/types.h"
#include "unistd.h"
#include "stdlib.h"
#include "signal.h"
#include "stdio.h"
#include "string.h"
#include "time.h"
#include "wait.h"
#include "sys/ipc.h"
#include "sys/msg.h"
#include "sys/shm.h"
#include "sys/sem.h"
#include "sys/stat.h"

struct Dokumentum
{
    char cim[14];
    char hnev[14];
    int ev;
    char tnev[16];
};

struct Message
{
    long mtype;
    char mtext[1024];
};

struct sharedData
{
    char text[1024];
};

pid_t mainProcessValue = 0;
int ready = 0;
int messageQueue;
int semid;
struct sharedData *s;

int semaphoreCreation(const char *pathname, int semaphoreValue)
{
    int semid;
    key_t key;

    key = ftok(pathname, 1);
    if ((semid = semget(key, 1, IPC_CREAT | S_IRUSR | S_IWUSR)) < 0)
        perror("semget");
    if (semctl(semid, 0, SETVAL, semaphoreValue) < 0)
        perror("semctl");

    return semid;
}

void semaphoreOperation(int semid, int op)
{
    struct sembuf operation;

    operation.sem_num = 0;
    operation.sem_op = op;
    operation.sem_flg = 0;

    if (semop(semid, &operation, 1) < 0)
        perror("semop");
}

void semaphoreDelete(int semid)
{
    semctl(semid, 0, IPC_RMID);
}

void readyHandler(int sig)
{
    if (sig == SIGUSR1)
    {
        ready++;
    }
}

void starthandler(int sig)
{
    if (sig == SIGUSR1)
    {
        ready++;
    }
}

pid_t hallgato(int pipe_id_rec, int pipe_id_send)
{
    pid_t process = fork();
    if (process == -1)
    {
        exit(-1);
    }
    if (process > 0)
    {
        return process;
    }

    kill(mainProcessValue, SIGUSR1);

    struct Dokumentum dok;
    dok.ev = 2024;
    sprintf(dok.cim, "%s", "Dolgozat cime");
    sprintf(dok.hnev, "%s", "Hallgato neve");
    sprintf(dok.tnev, "%s", "Temavezeto neve");

    write(pipe_id_send, &dok, sizeof(struct Dokumentum));

    char kerdes[64];
    struct Message msg;
    msgrcv(messageQueue, &msg, sizeof(struct Message), 5, 0);
    strcpy(kerdes, msg.mtext);
    printf("%s\n", kerdes);

    semaphoreOperation(semid, -1);
    printf("A temavezeto kozlemenye: %s\n", s->text);
    semaphoreOperation(semid, 1);
    shmdt(s);

    exit(0);
}

pid_t temavezeto(int pipe_id_rec, int pipe_id_send)
{
    pid_t process = fork();
    if (process == -1)
    {
        exit(-1);
    }
    if (process > 0)
    {
        return process;
    }

    kill(mainProcessValue, SIGUSR1);

    struct Dokumentum dok;
    read(pipe_id_rec, &dok, sizeof(struct Dokumentum));
    printf("A dolgozat cime: %s\n", dok.cim);
    printf("A hallgato neve: %s\n", dok.hnev);
    printf("A beadas eve: %d\n", dok.ev);
    printf("A temavezeto neve: %s\n", dok.tnev);

    struct Message msg;
    msg.mtype = 5;
    sprintf(msg.mtext, "%s", "Milyen technológiával szeretné a feladatát megvalósítani?");

    msgsnd(messageQueue, &msg, sizeof(struct Message), 0);

    char newData[50];
    srand(getpid());

    int chance = rand() % 100;
    int success = (chance > 20);
    if (success != 0)
    {
        strcpy(newData, "Elfogadom.");
    }
    else
    {
        strcpy(newData, "Nem fogadom el.");
    }

    semaphoreOperation(semid, -1);
    strcpy(s->text, newData);
    semaphoreOperation(semid, 1);
    shmdt(s);

    exit(0);
}

int main(int argc, char **argv)
{

    mainProcessValue = getpid();
    signal(SIGUSR1, starthandler);

    int status;
    key_t mainKey;

    mainKey = ftok(argv[0], 1);
    messageQueue = msgget(mainKey, 0600 | IPC_CREAT);
    if (messageQueue < 0)
    {
        perror("msgget");
        return -1;
    }

    int sh_mem_id;
    sh_mem_id = shmget(mainKey, sizeof(s), IPC_CREAT | S_IRUSR | S_IWUSR);
    s = shmat(sh_mem_id, NULL, 0);

    semid = semaphoreCreation(argv[0], 1);

    int io_pipes[2];
    int succ = pipe(io_pipes);

    if (succ == -1)
    {
        exit(-1);
    }

    int io_pipes1[2];
    int succ1 = pipe(io_pipes1);

    if (succ1 == -1)
    {
        exit(-1);
    }

    int io_pipes2[2];
    int succ2 = pipe(io_pipes2);

    if (succ2 == -1)
    {
        exit(-1);
    }

    int io_pipes3[2];
    int succ3 = pipe(io_pipes3);

    if (succ3 == -1)
    {
        exit(-1);
    }

    pid_t child1_pid = hallgato(io_pipes[0], io_pipes1[1]);
    pid_t child2_pid = temavezeto(io_pipes2[0], io_pipes3[1]);

    while (ready < 1)
        ;
    puts("A hallgato bejelentkezett!");
    while (ready < 2)
        ;
    puts("A temavezeto bejelentkezett!");

    struct Dokumentum doc;
    read(io_pipes1[0], &doc, sizeof(struct Dokumentum));
    write(io_pipes2[1], &doc, sizeof(struct Dokumentum));

    waitpid(child1_pid, &status, 0);
    waitpid(child2_pid, &status, 0);
    printf("Hallgato - terminated with status: %d\n", status);
    printf("Temavezeto - terminated with status: %d\n", status);

    close(io_pipes[0]);
    close(io_pipes[1]);
    close(io_pipes1[0]);
    close(io_pipes1[1]);
    close(io_pipes2[0]);
    close(io_pipes2[1]);
    close(io_pipes3[0]);
    close(io_pipes3[1]);

    status = msgctl(messageQueue, IPC_RMID, NULL);
    if (status < 0)
    {
        perror("msgctl");
    }

    return 0;
}