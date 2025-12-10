#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/shm.h>
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



int main (int argc, char ** argv) {
    printf("PUB : Init\n");

    //* Creation des semaphores et SHM *//
    printf("PUB : Initilisation Semaphore & SHM\n");
    //Lien avec les semaphores nommés
    sem_t *semTopicCreation = sem_open("/topicCreation",O_CREAT,0666,0);
    CHECK(semTopicCreation,"PUB : Erreur de creation du semaphore pour les topics\n");
    sem_t *semMessage = sem_open("/message",O_CREAT,0666,0);
    CHECK(semMessage,"PUB : Erreur de creation du semaphore pour les messages\n");
    //Association SHM Topic
    key_t tok = ftok(TOK_FILE,PRJ_ID);
    CHECK(tok,"PUB : Erreur de creation la cle\n");
    int shmid = shmget(tok,sizeof(struct Topic),0666 | IPC_CREAT);
    CHECK(shmid,"PUB : Erreyr lors de la creation de la SHM topic");
    char * shmadd = shmat(shmid,NULL,0);
    CHECK(shmadd,"PUB : Erreur dans la creation de la SHM pour les topics");

    //Association SHM message
    key_t tok_m = ftok(TOK_FILE,PRJ_ID+1);
    CHECK(tok_m,"PUB : Erreur de creation la cle\n");
    int shmid_m = shmget(tok_m,sizeof(struct Message),0666 | IPC_CREAT);
    CHECK(shmid_m,"PUB : Erreyr lors de la creation de la SHM topic");
    char * shmadd_m = shmat(shmid_m,NULL,0);
    CHECK(shmadd_m,"PUB : Erreur dans la creation de la SHM pour les topics");


    struct Topic * topicCopy = shmadd;
    struct Message * message = shmadd_m;

    //* Communication du topic name et message *//
    printf("PUB : Entre le nom du topic sur lequel communiquer : ");
    char topic[MAX_TOPICNAME];
    fgets(topic,MAX_TOPICNAME,stdin);
    topic[strlen(topic)-1] = '\0';
    printf("PUB : Entre votre message : ");
    char msg[MAX_MSG];
    fgets(msg,MAX_MSG,stdin);
    msg[strlen(msg)-1] = '\0';

    printf("PUB : Je souhaite publié le message \"%s\" sur le topic \"%s\" \n",msg,topic);

    printf("PUB : Je suis en attende des sémaphore\n");
    sem_wait(semTopicCreation);
    sem_wait(semMessage);
    printf("PUB : J'ai obtenu les ressources\n");
    strcpy(topicCopy->topic,topic);
    strcpy(message->msg,msg);
    printf("%d\n",message->broker_pid);
    sem_post(semTopicCreation);
    sem_post(semMessage);
    printf("PUB : J'ai fini d'ecrire les informations\n");

    exit(EXIT_SUCCESS);
}