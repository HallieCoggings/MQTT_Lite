#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <signal.h>

// TODO :
/*
    - Signal handler
    - Gestion de plusieurs Topic
*/

// --- DEFINTION DE CONSTANTES ----
#define MAX_NOMTOPIC 20 //taille max nom d'un topic
#define MAX_MSG 100 // taillem ax d'un message
#define MAX_NBTOPIC 10 // nombre max de topic possible
#define TOK_FILE "./.cle" //Fichier utilise pour le token
#define ID_PROJET 0 //ID du projet utilise pour le token
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
// -----------------------------------

// -- DEFINITION VARIABLE GLOBALE ---
// > Variable de SHM
int shmid; // id de la SHM
char * shmadd; // adresse de la SHM

// > Variable de semaphore
sem_t * semMSG;
// ---------------------------


// ---- DEFINTION DE SIGNAL HANDLER ----
void brokerHandler (int signb) {
    switch (signb)
    {
    case SIGINT:
        printf("BORKER : Destruction SHM & sémahore\n");
        shmdt(shmadd);
        shmctl(shmid,IPC_RMID,NULL);
        sem_close(semMSG);
        sem_unlink("/msg");
        printf("BROKER : Fin du Programme\n");
        exit(EXIT_SUCCESS);
        break;
    
    default:
        break;
    }
}
// ----------------------------------

// ---- MAIN -----
int main (int argc, char ** argv) {
    printf("BROKER (%d): Init\n", getpid());

    // * -- CREATION DU SIGNAL HANDLER * ---
    struct sigaction mask;
    sigset_t old;
    void brokerHandler();
    mask.sa_flags = 0;
    mask.sa_handler = brokerHandler;
    CHECK(sigemptyset(&mask.sa_mask),"BROKER : Erreur lors du SIGEMPTY\n");

    CHECK(sigprocmask(SIG_SETMASK,&mask.sa_mask,&old),"BROKER : Erreur lors du SIGPROC");
    CHECK(sigaction(SIGINT,&mask,NULL),"BROKER : Erreur lors du SIGACTION pour SIGINT\n");
    // -------------------------------------

    // * ---------- CREATION SEMAPHORE POUR SHM ------------- * //
    printf("BROKER :  Creation du semaphore pour la SHM\n");
    semMSG = sem_open("/msg",O_CREAT,0666,0);
    CHECK(semMSG,"BROKER : Erreur lors de l'ouverture du semaphore\n");
    printf("BROKER : Fin Creation du semaphore\n");
    // ----------------------------------------


    // * ----- CREATION SHM * ---- //
    printf("BROKER : Creation de la SHM pour les messages\n");
    printf("\t> Creation de la cle\n");
    key_t tok = ftok(TOK_FILE,ID_PROJET);
    CHECK(tok,"BROKER : Erreur creation cle pour la SHM\n");
    printf("\t> Creation de l'id de la SHM\n");
    shmid = shmget(tok,sizeof(struct Message), 0666 | IPC_CREAT);
    CHECK(shmid,"BROKER : Erreur lors de l'attribution d'id pour la SHM\n");
    printf("\t> Allocation de memoire a la SHM\n");
    shmadd = shmat(shmid,NULL,0);
    CHECK(shmadd,"BROKER : Erreur lros de l'allocation memoire de la SHM\n");
    printf("BROKER : Fin creation SHM\n");
    // ------------------------------------

    // * --- Main Code * -------- //
    sem_post(semMSG); // placer jeton dans le semaphore pour le rendre accessible
    printf("BROKER : Configuration Finie - Attente de publication de message\n");
    while (1) {
    }
    // --------------------------
    exit(EXIT_SUCCESS);
}
// ------- //