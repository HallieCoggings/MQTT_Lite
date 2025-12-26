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
    char topic[MAX_NOMTOPIC]; //topic sur lequel est SUBlié le message
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

// > Variable SHM PID
int shmid_PID; // id de la SHM PID
int * shmadd_PID; // adresse de la SHM PID

// > Variable de semaphore
sem_t * semMSG;

// > variable lie au broker
pid_t broker_pid;
int isSub = 0;
int subFlag = 0;

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
        shmdt(shmadd_PID);
        sem_close(semMSG);
        printf("SUB : Fin d'execution\n");
        exit(EXIT_SUCCESS);
        break;

    case SIGUSR1:
        printf("SUB : Impossible de s'abonner (trop de sub a ce topic ou impossible de creer nouveau topic)\n");
        subFlag = 2;
        break;

    case SIGUSR2:
        printf("SUB : Reception d'un message \n");
        printf("\t> Attente du semaphore\n");
        sem_wait(semMSG);
        printf("\t> Traitement de la reception\n");
        printf("SUB : Mesage : %s\n", msg->msg);
        printf("SUB : J'ai fini de traiter la demande");
        subFlag = 1;
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
    CHECK(sigaction(SIGUSR1,&mask,NULL),"SUB : Erreur lors du SIGACTION pour SIGUSR2\n");
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
    /*
    V1 Obtention du BROKER
    char buf_tmp[64];
    printf("SUB : Entrez le PID du broker : "); //recup PID broker pour envoi signaux
    fgets(buf_tmp,64,stdin);
    broker_pid = atoi(buf_tmp);
    //verification de l'existence du broker
    CHECK(kill(broker_pid, 0), "SUB : Le broker n'existe pas\n"); 
    printf("SUB : Broker trouve (PID: %d)\n", broker_pid);
    */
    printf("SUB : Recuperation du PID du Broker\n");
    printf("\t> Generation de la cle pour la SHM_PID\n");
    key_t tok_PID = ftok(TOK_FILE,ID_PROJET+1);
    CHECK(tok_PID,"SUB : Erreur creation cle pour la SHM\n");
    printf("\t> Recuperation de l'id de la SHM\n");
    shmid_PID = shmget(tok_PID,sizeof(int), 0666);
    CHECK(shmid_PID,"SUB : Erreur lors de la recuperation d'id pour la SHM\n");
    printf("\t> Obtenir Mem Addr de la SHM\n");
    shmadd_PID = shmat(shmid_PID,NULL,0);
    CHECK(shmadd_PID,"SUB :  Erreur lors de l'obtention de l'addresse Memoire de la SHM\n");
    printf("\t> Sauvegarde du PID\n");
    broker_pid = *shmadd_PID;
    printf("SUB : PID du Broker trouvé (%d)\n",broker_pid);
    // ---------
    char topic[MAX_NOMTOPIC];

    while (!isSub){
        subFlag = 0;
        printf("SUB : Nouvelle demande d'abonnement\n");
        printf("SUB : Entrez le nom du topic (quit pour quitter):");
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
            while(subFlag ==0) {
                //attente signal
            }

            if (subFlag == 1) {
                printf("SUB : Je suis sub à %s",topic);
            }

            if (subFlag == 0) {
                printf("SUB : La demande a echoue\n");
            }
        }else{
            raise(SIGINT); // Envoie d'un signal SIGINT sur soi meme
        }
    }

    printf("SUB : Je passe en mode Reception de MSG\n");
    while(1){
        //attente de message
    }

    // ---------------------------------
}

