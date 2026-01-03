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
// ------------------

// ---- DEFINITION DE MACRO ----
#define CHECK(sts,msg) if ((sts) == -1) { perror(msg); exit(-1); }
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

// -- DEFINITION VARIABLE GLOBALE ---
// > Variable de SHM
int shmid; // id de la SHM
char * shmadd; // adresse de la SHM

// > Variable SHM PID
int shmid_PID; // id de la SHM PID
int * shmadd_PID; // adresse de la SHM PID

// > Variable de la semaphore 
sem_t *semMSG;

// > Varibale du broker et de la confirmation de reception
pid_t broker_pid;
int confirmation = 0;
// ----------------------------------

// ---- DEFINTION DE SIGNAL HANDLER ----
void publisherHandler(int signb) {
    if (signb == SIGUSR1) { // publication d'un message par le pub
        printf("PUB : Confirmation recue du broker\n");
        confirmation = 1; //confirmation de la reception
    } else if (signb == SIGINT) { // signal de fin de programme
        printf("\nPUB : Fin du programme\n");
        shmdt(shmadd);
        shmdt(shmadd_PID);
        sem_close(semMSG);
        exit(EXIT_SUCCESS);
    }
}
// ----------------------------------


int main (int argc, char ** argv) {
    printf("PUB (%d): Init\n", getpid());

    // -- CREATION DU SIGNAL HANDLER  ---
    struct sigaction mask;
    sigset_t old;
    mask.sa_flags = 0; //pas de flag particulier
    mask.sa_handler = publisherHandler;
    CHECK(sigemptyset(&mask.sa_mask), "PUB : Erreur lors du SIGEMPTY\n"); //init masque de signaux a vide
    CHECK(sigprocmask(SIG_SETMASK, &mask.sa_mask, &old), "PUB : Erreur lors du SIGPROC"); //application du masque
    CHECK(sigaction(SIGUSR1, &mask, NULL), "PUB : Erreur lors du SIGACTION pour SIGUSR1\n"); //enregistrement handler pour SIGUSR1 (confirmation broker)
    CHECK(sigaction(SIGINT, &mask, NULL), "PUB : Erreur lors du SIGACTION pour SIGINT\n"); //enregistrement handler pour SIGINT (arret avec CTRL+C)
    // -------------------------------------

    // * -- RECUPERATION DU PID DU BROKER * ---
    // V2 : utilisation d'une SHM
    printf("PUB : Recuperation du PID du Broker\n");
    printf("\t> Generation de la cle pour la SHM_PID\n");
    key_t tok_PID = ftok(TOK_FILE,ID_PROJET+1);
    CHECK(tok_PID,"PUB : Erreur creation cle pour la SHM\n");
    printf("\t> Recuperation de l'id de la SHM\n");
    shmid_PID = shmget(tok_PID,sizeof(int), 0666);
    CHECK(shmid_PID,"PUB : Erreur lors de la recuperation d'id pour la SHM\n");
    printf("\t> Obtenir Mem Addr de la SHM\n");
    shmadd_PID = shmat(shmid_PID,NULL,0);
    if (shmadd_PID == (void*)-1) { perror("PUB : Erreur lors de l'obtention de l'adresse Memoire de la SHM\n"); exit(-1); }
    printf("\t> Sauvegarde du PID\n");
    broker_pid = *shmadd_PID;
    printf("PUB : PID du Broker trouvé (%d)\n",broker_pid);
    // -----------------------------------------

    // * ---------- OUVERTURE SEMAPHORE ------------- * //
    //ouverture semaphore creee par broker pour proteger acces shm
    printf("PUB : Ouverture du semaphore\n");
    semMSG = sem_open("/msg", 0); // 0=ouverture pas creation
    if (semMSG == SEM_FAILED) { perror("PUB : Erreur lors de l'ouverture du semaphore\n"); exit(-1); }
    printf("PUB : Fin ouverture du semaphore\n");
    // ----------------------------------------

    // * ----- CONNEXION A LA SHM * ---- //
    printf("PUB : Connexion a la SHM pour les messages\n");
    //creation cle ftok
    printf("\t> Creation de la cle\n");
    key_t tok = ftok(TOK_FILE, ID_PROJET);
    CHECK(tok, "PUB : Erreur creation cle pour la SHM\n");
    printf("\t> Recuperation de l'id de la SHM\n");
    //recuperation id shm existante
    shmid = shmget(tok, sizeof(struct Message), 0666);
    CHECK(shmid, "PUB : Erreur lors de la recuperation d'id pour la SHM\n");
    //association shm a espace memoire
    printf("\t> Attachement a la SHM\n");
    shmadd = shmat(shmid, NULL, 0);
    if (shmadd == (void*)-1) { perror("PUB : Erreur lors de l'attachement a la SHM\n"); exit(-1); }
    printf("PUB : Fin connexion SHM\n");
    // ------------------------------------

    // --- Main Code -------- //
    printf("PUB : Configuration Finie - Pret a publier\n");
    
    char topic[MAX_NOMTOPIC]; //buffer topic
    char message[MAX_MSG]; //buffer message

    //boucle infinie publication
    while (1) {
        // Saisie topic
        printf("\n--- Nouvelle publication ---\n");
        printf("Topic (ou 'quit' pour quitter) : ");
        fgets(topic, MAX_NOMTOPIC, stdin);
        topic[strcspn(topic, "\n")] = 0;

        //verif 'est-ce que l'utilisateur veut quitter ?'
        if (strcmp(topic, "quit") == 0) {
            printf("PUB : Arret demande\n");
            break;
        }

        //verif topic non vide
        if (strlen(topic) == 0) {
            printf("PUB : Topic vide\n");
            continue;
        }

        // Saisie message
        printf("Message : ");
        fgets(message, MAX_MSG, stdin);
        message[strcspn(message, "\n")] = 0;

        //verif message non vide
        if (strlen(message) == 0) {
            printf("PUB : Message vide\n");
            continue;
        }

        // Publication du message
        printf("PUB : Publication en cours...\n");
        confirmation = 0; //reinitialisation flag confirmation
        
        printf("\t> Attente de la disponibilite du semaphore\n");
        sem_wait(semMSG);
        
        //ecriture message dans shm
        printf("\t> Ecriture du message dans la SHM\n");
        struct Message *msg = (struct Message *)shmadd;
        //copie topic sans debordement
        strncpy(msg->topic, topic, MAX_NOMTOPIC - 1);
        msg->topic[MAX_NOMTOPIC - 1] = '\0';
        //copie message sans debordement
        strncpy(msg->msg, message, MAX_MSG - 1);
        msg->msg[MAX_MSG - 1] = '\0';
        //renseignement sender 
        msg->sender = getpid();
        //recepter = 0 -> pour tous les abonnes
        msg->recepter = 0;
        //publication = -1 pour l'action
        msg->action = -1;
        
        printf("\t> Liberation du semaphore\n");
        sem_post(semMSG);
        
        printf("\t> Notification du broker\n");
        kill(broker_pid, SIGUSR1);
        
        printf("\t> Attente de confirmation\n");
        int timeout = 0;
        while (!confirmation && timeout < 5) {
            sleep(1);
            timeout++;
        }
        
        if (confirmation) {
            printf("PUB : Message publie avec succes\n");
        } else {
            printf("PUB : Timeout - pas de confirmation\n");
        }
    }

    // * --- Nettoyage * ---
    printf("PUB : Nettoyage\n");
    shmdt(shmadd);
    shmdt(shmadd_PID);
    sem_close(semMSG);
    printf("PUB : Fin du programme\n");
    
    exit(EXIT_SUCCESS);
}
// ------- //
