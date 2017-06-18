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
    if (!NET_send_packet(&pckt_buff)) {
        error = PCKT_SEND_ERR;
        return FAIL;
    }
    return SUCCESS;
}

OPSTAT PCKT_recv() {
    if (pckt_cnt != 0 && pckt_buff.data_size < MAX_DATA_SIZE)
        return FAIL;
    if (!NET_recv_packet(&pckt_buff)) {
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

OPSTAT NET_connect(char const* host, char const* port) {
    net = fopen("bin/net", "w");
    if (!net) {
        error = NET_CONN_ERR;
        return FAIL;        
    }
    return SUCCESS;   
}

OPSTAT NET_bind(char const* port) {
    net = fopen("bin/net", "r");
    if (!net) {
        error = NET_BIND_ERR;
        return FAIL;
    }
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