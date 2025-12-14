#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <signal.h>

// TODO :
/*

*/

// --- DEFINITION DE CONSTANTES ----
#define MAX_NOMTOPIC 20 //taille max nom d'un topic
#define MAX_MSG 100 // taille max d'un message
#define MAX_NBTOPIC 10 // nombre max de topic possible
#define TOK_FILE "./.cle" //Fichier utilise pour le token
#define ID_PROJET 0 //ID du projet utilise pour le token
// ------------------

// ---- TO DO ----
// -variable de confirmation de la reception du broker


// ---- DEFINITION DE MACRO ----
#define CHECK(sts,msg) if ((sts) == -1 )  { perror(msg);exit(-1);}
//--------------


//STRUCTURE DU CODE DE PIERRE, ALLER VOIR SI DOUTE

// ----- DEFINITION DE STRUCTURE ----//
//Structure pour contenir un message
struct Message
{
    char topic[MAX_NOMTOPIC]; //topic sur lequel est publié le message
    char msg[MAX_MSG]; // message
    pid_t sender; // pid de celui qui envoie le message
    pid_t recepter; //pid de celui qui doit recevoir le message
};


//A Modifier ?
//struct listeTopic {
    //cjar
//}
struct listeTopic allTopic[MAX_NBTOPIC]; // tableau contenant tout les topics
// ---------------------------



// -- DEFINITION VARIABLE GLOBALE ---
int shmid; // id de la SHM
char * shmadd; // adresse de la SHM
sem_t *semMSG; //semaphore contenant message
//variable de confirmation de reception //TODO


// ---- DEFINITION DE SIGNAL HANDLER ----
void brokerHandler (int signb) {
    switch (signb)
    {
    case SIGINT: // Signal Fin de programme
        printf("SUBSCRIBER - confirmation recue du broker \n");
        //confirmation recu
        break;
    case SIGUSR1: // Signal => Publication d'un msg par Pub
       
        break;
    
    default:
        break;
    }
}
// ----------------------------------

// ---- MAIN -----
int main (int argc, char ** argv) {
    
    exit(EXIT_SUCCESS);
}

