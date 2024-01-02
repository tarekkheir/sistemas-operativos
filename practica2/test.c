#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>

FILE *fs;
int NUM_PLACES = 0;
int NUM_PARKING = 0;
int NUM_USERS = 0;
int TIEMPO_MIN_COGER = 0;
int TIEMPO_MAX_COGER = 0;
int TIEMPO_MIN_MONTANDO = 0;
int TIEMPO_MAX_MONTANDO = 0;
int values[7];

int openFile(char *nombre_fe) {
    FILE *fe;
    int i = 0;
    char entrada[100];
    char buffer[1024];
    
    strcpy(entrada, nombre_fe);

    if ((fe = fopen(entrada,"r")) == NULL) {
        fprintf(stderr,"Error al abrir el fichero %s.\n%s.\n",entrada,strerror(errno));
        return 1;
    }

    while (fgets(buffer, 1024, fe) != NULL) {
        values[i] = atoi(buffer);
        i++;
    }

    int NUM_USERS = values[0];
    int NUM_PARKING = values[1];
    int NUM_PLACES = values[2];
    int TIEMPO_MIN_COGER = values[3];
    int TIEMPO_MAX_COGER = values[4];
    int TIEMPO_MIN_MONTANDO = values[5];
    int TIEMPO_MAX_MONTANDO = values[6];

    fprintf(fs, "Première ligne dans le fichier.\n");
    fprintf(fs, "Deuxième ligne dans le fichier.\n");
    
    fclose(fe);

    return 0;
}

int main (int argc, char *argv[]) {
    char nombre_fe[100];
    char nombre_fs[100];
    char date_heure[100];
    struct tm *temps_info;
    time_t temps_actuel;

    if (argc > 3) {
        printf("Trop d'arguments\n");
        return 0;
    } else if (argc > 1) {
        strcpy(nombre_fe, argv[1]);
        
        if (argc == 3) {
            strcpy(nombre_fs, argv[2]);
        } else {
            time(&temps_actuel);
            temps_info = localtime(&temps_actuel);

            strftime(date_heure, sizeof(date_heure), "%Y-%m-%d%H:%M:%S", temps_info);
            strcpy(nombre_fs, "salida_sim_BiciMAD");
            strcat(nombre_fs, date_heure);
            strcat(nombre_fs, ".txt");
        }
    
    } else strcpy(nombre_fe, "entrada_BiciMAD.txt");

    printf("salida: %s\n", nombre_fs);

    if ((fs = fopen(nombre_fs, "w")) == NULL) {
        fprintf(stderr,"Error al abrir el fichero %s.\n%s.\n", nombre_fs, strerror(errno));
        return 0;
    }
    if (openFile(nombre_fe) != 0) return 0;

    fclose(fs);

    return 0;
}