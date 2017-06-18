#include <stdio.h>
#include <assert.h>
#include <string.h>

typedef enum opstat_e {
    FAIL = 0,
    SUCCESS = 1
} OPSTAT;

/* ERR */

typedef enum error_e {
    NO_ERR,
    NET_CONN_ERR,
    NET_BIND_ERR,
    FILE_OPEN_ERR,
    PCKT_WND_LOAD_ERR,
    PCKT_WND_SAVE_ERR,
    PCKT_WND_SEND_ERR,
    PCKT_WND_RECV_ERR,
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

/* PCKT_WND interface */

#define PCKT_WND_SIZE 2

int SRV_pckt_cnt = 0;
PCKT SRV_pckt_wnd[PCKT_WND_SIZE];

int CLT_pckt_cnt = 0;
PCKT CLT_pckt_wnd[PCKT_WND_SIZE];

OPSTAT PCKT_WND_load();
OPSTAT PCKT_WND_save();
OPSTAT PCKT_WND_recv();
OPSTAT PCKT_WND_send();

/* NET */

OPSTAT NET_connect(char const* host, char const* port);
OPSTAT NET_bind(char const* port);
void NET_clean();

void NET_recv_packet(PCKT* packet);
OPSTAT NET_send_packet(PCKT const* packet);

void NET_recv_ack(int* packet_id);
void NET_send_ack(int packet_id);

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

OPSTAT PCKT_WND_load() {
    CLT_pckt_cnt = 0;
    int bytes_read_now = 0;
    if (feof(file))
        return FAIL;
    do {
        PCKT* pckt = &CLT_pckt_wnd[CLT_pckt_cnt];
        bytes_read_now = fread(&pckt->data, sizeof(*(pckt->data)), MAX_DATA_SIZE, file);
        pckt->data_size = bytes_read_now;
        pckt->id = CLT_pckt_cnt;
        CLT_pckt_cnt++;
    } while (CLT_pckt_cnt < PCKT_WND_SIZE 
           && bytes_read_now == MAX_DATA_SIZE);
    if ((CLT_pckt_cnt == PCKT_WND_SIZE && bytes_read_now == MAX_DATA_SIZE)
        || feof(file))
        return SUCCESS;
    error = PCKT_WND_LOAD_ERR;
    return FAIL;
}

OPSTAT PCKT_WND_send() {
    SRV_pckt_cnt = CLT_pckt_cnt;
    int i;
    for (i = 0; i < CLT_pckt_cnt; i++) {
        if (!NET_send_packet(&CLT_pckt_wnd[i]))
            return FAIL;
    }
    return SUCCESS;
}

OPSTAT PCKT_WND_recv() {
    return SUCCESS;
}

OPSTAT PCKT_WND_save() {
    int i, bytes;
    printf("Packet cnt: %d\n", SRV_pckt_cnt);
    for (i = 0; i < SRV_pckt_cnt; i++) {
        printf("Packet #%d with %d bytes:\n", i, SRV_pckt_wnd[i].data_size);
        printf("'");
        for (bytes = 0; bytes < SRV_pckt_wnd[i].data_size; bytes++)
            printf("%c", SRV_pckt_wnd[i].data[bytes]);
        printf("'\n");
    }
    return SUCCESS;
}

OPSTAT NET_connect(char const* host, char const* port) {
    return SUCCESS;   
}

OPSTAT NET_bind(char const* port) {
    error = NET_BIND_ERR;
    return FAIL;
}

OPSTAT NET_send_packet(PCKT const* packet) {
    memcpy(&SRV_pckt_wnd[packet->id], packet, sizeof(*packet));
    return SUCCESS;
}

void NET_clean() {}

OPSTAT FILE_open(char const* filename, char const* mode) {
    file = fopen(filename, mode);
    if (!file) {
        error = FILE_OPEN_ERR;
        return FAIL;
    }
    return SUCCESS;
}

void FILE_clean() {
    if (!file)
        return;
    fclose(file);
    file = NULL;
}

OPSTAT CTX_init(int argc, char* argv[]) {
    switch (argc) {
        case NUM_CLT_ARGS:
            return FILE_open(argv[CLT_ARG_FILE], "r") 
                   && NET_connect(argv[CLT_ARG_HOST], argv[CLT_ARG_PORT]);
        case NUM_SRV_ARGS:
            return FILE_open(argv[SRV_ARG_FILE], "a")
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
        case FILE_OPEN_ERR:
            printf("Failed to open file");
            break;
        case PCKT_WND_LOAD_ERR:
            printf("Failed to load packet window");
            break;
        case PCKT_WND_SAVE_ERR:
            printf("Failed to save packet window");
            break;
        case PCKT_WND_SEND_ERR:
            printf("Failed to send packet window");
            break;
        case PCKT_WND_RECV_ERR:
            printf("Failed to receive packet window");
            break;
        case APP_NUM_ARGS_ERR:
            printf("Wrong number of arguments");
            break;
        default: 
            assert(0);
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
        while (PCKT_WND_load()
               && PCKT_WND_send()
               && PCKT_WND_recv()
               && PCKT_WND_save())
            pckt_window_cnt++;
        printf("Processed %d packet windows\n", pckt_window_cnt);
    }
    CTX_print();
    CTX_clean();
    return 0;
}