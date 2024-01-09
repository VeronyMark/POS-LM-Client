#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#define BUFFER_CAPACITY 7

/**
 * SPRAVA
 */
typedef struct Sprava {
    char odpovede[BUFFER_CAPACITY][256];
} SPRAVA;

void sprava_setOdpoved(SPRAVA *sprava, int index, const char *hodnota) {
    if (index >= 0 && index < BUFFER_CAPACITY) {
        snprintf(sprava->odpovede[index], sizeof(sprava->odpovede[index]), "%s", hodnota);
    }
}

/**
 * KODOVANIE ODPOVEDI KLIENTA,(SPRAVY) DO 1 RETAZCA
 * @param sprava
 * @param output
 */
void sprava_koduj(const SPRAVA *sprava, char *output) {
    snprintf(output, 1024, "%s;%s;%s;%s;%s;%s;%s;",
             sprava->odpovede[0], sprava->odpovede[1], sprava->odpovede[2],
             sprava->odpovede[3], sprava->odpovede[4], sprava->odpovede[5], sprava->odpovede[6]);
}

const char *questions[BUFFER_CAPACITY] = {
        "Zadaj pocet riadkov",
        "Zadaj pocet stlpcov",
        "Zadajte pocet mravcov v simulacii",
        "Zadajte pocet krokov",
        "Zadajte cislo [0] RANDOM [1] MANUAL [2] FILE",
        "Zadajte logiku [0] PRIAMA [1] INVERZNA",
        "Zadajte riesenie kolizii:\n[0] Mravec pri kolizii prestane existovat\n[1] Mravec sa pri kolizii zacne spravat podla opacnej logiky"
};

/**
 * THREAD DATA
 */
typedef struct ThreadData {
    char buffer[BUFFER_CAPACITY][256];
    int buffer_size;

    pthread_mutex_t mutex;
    pthread_cond_t is_full;
    pthread_cond_t is_empty;

    int server_socket;
    char *questions[BUFFER_CAPACITY];

} THREAD_DATA;

void ThreadData_init(THREAD_DATA *data, int server_socket) {
    data->buffer_size = 0;

    pthread_mutex_init(&data->mutex, NULL);
    pthread_cond_init(&data->is_full, NULL);
    pthread_cond_init(&data->is_empty, NULL);

    data->questions[0] = "Zadaj pocet riadkov";
    data->questions[1] = "Zadaj pocet stlpcov";
    data->questions[2] = "Zadajte pocet mravcov v simulacii:";
    data->questions[3] = "Zadajte pocet krokov:";
    data->questions[4] = "Zadajte cislo [0] RANDOM [1] MANUAL [2] FILE";
    data->questions[5] = "Zadajte logiku [0] PRIAMA [1] INVERZNA";
    data->questions[6] = "Zadajte riesenie kolizii:\n[0] Mravec pri kolizii prestane existovat\n[1] Mravec sa pri kolizii zacne spravat podla opacnej logiky";

    data->server_socket = server_socket;
}

void ThreadData_destroy(THREAD_DATA *data) {
    pthread_mutex_destroy(&data->mutex);
    pthread_cond_destroy(&data->is_full);
    pthread_cond_destroy(&data->is_empty);
}

void ThreadData_produce(THREAD_DATA *data) {

    for (int i = 0; i < BUFFER_CAPACITY; ++i) {

        const char *current_question = data->questions[i];

        printf("OTAZKA %d: %s\n", i + 1, current_question);

        char odpovedKlienta[256];
        scanf("%s", odpovedKlienta);

        pthread_mutex_lock(&data->mutex);
        while (data->buffer_size >= BUFFER_CAPACITY) {
            pthread_cond_wait(&data->is_empty, &data->mutex);
        }

        snprintf(data->buffer[data->buffer_size], sizeof(data->buffer[data->buffer_size]), "%s", odpovedKlienta);
        ++data->buffer_size;

        pthread_cond_signal(&data->is_full);
        pthread_mutex_unlock(&data->mutex);
    }
}

void ThreadData_consume(THREAD_DATA *data) {
    SPRAVA sprava;

    for (int i = 0; i < BUFFER_CAPACITY; ++i) {
        char item[256];

        pthread_mutex_lock(&data->mutex);
        while (data->buffer_size <= 0) {
            pthread_cond_wait(&data->is_full, &data->mutex);
        }
        snprintf(item, sizeof(item), "%s", data->buffer[0]);
        --data->buffer_size;
        pthread_cond_signal(&data->is_empty);
        pthread_mutex_unlock(&data->mutex);

        sprava_setOdpoved(&sprava, i, item);
    }

    if (data->server_socket != NULL) {
        char output[1024];
        sprava_koduj(&sprava, output);
        write(data->server_socket, output, strlen(output));
    }
}


void *produce(void *data) {
    ThreadData_produce((THREAD_DATA *) data);
    return NULL;
}

void *consume(void *data) {
    ThreadData_consume((THREAD_DATA *) data);
    return NULL;
}


int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    if (argc < 3) {
        fprintf(stderr, "usage %s hostname port\n", argv[0]);
        return 1;
    }

    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr, "Error, no such host\n");
        return 2;
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy(
            (char *) server->h_addr,
            (char *) &serv_addr.sin_addr.s_addr,
            server->h_length
    );

    serv_addr.sin_port = htons(atoi(argv[2]));

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("ERROR PRI VYTVARANI SOCKETU");
        return 3;
    }

    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR PRI PRIPAJANI NA SOCKET");
        return 4;
    }


    THREAD_DATA data;

    ThreadData_init(&data, sockfd);

    pthread_t th_produce, th_consume;

    pthread_create(&th_produce, NULL, produce, &data);
    pthread_create(&th_consume, NULL, consume, &data);

    pthread_join(th_produce, NULL);
    pthread_join(th_consume, NULL);


    char buffer[1024];
    bzero(buffer, 1024);

    read(sockfd, buffer, 255);

    printf("VYSLEDOK SIMULÁCIE\n");
    printf("%s\n", buffer);
    close(sockfd);
    ThreadData_destroy(&data);

    return 0;
}
