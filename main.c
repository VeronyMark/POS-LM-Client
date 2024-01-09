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

typedef struct Sprava{
    char ans[BUFFER_CAPACITY][256];
} SPRAVA;

typedef struct ThreadData {
    char buffer[BUFFER_CAPACITY][256];
    int buffer_size;

    pthread_mutex_t mutex;
    pthread_cond_t is_full;
    pthread_cond_t is_empty;
    int server_socket;
    char questions[BUFFER_CAPACITY][256];
} THREAD_DATA;

void Sprava_setAns(SPRAVA *sprava, int index, const char *value) {
    if (index >= 0 && index < 7) {
        snprintf(sprava->ans[index], sizeof(sprava->ans[index]), "%s", value);
    }
}


void Sprava_serialize(const SPRAVA *sprava, char *output) {
    snprintf(output, 1024, "%s;%s;%s;%s;%s;%s;%s;",
             sprava->ans[0], sprava->ans[1], sprava->ans[2],
             sprava->ans[3], sprava->ans[4], sprava->ans[5], sprava->ans[6]);
}

void ThreadData_init(THREAD_DATA *data, int server_socket) {
    data->buffer_size = 0;
    pthread_mutex_init(&data->mutex, NULL);
    pthread_cond_init(&data->is_full, NULL);
    pthread_cond_init(&data->is_empty, NULL);
    data->server_socket = server_socket;

    snprintf(data->questions[0], sizeof(data->questions[0]), "Zadaj pocet riadkov");
    snprintf(data->questions[1], sizeof(data->questions[1]), "Zadaj pocet stlpcov");
    snprintf(data->questions[2], sizeof(data->questions[2]), "Zadajte pocet mravcov v simulacii:");
    snprintf(data->questions[3], sizeof(data->questions[3]), "Zadajte pocet krokov:");
    snprintf(data->questions[4], sizeof(data->questions[4]), "Zadajte cislo [0] RANDOM [1] MANUAL [2] FILE");
    snprintf(data->questions[5], sizeof(data->questions[5]), "Zadajte logiku [0] PRIAMA [1] INVERZNA");
    snprintf(data->questions[6], sizeof(data->questions[6]), "Zadajte riesenie kolizii:\n[0] Mravec pri kolizii prestane existovat\n[1] Mravec sa pri kolizii zacne spravat podla opacnej logiky");
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

        char input[256];
        scanf("%s", input);

        pthread_mutex_lock(&data->mutex);
        while (data->buffer_size >= BUFFER_CAPACITY) {
            pthread_cond_wait(&data->is_empty, &data->mutex);
        }

        snprintf(data->buffer[data->buffer_size], sizeof(data->buffer[data->buffer_size]), "%s", input);
        ++data->buffer_size;

        pthread_cond_signal(&data->is_full);
        pthread_mutex_unlock(&data->mutex);
    }
}

void ThreadData_consume(THREAD_DATA *data) {
    while (1) {
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

            Sprava_setAns(&sprava, i, item);
        }

        printf("UKONCENIE\n");

        if (data->server_socket != NULL) {
            char output[1024];
            Sprava_serialize(&sprava, output);
            write(data->server_socket, output, strlen(output));
        }
        char buffer[1024];

        bzero(buffer,1024);
        read(data->server_socket, buffer, 255);

        printf("%s\n",buffer);
        close(data->server_socket);
    }
}

void *produce(void *data) {
    ThreadData_produce((THREAD_DATA *)data);
    return NULL;
}

void *consume(void *data) {
    ThreadData_consume((THREAD_DATA *)data);
    return NULL;
}

int main(int argc, char *argv[]) {

    int sockfd, n;
    struct sockaddr_in serv_addr;
    struct hostent* server;

    char buffer[256];

    if (argc < 3) {
        fprintf(stderr,"usage %s hostname port\n", argv[0]);
        return 1;
    }

    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr, "Error, no such host\n");
        return 2;
    }

    bzero((char*)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy(
            (char*)server->h_addr,
            (char*)&serv_addr.sin_addr.s_addr,
            server->h_length
    );
    serv_addr.sin_port = htons(atoi(argv[2]));

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error creating socket");
        return 3;
    }

    if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error connecting to socket");
        return 4;
    }

    // Initialize my_socket if necessary
    THREAD_DATA data;
    ThreadData_init(&data, sockfd);

    pthread_t th_produce, th_consume;
    pthread_create(&th_produce, NULL, produce, &data);
    pthread_create(&th_consume, NULL, consume, &data);

    pthread_join(th_produce, NULL);
    pthread_join(th_consume, NULL);


    ThreadData_destroy(&data);
    return 0;
}
