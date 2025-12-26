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

// > Variable SHM PID
int shmid_PID; // id de la SHM PID
int * shmadd_PID; // adresse de la SHM PID

// > Variable de semaphore
sem_t * semMSG;

// > Variable pour la gestion des topics
struct listeTopic allTopic[MAX_NBTOPIC]; // tableau contenant tout les topics
int nbTopicCrees = 0; //Nb topci créer 
// ---------------------------


// ---- DEFINTION DE SIGNAL HANDLER ----
void brokerHandler (int signb) {
    struct Message  * msg = shmadd;
    int indexTopic = -1;
    switch (signb)
    {
    case SIGINT: // Signal Fin de programme
        printf("\nBORKER : Destruction SHM & semaphore\n");
        shmdt(shmadd);
        shmctl(shmid,IPC_RMID,NULL);
        shmdt(shmadd_PID);
        shmctl(shmid_PID,IPC_RMID,NULL);
        sem_close(semMSG);
        sem_unlink("/msg");
        printf("BROKER : Fin du Programme\n");
        exit(EXIT_SUCCESS);
        break;

    case SIGUSR1: // Signal => Publication d'un msg par Pub
        printf("BROKER : J'ai reçu un message\n");
        printf("\t> Attente de la disponibilite du semaphore pour lire msg\n");
        sem_wait(semMSG);

        printf("\t> Lecture des differentes informations du message\n");
        printf("BROKER : J'ai lu :\n\t-Topic : %s\n\t-Message : %s\n\t -Sender : %d\n\t-Recepter : %d\n",msg->topic , msg->msg, msg->sender, msg->recepter);
        printf("\t> Notifier bonne reception du messages\n");
        kill(msg->sender,SIGUSR1); // Notifier Sender de la bonne reception du message
        printf("\t> Gestion du topic\n");


        // Maybe a retravailler car bien quand 10 topic mais peu efficace avec 10000 topics
        for (int i=0; i<MAX_NBTOPIC;i++){
            if (!(strcmp(allTopic[i].topic,msg->topic))) {
                indexTopic = i;
            }
        }

        if (indexTopic>-1) {
            printf("\t> Le topic existe, publication du message\n");
            for (int j = 0; j<allTopic[indexTopic].nb_sub;j++) {
                msg->sender = getpid(); // on definit le broker comme celui qui envoie le message
                msg->recepter = allTopic[indexTopic].sub[j]; // on définit a qui on envoie le message
                kill(allTopic[indexTopic].sub[j], SIGUSR2);// signal pour dire publication
            }

        }else{
            printf("\t> Le Topic n'existe pas, creation\n");
            if (nbTopicCrees+1 > MAX_NBTOPIC){
                printf("BROKER : ERREUR : Impossible de créer un Topic en plus\n");
            }else{
                strcpy(allTopic[nbTopicCrees].topic,msg->topic);
                nbTopicCrees++;
            }
        }
        sem_post(semMSG); //Mise en place d'un jeton pour que les processus puissent lire
        break;

    case SIGUSR2:
        printf("BROKER : J'ai recu une demande de sub\n");
        printf("\t> Attente de la liberation du semaphore pour traiter la demande\n");
        sem_wait(semMSG);
        printf("BROKER : J'ai reçu une demande de sub au topic %s de la part de %d\n",msg->topic, msg->sender);
        printf("\t> Envoi de l'accuse de reception\n");
        //kill(msg->sender, SIGUSR2);
        // Maybe a retravailler car bien quand 10 topic mais peu efficace avec 10000 topics
        for (int i=0; i<nbTopicCrees;i++){
            if (!(strcmp(allTopic[i].topic,msg->topic))) {
                indexTopic = i;
            }
        }


        if (indexTopic> -1) {
            if (allTopic[indexTopic].nb_sub+1 < MAX_SUB) {
                printf("\t> Ajout du sub\n");
                allTopic[indexTopic].sub[allTopic[indexTopic].nb_sub] = msg->sender;
                allTopic[indexTopic].nb_sub++;
            }else{
                printf("BROKER : Impossible de sub a ce topic, le nombre de sub max est atteint\n");
                kill(msg->sender,SIGUSR1);
            }
        }else{
            printf("\t> Le Topic n'existe pas, creation\n");
            if (nbTopicCrees+1 > MAX_NBTOPIC){
                printf("BROKER : Impossible de créer un Topic en plus\n");
                kill(msg->sender,SIGUSR1);
            }else{
                strcpy(allTopic[nbTopicCrees].topic,msg->topic);
                allTopic[nbTopicCrees].sub[allTopic[nbTopicCrees].nb_sub] = msg->sender;
                allTopic[nbTopicCrees].nb_sub++;
                nbTopicCrees++;
            }
        }

        sem_post(semMSG);
        break;
    
    default:
        break;
    }
    printf("BROKER : La demande a ete traitee\n");
    printf("BROKER : Attente d'une nouvelle demande\n\n\n");
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
    CHECK(sigaction(SIGUSR1,&mask,NULL),"BROKER : Erreur lors du SIGACTION pour SIGUSR1\n");
    CHECK(sigaction(SIGUSR2,&mask,NULL),"BROKER : Erreur lors du SIGACTION pour SIGUSR2\n");

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

    // * ------ Creation SHM pour contenir le PID * ------- //
    printf("BROKER : Creation de la SHM contenant mon PID\n");
    printf("\t> Creation de la cle\n");
    key_t tok_PID = ftok(TOK_FILE,ID_PROJET+1);
    CHECK(tok_PID,"BROKER : Erreur creation cle pour la SHM\n");
    printf("\t> Creation de l'id de la SHM\n");
    shmid_PID = shmget(tok_PID,sizeof(int), 0666 | IPC_CREAT);
    CHECK(shmid_PID,"BROKER : Erreur lors de l'attribution d'id pour la SHM\n");
    printf("\t> Allocation de memoire a la SHM\n");
    shmadd_PID = shmat(shmid_PID,NULL,0);
    CHECK(shmadd_PID,"BROKER : Erreur lros de l'allocation memoire de la SHM\n");
    printf("\t> Ecriture de mon PID dans la SHM\n");
    *shmadd_PID = getpid(); // ecrire valeur point
    printf("BROKER : Fin creation SHM_PID\n");
    // -------------------------------

    // * --- Main Code * -------- //
    sem_post(semMSG); // placer jeton dans le semaphore pour le rendre accessible
    printf("BROKER : Configuration Finie - Attente de publication de message\n");
    while (1) {
    }
    // --------------------------
    exit(EXIT_SUCCESS);
}
// ------- //