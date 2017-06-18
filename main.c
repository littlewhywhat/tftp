#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

typedef enum app_mode_e {
    CLT_MODE,
    SRV_MODE,
} APP_MODE;

APP_MODE app_mode;

typedef enum opstat_e {
    FAIL = 0,
    SUCCESS = 1
} OPSTAT;

/* RND */

void RND_init() {
    srand(time(NULL));
}

OPSTAT RND_opt_stat() {
    return rand() % 2;
}

/* ERR */

typedef enum error_e {
    NO_ERR,
    NET_ACCPT_ERR,
    NET_CONN_ERR,
    NET_BIND_ERR,
    FILE_OPEN_ERR,
    PCKT_LOAD_ERR,
    PCKT_SAVE_ERR,
    PCKT_SEND_ERR,
    PCKT_RECV_ERR,
    APP_NUM_ARGS_ERR,
} ERR;

ERR error = NO_ERR;

/* FILE */

FILE* file = NULL;

OPSTAT FILE_open(char const* filename, char const* mode);
void FILE_clean();

/* PCKT */

#define MAX_DATA_SIZE 2

typedef struct packet_s {
    int id;
    int data_size;
    char data[MAX_DATA_SIZE];
} PCKT;

int pckt_cnt = 0;
PCKT pckt_buff;
void PCKT_print(PCKT const* pckt);

OPSTAT PCKT_load();
OPSTAT PCKT_save();
OPSTAT PCKT_recv();
OPSTAT PCKT_send();

/* NET */

FILE* net = NULL;
int sock = -1;

OPSTAT NET_put_tries(struct addrinfo** tries, char const* host, char const* port);
OPSTAT NET_getnameinfo(struct sockaddr const* addr, socklen_t const* len,
                       char* host, int hsize, char* serv, int ssize);

OPSTAT NET_connect(char const* host, char const* port);
OPSTAT NET_bind(char const* port);
OPSTAT NET_accept();

OPSTAT NET_send_bytes(char* bytes, size_t cnt);
OPSTAT NET_recv_bytes(char* bytes, size_t cnt);

void NET_clean();

OPSTAT NET_recv_packet(PCKT* packet);
OPSTAT NET_send_packet(PCKT const* packet);

OPSTAT NET_recv_ack(int* packet_id);
OPSTAT NET_send_ack(int packet_id);

/* CTX */

int pckt_window_cnt = 0;

OPSTAT CTX_init(int argc, char* argv[]);
void CTX_print();
void CTX_clean();

/* MAIN */

enum srv_args {
    SRV_ARG_PORT = 1,
    SRV_ARG_FILE = 2,
};

enum clt_args {
    CLT_ARG_HOST = 1,
    CLT_ARG_PORT = 2,
    CLT_ARG_FILE = 3,
};

#define NUM_SRV_ARGS 3
#define NUM_CLT_ARGS 4

int main(int argc, char* argv[]);

/* DEFINITIONS */

void PCKT_print(PCKT const* pckt) {
    int bytes;
    printf("Packet #%d with %d bytes:\n", pckt->id, pckt->data_size);
    printf("'");
    for (bytes = 0; bytes < pckt->data_size; bytes++)
       printf("%c", pckt->data[bytes]);
    printf("'\n");
}

OPSTAT PCKT_load() {
    int bytes_read_now = 0;
    if (feof(file))
        return FAIL;
    bytes_read_now = fread(&pckt_buff.data, sizeof(*(pckt_buff.data)), MAX_DATA_SIZE, file);
    pckt_buff.data_size = bytes_read_now;
    pckt_buff.id = pckt_cnt;
    pckt_cnt++;
    if (bytes_read_now == MAX_DATA_SIZE
        || feof(file))
        return SUCCESS;
    error = PCKT_LOAD_ERR;
    return FAIL;
}

OPSTAT PCKT_send() {
    int ack_id;
    while (NET_send_packet(&pckt_buff)
             && NET_recv_ack(&ack_id)
             && ack_id != pckt_buff.id);
    if (error == NET_CONN_ERR) {
        error = PCKT_SEND_ERR;
        return FAIL;
    }
    return SUCCESS;
}

