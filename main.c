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

int sock = -1;

/* for server needs */
typedef struct peer_info {
    /* initialised on first successful receive */
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];
    /* updated after each receive and checked with host, serv */
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len;
} PEER_INFO;

PEER_INFO* peer_info = NULL;

void PEER_INFO_init(char const* host, char const* serv);
OPSTAT PEER_INFO_verify(char const* host, char const* serv);

OPSTAT NET_put_tries(struct addrinfo** tries, char const* host, char const* port);
OPSTAT NET_getnameinfo(struct sockaddr const* addr, socklen_t const* len,
                       char* host, int hsize, char* serv, int ssize);

OPSTAT NET_connect(char const* host, char const* port);
OPSTAT NET_bind(char const* port);

OPSTAT NET_send_bytes(char* bytes, size_t cnt);
OPSTAT NET_recv_bytes(char* bytes, size_t cnt);
OPSTAT NET_recv_from(char* bytes, size_t cnt);
OPSTAT NET_send_to(char* bytes, size_t cnt);

void NET_clean();

OPSTAT NET_recv_packet(PCKT* packet);
OPSTAT NET_send_packet(PCKT const* packet);

OPSTAT NET_recv_ack(int* packet_id);
OPSTAT NET_send_ack(int packet_id);

/* CTX */

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
    if (bytes_read_now == MAX_DATA_SIZE
        || feof(file))
        return SUCCESS;
    error = PCKT_LOAD_ERR;
    return FAIL;
}

OPSTAT PCKT_send() {
    int ack_id;
    while (NET_send_packet(&pckt_buff)
             && !(NET_recv_ack(&ack_id) && ack_id == pckt_buff.id));
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

OPSTAT NET_send_packet(PCKT const* packet) {
    if (!NET_send_bytes((char*)packet, sizeof(*packet))) {
        error = NET_CONN_ERR;
        return FAIL;
    }
    return SUCCESS;
}

OPSTAT NET_recv_packet(PCKT* packet) {
    if (!NET_recv_from((char*)packet, sizeof(*packet))) {
        error = NET_CONN_ERR;
        return FAIL;        
    }
    PCKT_print(packet);
    return SUCCESS;
}

OPSTAT NET_recv_ack(int* packet_id) {
    if (!NET_recv_bytes((char*)packet_id, sizeof(*packet_id))) {
        error = NET_CONN_ERR;
        return FAIL;
    }
    return SUCCESS;
}

OPSTAT NET_send_ack(int packet_id) {
    if (!NET_send_to((char*)&packet_id, sizeof(packet_id))) {
        error = NET_CONN_ERR;
        return FAIL;
    }
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

OPSTAT NET_recv_from(char* bytes, size_t cnt) {
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len = sizeof(peer_addr);
    char host[NI_MAXHOST], serv[NI_MAXSERV];

    size_t bytes_read = 0, bytes_read_now;
    while (bytes_read != cnt) {
        do {
            bytes_read_now = recvfrom(sock, bytes + bytes_read, cnt - bytes_read, 0,
                                      (struct sockaddr*) &peer_addr, &peer_addr_len);
        } while ((bytes_read_now == -1) && (errno == EINTR));
        if (bytes_read_now == -1 
            || !NET_getnameinfo((struct sockaddr *) &peer_addr, &peer_addr_len, 
                                host, NI_MAXHOST, serv, NI_MAXSERV))
            return FAIL;
        if (peer_info == NULL)
            PEER_INFO_init(host, serv);
        else if (!PEER_INFO_verify(host, serv))
            return FAIL;
        bytes_read += bytes_read_now;
    }
    memcpy(&peer_info->peer_addr, &peer_addr, sizeof(peer_addr));
    memcpy(&peer_info->peer_addr_len, &peer_addr_len, sizeof(peer_addr_len));
    return SUCCESS;
}

OPSTAT NET_send_to(char* bytes, size_t cnt) {
    if (!peer_info)
        return FAIL;
    size_t bytes_sent = 0, bytes_sent_now;
    while (bytes_sent != cnt) {
        do {
            bytes_sent_now = sendto(sock, bytes + bytes_sent, cnt - bytes_sent, 0,
                                      (struct sockaddr*) &peer_info->peer_addr, 
                                      peer_info->peer_addr_len);
        } while ((bytes_sent_now == -1) && (errno == EINTR));
        if (bytes_sent_now == -1)
            return FAIL;
        bytes_sent += bytes_sent_now;
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
    if (peer_info) {
        free(peer_info);
        peer_info = NULL;
    }
    if (sock != -1)
        close(sock);
}

void PEER_INFO_init(char const* host, char const* serv) {
    peer_info = malloc(sizeof(PEER_INFO));
    strcpy(peer_info->host, host);
    strcpy(peer_info->serv, serv);
}

OPSTAT PEER_INFO_verify(char const* host, char const* serv) {
    if (!peer_info)
        return FAIL;
    if (strcmp(peer_info->host, host) != 0
        || strcmp(peer_info->serv, serv) != 0)
        return FAIL;
    return SUCCESS;
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
                       && PCKT_send())
                    pckt_cnt++;
                break;
            case SRV_MODE:
                while (PCKT_recv()
                       && PCKT_save())
                    pckt_cnt++;
                if (peer_info) {
                    printf("Host: %s\n", peer_info->host);
                    printf("Service: %s\n", peer_info->serv);
                }
                break;
            default:
                assert(0);
                break;
        }
        printf("Processed %d packets\n", pckt_cnt);
    }
    CTX_print();
    CTX_clean();
    return 0;
}