#ifndef OTA_H
#define OTA_H

#include "stdint.h"

#include "main.h"

#define OTA_MESSAGE_BUFFER_SIZE 300

#define OTA_FIX_HEAD_FIRST_BYTE 0xAA
#define OTA_FIX_HEAD_SECOND_BYTE 0x55

#define MAIN_BOARD_ID 0x00
#define PRODUCT_TYPE 0x00
#define IC_TYPE 0x00
#define HW_VERSION 0x01
#define ALL_BOARD 0x00

#ifdef __cplusplus
extern "C"
{
#endif

    enum OTA_RECEIVE_STATE
    {
        OTA_STATE_RECV_FIX_FIRST = 0,
        OTA_STATE_RECV_FIX_SECOND,
        OTA_STATE_RECV_VERSION,
        OTA_STATE_RECV_ID,
        OTA_STATE_RECV_LEN_LOW,
        OTA_STATE_RECV_LEN_HIGH,
        OTA_STATE_RECV_DATA,
        OTA_STATE_RECV_CHECK,
        OTA_STATE_RENAME_RESPONSE,
    };

    enum OTA_MESSAGE_ID
    {
        OTA_ID_HEARTBEAT = 0,
        OTA_ID_GET_INFO = 0x01,
        OTA_ID_START_UPDATE = 0x0A,
        OTA_ID_UPDATE,
        OTA_ID_GET_STATUS,
        OTA_ID_UPDATE_OVER,
        OTA_ID_RESET,
        OTA_ID_GET_VERSION,
        OTA_ID_MESSGAE_MAX,
    };

    enum OTA_STATUS
    {
        OTA_STATUS_NOT_STARTED = 0x00,
        OTA_STATUS_STARTED,
        OTA_STATUS_UPDATING,
        OTA_STATUS_SUCCESS,
        OTA_STATUS_FAIL,
        OTA_STATUS_NOT_STARTED_IN_BOOT,
    };

    struct OTA_head
    {
        uint8_t flag_first;
        uint8_t flag_second;
        uint8_t ver;
        uint8_t msg_id;
        uint16_t length;
    };

    struct OTA_message
    {
        struct OTA_head head;
        uint8_t data[OTA_MESSAGE_BUFFER_SIZE];
        uint8_t check;
        uint8_t recv_count;
    };

    void OTA_Data_Frame_Receive(uint8_t one_char);
    void OTA_CMD_Parse(void);
    void Send_Response(uint8_t msg_id, uint8_t *data, uint8_t len);
    void Send_Response_Without_ID(uint8_t msg_id, uint8_t *data, uint8_t len);
    uint16_t Calc_CRC16(uint8_t *pucFrame, uint32_t usLen);

#ifdef __cplusplus
}
#endif

#endif /* OTA_H */