OPSTAT PCKT_recv() {
    if (pckt_cnt > 0 && pckt_buff.data_size < MAX_DATA_SIZE)
        return FAIL;
    while (NET_recv_packet(&pckt_buff)
             && pckt_cnt != pckt_buff.id
             && NET_send_ack(pckt_buff.id));
    if (error == NET_CONN_ERR 
        || !NET_send_ack(pckt_cnt)) {
        error = PCKT_RECV_ERR;
        return FAIL;
    }
    pckt_cnt++;
    return SUCCESS;
}

OPSTAT PCKT_save() {
    int bytes_written_now;
    bytes_written_now = fwrite(&pckt_buff.data, sizeof(*(pckt_buff.data)), pckt_buff.data_size, file);
    if (bytes_written_now != pckt_buff.data_size) {
        error = PCKT_SAVE_ERR;
        return FAIL;
    }
    return SUCCESS;
}

// OPSTAT NET_bind(char const* port) {
//     net = fopen("bin/net", "r");
//     if (!net) {
//         error = NET_BIND_ERR;
//         return FAIL;
//     }
//     return SUCCESS;
// }

OPSTAT NET_send_packet(PCKT const* packet) {
    if (!NET_send_bytes((char*)packet, sizeof(*packet))) {
        error = NET_CONN_ERR;
        return FAIL;
    }
    return SUCCESS;
}

OPSTAT NET_recv_packet(PCKT* packet) {
    if (!NET_recv_bytes((char*)packet, sizeof(*packet))) {
        error = NET_CONN_ERR;
        return FAIL;        
    }
    PCKT_print(packet);
    return SUCCESS;
}

OPSTAT NET_recv_ack(int* packet_id) {
    // if (!fread(packet_id, sizeof(*packet_id), 1, net)) {
    //     error = NET_CONN_ERR;
    //     return FAIL;
    // }
    return RND_opt_stat();
}

OPSTAT NET_send_ack(int packet_id) {
    // if (!fwrite(&packet_id, sizeof(packet_id), 1, net)) {
    //     error = NET_CONN_ERR;
    //     return FAIL;        
    // }
    return SUCCESS;
}

OPSTAT NET_send_bytes(char* bytes, size_t cnt) {
    size_t bytes_sent = 0, bytes_sent_now;
    while (bytes_sent != cnt) {
        do {
           bytes_sent_now = write(sock, bytes + bytes_sent, cnt - bytes_sent);
        } while (bytes_sent_now == -1 && errno == EINTR);
        if (bytes_sent_now == -1)
            return FAIL;
        bytes_sent += bytes_sent_now;
    }
    return SUCCESS;
}

OPSTAT NET_recv_bytes(char* bytes, size_t cnt) {
    size_t bytes_read = 0, bytes_read_now;
    while (bytes_read != cnt) {
        do {
            bytes_read_now = read(sock, bytes + bytes_read, cnt - bytes_read);
        } while ((bytes_read_now == -1) && (errno == EINTR));
        if (bytes_read_now == -1)
            return FAIL;
        bytes_read += bytes_read_now;
    }
    return SUCCESS;
}

OPSTAT NET_put_tries(struct addrinfo** tries, char const* host, char const* port) {
    struct addrinfo hints;

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_socktype = SOCK_DGRAM;  
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = IPPROTO_UDP;

    if (getaddrinfo(host, port, &hints, tries) != 0)
        return FAIL;
    return SUCCESS;
}

OPSTAT NET_getnameinfo(struct sockaddr const* addr, socklen_t const* len,
                       char* host, int hsize, char* serv, int ssize) {
    if (getnameinfo(addr, *len, host, hsize, serv, ssize, NI_NUMERICHOST) != 0)
        return FAIL;
    return SUCCESS;
}

OPSTAT NET_connect(char const* host, char const* port) {
    struct addrinfo* tries = NULL,* try;
    char host_info[NI_MAXHOST];
  
    if (!NET_put_tries(&tries, host, port)) {
        error = NET_CONN_ERR;
        return FAIL;
    }
    for (try = tries; try != NULL; try = try->ai_next) {
        sock = socket(try->ai_family, try->ai_socktype, try->ai_protocol);
        if (sock == -1)
            continue;
        if (connect(sock, try->ai_addr, try->ai_addrlen) != -1)
            break;                  
        close(sock);
    }
    if (!try || !NET_getnameinfo(try->ai_addr, &try->ai_addrlen, host_info, sizeof(host_info), NULL, 0)) {
        error = NET_CONN_ERR;
        return FAIL;
    }
    printf("Connected to %s\n", host_info);
    freeaddrinfo(tries);

    RND_init();
    net = fopen("bin/net", "w");
    if (!net) {
        error = NET_CONN_ERR;
        return FAIL;        
    }
    return SUCCESS;
}

