#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#define VERDE "\033[00;32m"
#define ROJO "\033[00;31m"
#define AMARILLO "\033[00;33m"
#define AZUL "\033[0;34m"
#define NARANJA "\033[38;5;208m"

FILE *fs;

sem_t *sem_parking;
sem_t *sem_queue_departure;
sem_t *sem_queue_arrival;

pthread_t *users;    

int NUM_PLACES = 0;
int NUM_PARKING = 0;
int NUM_USERS = 0;
int TIEMPO_MIN_COGER = 0;
int TIEMPO_MAX_COGER = 0;
int TIEMPO_MIN_MONTANDO = 0;
int TIEMPO_MAX_MONTANDO = 0;
int values[7];

int **parking;
int **statuts;
int *places_occupees;
int shouldTerminate = 1;

// Argumentos de las funciones de los hilos
typedef struct {
    int user_id;
    int parking_id;
    int tentatives;
} TypeData;

int openFile(char *nombre_fe) {
    FILE *fe;
    int i = 0;
    char entrada[100];
    char buffer[1024];
    
    strcpy(entrada, nombre_fe);

    // Abrir el archivo de entrada
    if ((fe = fopen(entrada,"r")) == NULL) {
        fprintf(stderr,"Error al abrir el fichero %s.\n%s.\n",entrada,strerror(errno));
        return 1;
    }

    // Leer línea por línea el archivo de entrada
    while (fgets(buffer, 1024, fe) != NULL) {
        values[i] = atoi(buffer);
        i++;
    }

    NUM_USERS = values[0];
    NUM_PARKING = values[1];
    NUM_PLACES = values[2];
    TIEMPO_MIN_COGER = values[3];
    TIEMPO_MAX_COGER = values[4];
    TIEMPO_MIN_MONTANDO = values[5];
    TIEMPO_MAX_MONTANDO = values[6];


    // Asignación de memoria para las variables globales
    sem_parking = malloc(sizeof(sem_t)*NUM_PARKING);
    sem_queue_departure = malloc(sizeof(sem_t)*NUM_PARKING);
    sem_queue_arrival = malloc(sizeof(sem_t)*NUM_PARKING);

    for(int i=0; i < NUM_PARKING; i++) {
        sem_parking[i] = NULL;
        sem_queue_arrival[i] = NULL;
        sem_queue_departure[i] = NULL;
    }

    parking = malloc(sizeof(int*)*NUM_PARKING);

    for(int i=0; i < NUM_PARKING; i++) {
        parking[i] = malloc(sizeof(int)*NUM_PLACES);
        for(int j=0; j < NUM_PLACES; j++) {
            parking[i][j] = 0;
        }
    }
    
    statuts = malloc(sizeof(int*)*NUM_USERS);

    for(int i=0; i < NUM_USERS; i++) {
        statuts[i] = malloc(sizeof(int)*2);
        for(int j=0; j < 2; j++) {
            statuts[i][j] = 0;
        }
    }
    
    places_occupees = malloc(sizeof(int)*NUM_PARKING);
    for(int i=0; i < NUM_PARKING; i++) places_occupees[i] = 0;

    users = malloc(sizeof(pthread_t)*NUM_USERS);
    
    fclose(fe);

    return 0;
}

void init() {
    // Datos de partida
    printf("BiciMAD: CONFIGURACION INICIAL\n");
    printf("Usuarios: [%d]\n", NUM_USERS);
    printf("Numero de Estaciones: [%d]\n", NUM_PARKING);
    printf("Numero de huecos por estacion: [%d]\n", NUM_PLACES);
    printf("Tiempo minimo de espera para decidir coger una bici: [%d]\n", TIEMPO_MIN_COGER);
    printf("Tiempo maximo de espera para decidir coger una bici: [%d]\n", TIEMPO_MAX_COGER);
    printf("Tiempo minimo que pasa un usuario montando una bici: [%d]\n", TIEMPO_MIN_MONTANDO);
    printf("Tiempo maximo que pasa un usuario montando una bici: [%d]\n", TIEMPO_MAX_MONTANDO);
    printf("\nSIMULACIÓN FUNCIONAMIENTO BiciMAD\n\n");

    // Escritura en el archivo de salida
    fprintf(fs, "BiciMAD: CONFIGURACION INICIAL\n");
    fprintf(fs, "Usuarios: [%d]\n", NUM_USERS);
    fprintf(fs, "Numero de Estaciones: [%d]\n", NUM_PARKING);
    fprintf(fs, "Numero de huecos por estacion: [%d]\n", NUM_PLACES);
    fprintf(fs, "Tiempo minimo de espera para decidir coger una bici: [%d]\n", TIEMPO_MIN_COGER);
    fprintf(fs, "Tiempo maximo de espera para decidir coger una bici: [%d]\n", TIEMPO_MAX_COGER);
    fprintf(fs, "Tiempo minimo que pasa un usuario montando una bici: [%d]\n", TIEMPO_MIN_MONTANDO);
    fprintf(fs, "Tiempo maximo que pasa un usuario montando una bici: [%d]\n", TIEMPO_MAX_MONTANDO);
    fprintf(fs, "\nSIMULACIÓN FUNCIONAMIENTO BiciMAD\n\n");
    
    // Inicialización de los aparcamientos a 3/4 de su capacidad
    for (int i=0; i < NUM_PARKING; i++) {
        for(int j=0; j < NUM_PLACES; j++) {
            if (j <= (NUM_PLACES*3) / 4)
            parking[i][j] = 1;
            places_occupees[i]++;
        }
    }
}

