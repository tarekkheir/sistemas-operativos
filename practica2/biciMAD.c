#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#define NUM_PLACES 5
#define NUM_PARKING 5
#define NUM_USERS 50
#define MAX_WAIT_SECONDS 20
#define VERT "\033[00;32m"
#define ROUGE "\033[00;31m"
#define JAUNE "\033[00;33m"
#define BLEU "\033[0;34m"
#define ORANGE "\033[38;5;208m"

sem_t sem_parking[NUM_PARKING];
sem_t sem_queue_departure[NUM_PARKING];
sem_t sem_queue_arrival[NUM_PARKING];
pthread_t users[NUM_USERS];    
int parking[NUM_PARKING][NUM_PLACES];
int places_occupees[NUM_PARKING];
int statuts[NUM_USERS][2];
int shouldTerminate = 1;

// Arguments des fonctions des threads
typedef struct {
    int user_id;
    int parking_id;
    int tentatives;
} TypeData;

void init_parking() {
    // Initialisation des parking au 3/4 de sa capacité
    for (int i = 0; i < NUM_PARKING; i++) {
        for(int j=0; j < NUM_PLACES; j++) {
            parking[i][j] = 1;
            places_occupees[i]++;
            printf("parking [%d]: [%d] places occupees, index=[%d]\n", i, places_occupees[i], j);
        }
    }
}

void printStatuts() {
    printf("[");
    for(int i=0; i < NUM_USERS; i++) {
        printf("[%d,%d]", statuts[i][0], statuts[i][1]);
        if (i < (NUM_USERS-1)) printf(", ");
        else printf("]\n");
    }
}

void checkEnd(int user_id) {
    int end = 0;

    for(int i=0; i < NUM_USERS; i++) {
        if (statuts[i][0] == 0) end = 1;
    }

    if (end == 0) {
        shouldTerminate = 0;
        printf("Fin du programme\n");
        printf("user_id: [%d]\n", user_id);
        sleep(5);
        printStatuts();

        for(int i=0; i < NUM_USERS; i++) {
            // printf("[%d] statut: [%d,%d]\n", i, statuts[i][0], statuts[i][1]);
            if (statuts[i][0] == 2) sem_post(&sem_queue_departure[statuts[i][1]]);
            if (statuts[i][0] == 3) sem_post(&sem_queue_arrival[statuts[i][1]]);
        }
    }
}

void ultimateClean() {
    for(int i=0; i < NUM_PARKING; i++) {
        int value_arrival = 0;
        int value_departure = 0;
        sem_getvalue(&sem_queue_arrival[i], &value_arrival);
        sem_getvalue(&sem_queue_departure[i], &value_departure);
        
        value_arrival *= (-1);
        value_departure *= (-1);
        
        if (value_arrival != 0) {
            for(int j=0; j < value_arrival; j++) sem_post(&sem_queue_arrival[i]);
        }
        if (value_departure != 0) {
            for(int j=0; j < value_departure; j++) sem_post(&sem_queue_departure[i]);
        }
    }
}

void* bici_arrival(void* args) {
    TypeData* data = (TypeData*)args;
    int id = data->user_id;
    int parking_id = data->parking_id;
    int tentatives = data->tentatives;
    int value = 0;

    sem_wait(&sem_parking[parking_id]); // Attente du sémaphore pour accéder aux places de parking

    printf("[%d] veut se gare au parking [%d], tentatives: %d\n", id, parking_id, tentatives);
    // Recherche d'une place libre
    int parking_spot = -1;
    for (int i = 0; i < NUM_PLACES; i++) {
        if (parking[parking_id][i] == 0) {
            parking_spot = i;
            parking[parking_id][i] = 1; // Marquer la place comme occupée
            places_occupees[parking_id]++;
            
            if (places_occupees[parking_id] > 0) {
                sem_getvalue(&sem_queue_departure[parking_id], &value);

                if (value*(-1) > 0) {
                    sem_post(&sem_queue_departure[parking_id]); // Libérer un thread de la queue de départ
                    printf(ORANGE "liberation queue depart\033[0m\n");
                }
            }

            printf(ROUGE "%d se gare a place [%d] au parking [%d]\n", id, parking_spot, parking_id);
            printf(JAUNE "parking [%d]: [%d] places occupees, index=[%d]\033[0m\n", parking_id, places_occupees[parking_id], parking_spot);
            
            break;
        }
    }

    if (parking_spot == -1) {
        printf("[%d] Attend pour stationner au parking [%d]\n", id, parking_id);
    }

    sem_post(&sem_parking[parking_id]); // Libérer le sémaphore après avoir terminé

    if (parking_spot == -1) { // Création d'un nouveau thread de stationnement 
        statuts[id][0] = 3;
        statuts[id][1] = parking_id;
        checkEnd(id);
        sem_wait(&sem_queue_arrival[parking_id]);
        statuts[id][0] = 0;
        statuts[id][1] = 0;
        checkEnd(id);

        if (shouldTerminate != 0) {
            pthread_t new_thread;
            TypeData* new_data = malloc(sizeof(TypeData));
            if (new_data == NULL) exit(EXIT_FAILURE);

            new_data->parking_id = parking_id;
            new_data->user_id = id;
            new_data->tentatives = tentatives + 1;

            pthread_create(&new_thread, NULL, bici_arrival, (void*)new_data);
            pthread_join(new_thread, NULL);
        }
    }
    
    free(data);
    if (shouldTerminate != 0) {
        statuts[id][0] = 1;
        statuts[id][1] = -1;
        checkEnd(id);
    } else ultimateClean();

    pthread_exit(NULL);
    return NULL;
}

