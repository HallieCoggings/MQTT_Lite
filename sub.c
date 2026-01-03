/*******************
 * Titre du fichier : sub.c
 * Date : Decembre 2025
 * Version : 1.0
 * Description :
 * Ce fichier contient le programme d'un abonne a un topic dans la version simplifie de MQTT
 * 
 */
// Importation des libraires
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
//Macro pour tester qu'il n'y a eu aucune erreur lors de differentes actions liees au systeme
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
    int action; //gestion abonnement(0), desinscription(1) et publication(-1) dans un topic
};

// -----------------------------------

// -- DEFINITION VARIABLE GLOBALE ---
// > Variable de SHM
int shmid; // id de la SHM
struct Message * shmadd; // adresse de la SHM

// > Variable SHM PID
int shmid_PID; // id de la SHM PID du broker
int * shmadd_PID; // adresse de la SHM PID du broker

// > Variable de semaphore
sem_t * semMSG;

// > variable lie au broker
pid_t broker_pid;
int isSub = 0; // est-ce que le processus est abonne
int subFlag = 0; // est-ce que le processus peut s'abonner

// > variable pour sauvegarder le topic
char topic_save[MAX_NOMTOPIC]; // sauvegarde du topic pour la desinscription

// ---------------------------

// --- DEFINITION SIGHANDLER --- //
void subHandler (int signb) {
    struct Message * msg = shmadd;
    switch (signb)
    {
    case SIGINT: // Signal de fin de programme
        printf("\nSUB : Arret du programme\n");
        
        if(isSub){
            printf("SUB : Desinscription du topic %s\n", topic_save);

            sem_wait(semMSG);
            struct Message *msg = shmadd;
            strcpy(msg->topic, topic_save);
            msg->sender = getpid();
            msg->recepter = broker_pid;
            msg->action = 1; //desinscription
            sem_post(semMSG);

            kill(broker_pid, SIGUSR2);
            sleep(1);
        }
        
        // * ----------- Destruction des liens ------------- * //
        printf("SUB : Suppression liens SHM & Semaphore\n");
        shmdt(shmadd);
        shmdt(shmadd_PID);
        sem_close(semMSG);
        printf("SUB : Fin d'execution\n");
        exit(EXIT_SUCCESS);
        break;

    case SIGUSR1: // Signal pour dire que l'abonnement est impossible
        printf("SUB : Impossible de s'abonner (trop de sub a ce topic ou impossible de creer nouveau topic)\n");
        subFlag = 2;
        break;

    case SIGUSR2: // Signal pour dire qu'un message est present
        printf("SUB : Reception d'un message (topic : %s) \n",msg->topic);
        printf("\t> Attente du semaphore\n");
        sem_wait(semMSG);
        printf("\t> Traitement de la reception\n");
        printf("SUB : Message : %s\n", msg->msg);
        printf("SUB : J'ai fini de traiter la demande\n");
        sem_post(semMSG);
        break;

    case SIGHUP:
        subFlag = 1;
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
    CHECK(sigaction(SIGUSR1,&mask,NULL),"SUB : Erreur lors du SIGACTION pour SIGUSR1\n");
    CHECK(sigaction(SIGUSR2,&mask,NULL),"SUB : Erreur lors du SIGACTION pour SIGUSR2\n");
    CHECK(sigaction(SIGINT,&mask,NULL),"SUB : Erreur lors du SIGACTION pour SIGINT\n");
    CHECK(sigaction(SIGHUP,&mask,NULL),"SUB : Erreur lors du SIGACTION pour SIGHUP\n");

    // ----------------------------------

    //* --- CREATION SEMAPHORE POUR SHM --- * //
    // Semphore pour acceder a la SHM des messages
    printf("SUB : Ouverture du semaphore pour la SHM\n");
    semMSG = sem_open("/msg",0);
    if (semMSG == SEM_FAILED) { perror("SUB : Erreur lors de l'ouverture du semaphore\n"); exit(-1); }
    printf("SUB : Fin Ouverture Semaphore\n");
    // --------------------------------------

    // * ---- ACCES SHM --------- * //
    // Lien avec la SHM pour les messages
    printf("SUB : Acces a la SHM\n");
    printf("\t> Creation de la cle\n");
    key_t tok = ftok(TOK_FILE,ID_PROJET);
    CHECK(tok,"SUB : Erreur creation cle pour la SHM\n");
    printf("\t> Recuperation de l'id de la SHM\n");
    shmid = shmget(tok,sizeof(struct Message), 0666);
    CHECK(shmid,"SUB : Erreur lors de la recuperation d'id pour la SHM\n");
    printf("\t> Obtenir Mem addr de la SHM\n");
    shmadd = shmat(shmid, NULL, 0);
    if (shmadd == (void*)-1) { perror("SUB : Erreur lors de l'obtention de l'adresse Memoire de la SHM\n"); exit(-1); }
    printf("SUB : Fin acces a la SHM\n");
    // ------------------------

    // * ----------- MAIN ------------- *//

    // * ------ OBTENIR PID DU BROKER ------ * //
    printf("SUB : Recuperation du PID du Broker\n");
    printf("\t> Generation de la cle pour la SHM_PID\n");
    key_t tok_PID = ftok(TOK_FILE,ID_PROJET+1);
    CHECK(tok_PID,"SUB : Erreur creation cle pour la SHM\n");
    printf("\t> Recuperation de l'id de la SHM\n");
    shmid_PID = shmget(tok_PID,sizeof(int), 0666);
    CHECK(shmid_PID,"SUB : Erreur lors de la recuperation d'id pour la SHM\n");
    printf("\t> Obtenir Mem Addr de la SHM\n");
    shmadd_PID = shmat(shmid_PID,NULL,0);
    if (shmadd_PID == (void*)-1) { perror("SUB : Erreur lors de l'obtention de l'adresse Memoire de la SHM\n"); exit(-1); }
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
            msg->action = 0; // abonnement
            sem_post(semMSG);
            kill(broker_pid,SIGUSR2);
            printf("\t> Attente de la confirmation d'abonnement\n");
            while(subFlag ==0) {
                //attente signal
            }

            if (subFlag == 1) {
                printf("SUB : Je suis sub à %s\n",topic);
                strcpy(topic_save, topic); // sauvegarde du topic
                isSub = 1;
            }

            if (subFlag == 2) {
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
