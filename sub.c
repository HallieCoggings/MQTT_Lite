#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <signal.h>
#include <string.h>

// --- DEFINITION DE CONSTANTES ----
#define MAX_NOMTOPIC 20 //taille max nom d'un topic
#define MAX_MSG 100 // taille max d'un message
#define MAX_NBTOPIC 10 // nombre max de topic possible
#define TOK_FILE "./.cle" //Fichier utilise pour le token
#define ID_PROJET 0 //ID du projet utilise pour le token
#define MAX_SUB 20 // Nombre max de sub a un topic
// ------------------


// ---- DEFINITION DE MACRO ----
#define CHECK(sts,msg) if ((sts) == -1 )  { perror(msg);exit(-1);}
//--------------


// ----- DEFINITION DE STRUCTURE ----//
//Structure pour contenir un message
struct Message
{
    char topic[MAX_NOMTOPIC]; //topic sur lequel est publié le message
    char msg[MAX_MSG]; // message
    pid_t sender; // pid de celui qui envoie le message
    pid_t recepter; //pid de celui qui doit recevoir le message
};

//Structure pour gerer la liste de diffusion sur un topic
struct listeTopic
{
    char topic[MAX_NOMTOPIC]; // nom du topic
    pid_t sub[MAX_SUB]; // nombre de sub
    int nb_sub; // nombre de sub au topic
};

// -----------------------------------

// -- DEFINITION VARIABLE GLOBALE ---
// > Variable de SHM
int shmid; // id de la SHM
struct Message * shmadd; // adresse de la SHM

// > Variable de semaphore
sem_t * semMSG;

// > variable lie au broker
pid_t broker_pid;

// ---------------------------

// --- DEFINITION SIGHANDLER --- //
void subHandler (int signb) {
    struct Message * msg = shmadd;
    switch (signb)
    {
    case SIGINT:
        printf("\nSUB : Arret du programme\n");
        // * ----------- Destruction des liens ------------- * //
        printf("SUB : Suppression liens SHM & Semaphore\n");
        shmdt(shmadd);
        sem_close(semMSG);
        printf("SUB : Fin d'execution\n");
        exit(EXIT_SUCCESS);
        break;

    case SIGUSR2:
        printf("SUB : Reception d'un message \n");
        printf("\t> Attente du semaphore\n");
        sem_wait(semMSG);
        printf("\t> Traitement de la reception\n");
        printf("SUB : Mesage : %s\n", msg->msg);
        printf("SUB : J'ai fini de traiter la demande");
        sem_post(semMSG);
        break;
    
    default:
        break;
    }
}
// ---------------------------------

// ---- MAIN -----
int main (int argc, char ** argv) {

    // * --- CREATION SIGHANDLER ----- * //
    struct sigaction mask;
    sigset_t old;
    void subHandler();
    mask.sa_flags = 0;
    mask.sa_handler = subHandler;
    CHECK(sigemptyset(&mask.sa_mask),"SUB : Erreur lors du SIGEMPTY\n");
    CHECK(sigprocmask(SIG_SETMASK,&mask.sa_mask,&old),"SUB : Erreur lors du SIGPROC\n");
    CHECK(sigaction(SIGUSR2,&mask,NULL),"SUB : Erreur lors du SIGACTION pour SIGUSR2\n");
    CHECK(sigaction(SIGINT,&mask,NULL),"SUB : Erreur lors du SIGACTION pour SIGINT");
    // ----------------------------------

    //* --- CREATION SEMAPHORE POUR SHM ---* //
    printf("SUB : Ouverture du semaphore pour la SHM\n");
    semMSG = sem_open("/msg",0);
    CHECK(semMSG,"SUB : Erreur lors de l'ouverture du sempahore\n");
    printf("SUB : Fin Ouverture Semaphore\n");
    // --------------------------------------

    // * ---- ACCES SHM ---------* //
    printf("SUB : Acces a la SHM\n");
    printf("\t> Creation de la cle\n");
    key_t tok = ftok(TOK_FILE,ID_PROJET);
    CHECK(tok,"SUB : Erreur creation cle pour la SHM\n");
    printf("\t> Recuperation de l'id de la SHM\n");
    shmid = shmget(tok,sizeof(struct Message), 0666);
    CHECK(shmid,"SUB : Erreur lors de la recuperation d'id pour la SHM\n");
    printf("\t> Obtenir Mem addr de la SHM\n");
    shmadd = shmat(shmid, NULL, 0);
    CHECK(shmadd,"SUB : Erreur lors de l'obtention de l'addresse Memoire de la SHM\n");
    printf("SUB : Fin acces a la SHM\n");
    // ------------------------

    // * ----------- MAIN -------------*

    // Obtention du Broker
    char buf_tmp[64];
    printf("SUB : Entrez le PID du broker : "); //recup PID broker pour envoi signaux
    fgets(buf_tmp,64,stdin);
    broker_pid = atoi(buf_tmp);
    //verification de l'existence du broker
    CHECK(kill(broker_pid, 0), "PUB : Le broker n'existe pas\n"); 
    printf("SUB : Broker trouve (PID: %d)\n", broker_pid);
    // ---------
    char topic[MAX_NOMTOPIC];


    printf("SUB : Nouvelle demande d'abonnement\n");
    printf("SUB : Entrez le nom du topic :");
    fgets(topic,MAX_NOMTOPIC,stdin);
    topic[strcspn(topic, "\n")] = '\0';

    if (strcmp(topic,"quit")!=0){
        printf("SUB : Traitement de la demande d'abo\n");
        printf("\t> Attente Semaphore\n");
        sem_wait(semMSG);
        printf("\t> Ecriture dans la SHM\n");
        struct Message * msg = shmadd;
        strcpy(msg->topic,topic);
        msg->sender = getpid();
        msg->recepter = broker_pid;
        kill(broker_pid,SIGUSR2);
        sem_post(semMSG);
        printf("SUB : Je passe en mode Reception de MSG\n");
        while(1){
            // Boucle d'attente des messages et signaux
        }
    }else{
        raise(SIGINT); // Envoie d'un signal SIGINT sur soi meme
    }
    
    // ---------------------------------
}