void* bici_departure(void* args) {
    TypeData* data = (TypeData*)args;
    int id = data->user_id;
    int parking_id = data->parking_id;
    int tentatives = data->tentatives;
    int montando = 0;
    int cogiendo = 0;
    int destination = 0;
    int value = 0;

    sem_wait(&sem_parking[parking_id]); // Attente du sémaphore pour accéder aux places de parking
    
    // Choix du temps de prise de décision à l'intérieur de la seccion critica pour qu'il soit unique
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    srand((unsigned int)(currentTime.tv_usec));
    
    cogiendo = rand() % 4 + 1;
    
    printf("[%d] veut un velo au parking [%d], tentatives: %d\n", id, parking_id, tentatives);
    sleep(cogiendo);

    // Recherche d'une place occupée
    int parking_spot = -1;
    for (int i = 0; i < NUM_PLACES; i++) {
        if (parking[parking_id][i] == 1) {
            parking_spot = i;
            parking[parking_id][i] = 0; // Marquer la place comme libre
            places_occupees[parking_id]--;
            
            if (places_occupees[parking_id] < NUM_PLACES) {
                sem_getvalue(&sem_queue_arrival[parking_id], &value);

                if (value*(-1) > 0) {                    
                    sem_post(&sem_queue_arrival[parking_id]);
                    printf(ORANGE "liberation queue arrivee\033[0m\n");
                }
            }

            struct timeval currentTime;

            gettimeofday(&currentTime, NULL);
            srand((unsigned int)(currentTime.tv_usec));
            
            montando = rand() % 5 + 1;

            gettimeofday(&currentTime, NULL);
            srand((unsigned int)(currentTime.tv_usec));

            destination = rand() % (NUM_PARKING);
            while(destination == parking_id) destination = rand() % (NUM_PARKING);
            
            printf(VERT "[%d] libere la place [%d] au parking [%d], destination: %d\n", id, parking_spot, parking_id, destination);
            printf(JAUNE "parking [%d]: [%d] places occupees, index=[%d]\033[0m\n", parking_id, places_occupees[parking_id], parking_spot);
            break;
        }
    }

    if (parking_spot == -1) {
        printf("[%d] Attend pour demarrer du parking [%d]\n", id, parking_id);
    }

    sem_post(&sem_parking[parking_id]); // Libérer le sémaphore après avoir terminé

    if (parking_spot != -1) { // Création nouveau thread de stationnement
        sleep(montando);

        pthread_t new_thread;
        TypeData* new_data = malloc(sizeof(TypeData));

        if (new_data == NULL) exit(EXIT_FAILURE);

        new_data->parking_id = destination;
        new_data->user_id = id;
        new_data->tentatives = 1;

        pthread_create(&new_thread, NULL, bici_arrival, (void*)new_data);
        pthread_join(new_thread, NULL);
    } else {
        statuts[id][0] = 2;
        statuts[id][1] = parking_id;
        checkEnd(id);
        sem_wait(&sem_queue_departure[parking_id]); // Mise en attente dans la queue de départ
        statuts[id][0] = 0;
        statuts[id][1] = 0;
        checkEnd(id);
        // Relance d'un thread de depart
        if (shouldTerminate != 0) {
            pthread_t new_thread;
            TypeData* new_data = malloc(sizeof(TypeData));
            if (new_data == NULL) exit(EXIT_FAILURE);

            new_data->parking_id = parking_id;
            new_data->user_id = id;
            new_data->tentatives = tentatives + 1;

            pthread_create(&new_thread, NULL, bici_departure, (void*)new_data);
            pthread_join(new_thread, NULL);
        }
    }

    free(data);
    printf(BLEU "Fin du thread [%d], tentative: [%d]\033[0m\n", id, tentatives);
    if (shouldTerminate != 0) {
        statuts[id][0] = 1;
        statuts[id][1] = -1;
        checkEnd(id);
    } else ultimateClean();

    pthread_exit(NULL);
    return NULL;
}

int main() {
    srand(time(NULL));
    init_parking();

    for(int i=0; i < NUM_PARKING; i++) {
        sem_init(&sem_parking[i], 0, 1); // Initialisation du sémaphore parking
        sem_init(&sem_queue_departure[i], 0, 0); // Initialisation du sémaphore queue de départ
        sem_init(&sem_queue_arrival[i], 0, 0); // Initialisation du sémaphore queue d'arrivé
    }

    // Création des threads pour les bici entrantes
    for (int i = 0; i < NUM_USERS; i++) {
        TypeData* data = malloc(sizeof(TypeData));
        
        if (data == NULL) exit(EXIT_FAILURE);
        data->user_id = i;
        data->parking_id = rand() % NUM_PARKING;
        data->tentatives = 1;
        pthread_create(&users[i], NULL, bici_departure, (void*)data);
    }

    // Attente de la fin de tous les threads
    for (int i = 0; i < NUM_USERS; i++) {
        pthread_join(users[i], NULL);
    }

    for(int i=0; i < NUM_PARKING; i++) {
        sem_destroy(&sem_parking[i]); // Destruction du sémaphore parking
        sem_destroy(&sem_queue_departure[i]); // Destruction du sémaphore queue de départ
        sem_destroy(&sem_queue_arrival[i]); // Destruction du sémaphore queue d'arrivé
    }

    return 0;
}
