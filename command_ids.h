#ifndef COMMAND_IDS_H
#define COMMAND_IDS_H 1

#define CMD_STOP_PERSISTENCE  0x80
#define CMD_START_PERSISTENCE 0x81
#define CMD_SET_FLUSH_PARAM   0x82

#define CMD_START_REPLICATION 0x90
#define CMD_STOP_REPLICATION  0x91
#define CMD_SET_TAP_PARAM     0x92
#define CMD_EVICT_KEY         0x93
#define CMD_GET_LOCKED        0x94
#define CMD_UNLOCK_KEY        0x95

typedef protocol_binary_request_set protocol_binary_request_getl;

#endif /* COMMAND_IDS_H */