void checkEnd(int user_id) { // Verificación de fin de programa y liberación de semáforos en espera
    int end = 0;

    for(int i=0; i < NUM_USERS; i++) {
        if (statuts[i][0] == 0) end = 1;
    }

    // Liberacion de semaforos en espera
    if (end == 0) {
        shouldTerminate = 0;
        printf("Final del programa\n");
        fprintf(fs, "Final del programa\n");
        sleep(1);

        for(int i=0; i < NUM_USERS; i++) {
            if (statuts[i][0] == 2) sem_post(&sem_queue_departure[statuts[i][1]]);
            if (statuts[i][0] == 3) sem_post(&sem_queue_arrival[statuts[i][1]]);
        }
    }
}

void ultimateClean() { // Verificación y liberación adicional de semáforos en espera
    for(int i=0; i < NUM_PARKING; i++) {
        int value_arrival = 0;
        int value_departure = 0;

        sem_getvalue(&sem_queue_arrival[i], &value_arrival);
        sem_getvalue(&sem_queue_departure[i], &value_departure);
        
        value_arrival *= (-1);
        value_departure *= (-1);
        
        if (value_arrival != 0) {
            for(int j=0; j < value_arrival; j++)
                sem_post(&sem_queue_arrival[i]);
        }
        if (value_departure != 0) {
            for(int j=0; j < value_departure; j++)
                sem_post(&sem_queue_departure[i]);
        }
    }
}

