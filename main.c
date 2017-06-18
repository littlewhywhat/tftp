#include <stdio.h>
#include <assert.h>
#include <string.h>

typedef enum app_mode_e {
    CLT_MODE,
    SRV_MODE,
} APP_MODE;

APP_MODE app_mode;

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

void PCKT_print(PCKT const* pckt);

/* PCKT_WND interface */

#define PCKT_WND_SIZE 2

int pckt_cnt = 0;
PCKT pckt_wnd[PCKT_WND_SIZE];

OPSTAT PCKT_WND_load();
OPSTAT PCKT_WND_save();
OPSTAT PCKT_WND_recv();
OPSTAT PCKT_WND_send();

/* NET */

FILE* net = NULL;

OPSTAT NET_connect(char const* host, char const* port);
OPSTAT NET_bind(char const* port);
void NET_clean();

OPSTAT NET_recv_packet(PCKT* packet);
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

void PCKT_print(PCKT const* pckt) {
    int bytes;
    printf("Packet #%d with %d bytes:\n", pckt->id, pckt->data_size);
    printf("'");
    for (bytes = 0; bytes < pckt->data_size; bytes++)
       printf("%c", pckt->data[bytes]);
    printf("'\n");
}

OPSTAT PCKT_WND_load() {
    pckt_cnt = 0;
    int bytes_read_now = 0;
    if (feof(file))
        return FAIL;
    do {
        PCKT* pckt = &pckt_wnd[pckt_cnt];
        bytes_read_now = fread(&pckt->data, sizeof(*(pckt->data)), MAX_DATA_SIZE, file);
        pckt->data_size = bytes_read_now;
        pckt->id = pckt_cnt;
        pckt_cnt++;
    } while (pckt_cnt < PCKT_WND_SIZE 
           && bytes_read_now == MAX_DATA_SIZE);
    if ((pckt_cnt == PCKT_WND_SIZE && bytes_read_now == MAX_DATA_SIZE)
        || feof(file))
        return SUCCESS;
    error = PCKT_WND_LOAD_ERR;
    return FAIL;
}

OPSTAT PCKT_WND_send() {
    fwrite(&pckt_cnt, sizeof(int), 1, net);
    int i;
    for (i = 0; i < pckt_cnt; i++) {
        if (!NET_send_packet(&pckt_wnd[i])) {
            error = PCKT_WND_SEND_ERR;
            return FAIL;
        }
    }
    return SUCCESS;
}

OPSTAT PCKT_WND_recv() {
    fread(&pckt_cnt, sizeof(int), 1, net);
    if (feof(net))
        return FAIL;
    int i;
    for (i = 0; i < pckt_cnt; i++) {
        if (!NET_recv_packet(&pckt_wnd[i])) {
            error = PCKT_WND_RECV_ERR;
            return FAIL;
        }
    }
    return SUCCESS;
}

OPSTAT PCKT_WND_save() {
    int i, bytes_read_now;
    for (i = 0; i < pckt_cnt; i++) {
        PCKT* pckt = &pckt_wnd[i];
        bytes_read_now = fwrite(&pckt->data, sizeof(*(pckt->data)), pckt->data_size, file);
        if (bytes_read_now != pckt->data_size) {
            error = PCKT_WND_SAVE_ERR;
            return FAIL;
        }
    }
    return SUCCESS;
}

OPSTAT NET_connect(char const* host, char const* port) {
    net = fopen("bin/net", "w");
    return SUCCESS;   
}

OPSTAT NET_bind(char const* port) {
    net = fopen("bin/net", "r");
    return SUCCESS;
}

OPSTAT NET_send_packet(PCKT const* packet) {
    fwrite(packet, sizeof(*packet), 1, net);
    return SUCCESS;
}

OPSTAT NET_recv_packet(PCKT* packet) {
    fread(packet, sizeof(*packet), 1, net);
    return SUCCESS;
}

void NET_clean() {
    if (net) {
        fclose(net);
        net = NULL;
    }
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
        switch (app_mode) {
            case CLT_MODE:
                while (PCKT_WND_load()
                       && PCKT_WND_send()) {
                    pckt_window_cnt++;
                }
                break;
            case SRV_MODE:
                while (PCKT_WND_recv()
                       && PCKT_WND_save()) {
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