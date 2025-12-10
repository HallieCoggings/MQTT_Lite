#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>
#include <signal.h>

#define PRJ_ID 2
#define TOK_FILE "./.cle"
#define MAX_TOPICNAME 50
#define MAX_MSG 100

#define CHECK(sts,msg) if ((sts) == -1 )  { perror(msg);exit(-1);}

struct Topic
{
    char topic[MAX_TOPICNAME];
    pid_t broker_pid;
};

struct Message
{
    char msg[MAX_MSG];
    pid_t broker_pid;
};

struct Topic * topic;
struct Message * message;

int main (int argc , char ** argv) {
    printf("BROKER (%d) : Initialisation\n",getpid());

    //* Creation des semaphores et SHM *//
    printf("BROKER : Creation du semaphore Topic\n");
    sem_t *semTopicCreation = sem_open("/topicCreation",O_CREAT,0666,0);
    sem_t *semMessage = sem_open("/message",O_CREAT,0666,0);
    printf("BROKER : Fin creation semaphore nomme\n");


    // SHM topic
    printf("BROKER : Creation de la SHM pour le topic\n");
    key_t tok = ftok(TOK_FILE,PRJ_ID);
    CHECK(tok,"BROKER : Erreur de creation la cle\n");
    int shmid = shmget(tok,sizeof(struct Topic),0666 | IPC_CREAT);
    CHECK(shmid,"BROKER : Erreur lors de la creation de la SHM topic");
    char * shmadd = shmat(shmid,NULL,0);
    CHECK(shmadd,"BROKER : Erreur dans la creation de la SHM pour les topics");

    //SHM Message
    printf("BROKER : Creation de la SHM pour le message\n");
    key_t tok_m = ftok(TOK_FILE,PRJ_ID+1);
    CHECK(tok_m,"BROKER : Erreur de creation la cle\n");
    int shmid_m = shmget(tok_m,sizeof(struct Message),0666 | IPC_CREAT);
    CHECK(shmid_m,"BROKER : Erreyr lors de la creation de la SHM message");
    char * shmadd_m = shmat(shmid_m,NULL,0);
    CHECK(shmadd_m,"BROKER : Erreur dans la creation de la SHM message");

    //* Creation du sighandler *//
    printf("BROKER : Creation de la gestion des signaux\n");

    topic = shmadd;
    message = shmadd_m;
    topic->broker_pid = getpid();
    message->broker_pid = getpid();

    //Rendre les SHMs disponibles
    sem_post(semTopicCreation);
    sem_post(semMessage);

    //* Destruction des semaphores et SHM *//
    printf("BROKER : Destruction Semaphore et SHM\n");

    shmdt(shmadd);
    shmctl(shmid,IPC_RMID,NULL);
    shmdt(shmadd_m);
    shmctl(shmid_m,IPC_RMID,NULL);

    sem_close(semTopicCreation);
    sem_unlink("/topicCreation");
    sem_close(semMessage);
    sem_unlink("/message");

    exit(EXIT_SUCCESS);
}