OPSTAT NET_accept() {
    int backlog = 3;
    
    if (listen(sock, backlog) == -1) {
        error = NET_ACCPT_ERR;
        return FAIL;
    }
    struct sockaddr client;
    socklen_t size = sizeof(client);

    sock = accept(sock, &client, &size);
    
    char host[NI_MAXHOST], serv[NI_MAXSERV];
    
    if (sock == -1 || !NET_getnameinfo(&client, &size, host, sizeof(host), serv, sizeof(serv))) {
        return FAIL;
    }
    printf("Server: connect from host %s, port %s.\n", host, serv);
    return SUCCESS;
}

OPSTAT NET_bind(char const* port) {
    struct addrinfo* tries = NULL, * try;
  
    if (!NET_put_tries(&tries, NULL, port)) {
        error = NET_BIND_ERR;
        return FAIL;
    }

    for (try = tries; try != NULL; try = try->ai_next) {
        sock = socket(try->ai_family, try->ai_socktype, try->ai_protocol);
        if (sock == -1)
            continue;
        if (bind(sock, try->ai_addr, try->ai_addrlen) == 0) {
            printf("Bound to port %s\n", port);
            break;                  
        }
        close(sock);
    }
    if (try == NULL) {
        error = NET_BIND_ERR;
        return FAIL;
    }
    freeaddrinfo(tries);
    return SUCCESS;
}

void NET_clean() {
    if (net) {
        fclose(net);
        net = NULL;
    }
    close(sock);
}

OPSTAT FILE_open(char const* filename, char const* mode) {
    file = fopen(filename, mode);
    if (!file) {
        error = FILE_OPEN_ERR;
        return FAIL;
    }
    return SUCCESS;
}

void FILE_clean() {
    if (file) {
        fclose(file);
        file = NULL;
    }
}

OPSTAT CTX_init(int argc, char* argv[]) {
    switch (argc) {
        case NUM_CLT_ARGS:
            app_mode = CLT_MODE;
            return FILE_open(argv[CLT_ARG_FILE], "r") 
                   && NET_connect(argv[CLT_ARG_HOST], argv[CLT_ARG_PORT]);
        case NUM_SRV_ARGS:
            app_mode = SRV_MODE;
            return FILE_open(argv[SRV_ARG_FILE], "w")
                   && NET_bind(argv[SRV_ARG_PORT]);
        default: 
            error = APP_NUM_ARGS_ERR;
            return FAIL;
    }
}

void CTX_print() {
    switch (error) {
        case NO_ERR:
            printf("Finished without errors");
            break;
        case NET_CONN_ERR:
            printf("Failed to connect");
            break;
        case NET_BIND_ERR:
            printf("Failed to bind");
            break;
        case NET_ACCPT_ERR: 
            printf("Failed to accept");
            break;
        case FILE_OPEN_ERR:
            printf("Failed to open file");
            break;
        case PCKT_LOAD_ERR:
            printf("Failed to load packet window");
            break;
        case PCKT_SAVE_ERR:
            printf("Failed to save packet window");
            break;
        case PCKT_SEND_ERR:
            printf("Failed to send packet window");
            break;
        case PCKT_RECV_ERR:
            printf("Failed to receive packet window");
            break;
        case APP_NUM_ARGS_ERR:
            printf("Wrong number of arguments");
            break;
    }
    printf("\n");
}
void CTX_clean() {
    FILE_clean();
    NET_clean();
}

int main(int argc, char* argv[]) {
    if (CTX_init(argc, argv)) {
        switch (app_mode) {
            case CLT_MODE:
                while (PCKT_load()
                       && PCKT_send()) {
                    pckt_window_cnt++;
                }
                break;
            case SRV_MODE:
                while (PCKT_recv()
                       && PCKT_save()) {
                    pckt_window_cnt++;
                }
                break;
            default:
                assert(0);
                break;
        }
        printf("Processed %d packet windows\n", pckt_window_cnt);
    }
    CTX_print();
    CTX_clean();
    return 0;
}