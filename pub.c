#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <signal.h>

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
};

// -- DEFINITION VARIABLE GLOBALE ---
// > Variable de SHM
int shmid; // id de la SHM
char * shmadd; // adresse de la SHM

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
    printf("PUB : Entrez le PID du broker : "); //recup PID broker pour envoi signaux
    scanf("%d", &broker_pid);
    //verification de l'existence du broker
    CHECK(kill(broker_pid, 0), "PUB : Le broker n'existe pas\n"); 
    printf("PUB : Broker trouve (PID: %d)\n", broker_pid);
    // -----------------------------------------

    // * ---------- OUVERTURE SEMAPHORE ------------- * //
    //ouverture semaphore creee par broker pour proteger acces shm
    printf("PUB : Ouverture du semaphore\n");
    semMSG = sem_open("/msg", 0); // 0=ouverture pas creation
    CHECK(semMSG, "PUB : Erreur lors de l'ouverture du semaphore\n");
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
    shmid = shmget(tok, sizeof(struct Message), 0666); //on utilise pas 777 ici
    CHECK(shmid, "PUB : Erreur lors de la recuperation d'id pour la SHM\n");
    //association shm a espace memoire
    printf("\t> Attachement a la SHM\n");
    shmadd = shmat(shmid, NULL, 0);
    CHECK(shmadd, "PUB : Erreur lors de l'attachement a la SHM\n");
    printf("PUB : Fin connexion SHM\n");
    // ------------------------------------

    // --- Main Code -------- //
    printf("PUB : Configuration Finie - Pret a publier\n");
    
    char topic[MAX_NOMTOPIC]; //buffer topic
    char message[MAX_MSG]; //buffer message
    int c;

    while ((c = getchar()) != '\n' && c != EOF); // vider buffer

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
        
        //ecriture message dans sem
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
    sem_close(semMSG);
    printf("PUB : Fin du programme\n");
    
    exit(EXIT_SUCCESS);
}
// ------- //