void* bici_arrival(void* args) {
    TypeData* data = (TypeData*)args;
    int id = data->user_id;
    int parking_id = data->parking_id;
    int tentatives = data->tentatives;
    int value = 0;

    sem_wait(&sem_parking[parking_id]); // Esperando el semáforo para acceder a las plazas de aparcamiento

    if (tentatives == 1) {
        printf(NARANJA "Usuario [%d] quiere dejar bici en Estacion [%d]\033[0m\n", id, parking_id);
        fprintf(fs, "Usuario [%d] quiere dejar bici en Estacion [%d]\n", id, parking_id);
    }
    // Búsqueda de un lugar libre
    int parking_spot = -1;
    for (int i = 0; i < NUM_PLACES; i++) {
        if (parking[parking_id][i] == 0) {
            parking_spot = i;
            parking[parking_id][i] = 1; // Marcar el lugar como ocupado
            places_occupees[parking_id]++;
            
            if (places_occupees[parking_id] > 0) {
                sem_getvalue(&sem_queue_departure[parking_id], &value);

                if (value*(-1) > 0)
                    sem_post(&sem_queue_departure[parking_id]); // Liberar un hilo de la cola inicial
            }

            printf(ROJO "Usuario [%d] deja bici en Estacion [%d]\033[0m\n", id, parking_id);
            fprintf(fs, "Usuario [%d] deja bici en Estacion [%d]\n", id, parking_id);            
            break;
        }
    }

    sem_post(&sem_parking[parking_id]); // Liberar el semáforo después de haber terminado

    if (parking_spot == -1) { // Crear un nuevo hilo de estacionamiento 
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

    sem_wait(&sem_parking[parking_id]); // Esperando el semáforo para acceder a las plazas de aparcamiento
    
    // Elección del tiempo de toma de decisiones dentro de la seccion critica para que sea único
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    srand((unsigned int)(currentTime.tv_usec));
    
    cogiendo = rand() % TIEMPO_MAX_COGER + TIEMPO_MIN_COGER;
    
    if (tentatives == 1) {
        printf("Usuario [%d] quiere coger bici de Estacion [%d]\n", id, parking_id);
        fprintf(fs, "Usuario [%d] quiere coger bici de Estacion [%d]\n", id, parking_id);
    }

    sleep(cogiendo);
    printf(AMARILLO "Usuario [%d] coge bici de Estacion [%d]\033[0m\n", id, parking_id);
    fprintf(fs, "Usuario [%d] coge bici de Estacion [%d]\n", id, parking_id);

    // Búsqueda de un lugar ocupado
    int parking_spot = -1;
    for (int i = 0; i < NUM_PLACES; i++) {
        if (parking[parking_id][i] == 1) {
            parking_spot = i;
            parking[parking_id][i] = 0; // Marcar el lugar como libre
            places_occupees[parking_id]--;
            
            if (places_occupees[parking_id] < NUM_PLACES) {
                sem_getvalue(&sem_queue_arrival[parking_id], &value);

                if (value*(-1) > 0)                 
                    sem_post(&sem_queue_arrival[parking_id]);
            }

            struct timeval currentTime;

            gettimeofday(&currentTime, NULL);
            srand((unsigned int)(currentTime.tv_usec));
            
            montando = rand() % TIEMPO_MAX_MONTANDO + TIEMPO_MIN_MONTANDO;

            gettimeofday(&currentTime, NULL);
            srand((unsigned int)(currentTime.tv_usec));

            destination = rand() % (NUM_PARKING);
            while(destination == parking_id) destination = rand() % (NUM_PARKING);
            
            printf(VERDE "Usuario [%d] montando en bici..., destination: Estacion [%d]\033[0m\n", id, destination);
            fprintf(fs, "Usuario [%d] montando en bici..., destination: Estacion [%d]\n", id, destination);            
            break;
        }
    }

    sem_post(&sem_parking[parking_id]); // Liberar el semáforo después de haber terminado

    if (parking_spot != -1) { // Crear nuevo hilo de estacionamiento
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
        sem_wait(&sem_queue_departure[parking_id]); // Puesta en espera en la cola inicial
        
        statuts[id][0] = 0;
        statuts[id][1] = 0;
        
        checkEnd(id);
        // Reiniciar un hilo de partida
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

    if (shouldTerminate != 0) {
        statuts[id][0] = 1;
        statuts[id][1] = -1;
        checkEnd(id);
    } else ultimateClean();

    pthread_exit(NULL);
    return NULL;
}

void cleanVariableGlobale() {

    for(int i=0; i < NUM_PARKING; i++) {
        free(sem_parking[i]);
        free(sem_queue_arrival[i]);
        free(sem_queue_departure[i]);
    }

    for(int i=0; i < NUM_PARKING; i++) free(parking[i]);
    for(int i=0; i < NUM_USERS; i++) free(statuts[i]);
    free(places_occupees);
    free(users);
}

int main(int argc, char *argv[]) {
    char nombre_fe[100];
    char nombre_fs[100];
    char date_heure[100];
    struct tm *temps_info;
    time_t temps_actuel;

    // Verificación de argumentos de ejecución y asignación de nombres de archivo
    if (argc > 3) {
        printf("Demasiados argumentos\n");
        return 0;
    } else if (argc > 1) {
        strcpy(nombre_fe, argv[1]);
        if (argc == 3) strcpy(nombre_fs, argv[2]);
    } else {
        strcpy(nombre_fe, "entrada_BiciMAD.txt");
        time(&temps_actuel);
        temps_info = localtime(&temps_actuel);

        strftime(date_heure, sizeof(date_heure), "%Y-%m-%d%H-%M-%S", temps_info);
        strcpy(nombre_fs, "salida_sim_BiciMAD");
        strcat(nombre_fs, date_heure);
        strcat(nombre_fs, ".txt");
    }

    if ((fs = fopen(nombre_fs, "w")) == NULL) {
        fprintf(stderr,"Error al abrir el fichero %s.\n%s.\n", nombre_fs, strerror(errno));
        return 0;
    }
    if (openFile(nombre_fe) != 0) return 0;


    srand(time(NULL));
    init();

    for(int i=0; i < NUM_PARKING; i++) {
        sem_init(&sem_parking[i], 0, 1); // Initialisacion del semaforo parking
        sem_init(&sem_queue_departure[i], 0, 0); // Initialisacion del semaforo queue de departure
        sem_init(&sem_queue_arrival[i], 0, 0); // Initialisacion del semaforo queue de arrival
    }

    // Creación de hilos para bici entrantes
    for (int i = 0; i < NUM_USERS; i++) {
        TypeData* data = malloc(sizeof(TypeData));
        
        if (data == NULL) exit(EXIT_FAILURE);
        data->user_id = i;
        data->parking_id = rand() % NUM_PARKING;
        data->tentatives = 1;
        pthread_create(&users[i], NULL, bici_departure, (void*)data);
    }

    // Esperando el final de todos los hilos
    for (int i = 0; i < NUM_USERS; i++) {
        pthread_join(users[i], NULL);
    }

    fclose(fs);
    cleanVariableGlobale(); // Liberacion de memoria de los variables globales

    for(int i=0; i < NUM_PARKING; i++) {
        sem_destroy(&sem_parking[i]); // Destrucción del semáforo cola de parking
        sem_destroy(&sem_queue_departure[i]); // Destruction du sémaphore cola de salida
        sem_destroy(&sem_queue_arrival[i]); // Destrucción del semáforo cola de llegada
    }

    return 0;
}
