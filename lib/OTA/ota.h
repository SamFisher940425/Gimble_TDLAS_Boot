#ifndef OTA_H
#define OTA_H

#include "stdint.h"

#include "main.h"

#define OTA_MESSAGE_BUFFER_SIZE 256

#define OTA_FRAME_HEAD_1 0x55
#define OTA_FRAME_HEAD_2 0xAA
#define OTA_FRAME_TAIL_1 0x0D
#define OTA_FRAME_TAIL_2 0x0A

#ifdef __cplusplus
extern "C"
{
#endif

    enum OTA_RECEIVE_STATE
    {
        OTA_STATE_RECV_HEAD_1 = 0,
        OTA_STATE_RECV_HEAD_2,
        OTA_STATE_RECV_SRC_ID,
        OTA_STATE_RECV_DST_ID,
        OTA_STATE_RECV_FUNC_CODE,
        OTA_STATE_RECV_DATA_LEN,
        OTA_STATE_RECV_DATA,
        OTA_STATE_RECV_CHECK,
        OTA_STATE_RECV_TAIL_1,
        OTA_STATE_RECV_TAIL_2
    };

    enum OTA_FUNC_CODE
    {
        OTA_FC_BEGIN = 0x10,
        OTA_FC_TRANSMIT,
        OTA_FC_GET_STATE,
        OTA_FC_FINISH,
        OTA_FC_RESET,
        OTA_FUNC_CODE_MAX
    };

    enum OTA_STATUS
    {
        OTA_STATUS_NOT_STARTED = 0x00,
        OTA_STATUS_STARTED,
        OTA_STATUS_UPDATING,
        OTA_STATUS_SUCCESS,
        OTA_STATUS_FAIL,
        OTA_STATUS_NOT_STARTED_IN_BOOT
    };

    struct OTA_head
    {
        uint8_t head_1;
        uint8_t head_2;
        uint8_t src_id;
        uint8_t dst_id;
        uint8_t func_code;
        uint8_t data_len;
    };

    struct OTA_message
    {
        struct OTA_head head;
        uint8_t data[OTA_MESSAGE_BUFFER_SIZE];
        uint8_t check;
        uint8_t tail_1;
        uint8_t tail_2;
    };

    void OTA_Data_Frame_Receive(uint8_t one_char);
    void OTA_CMD_Parse(void);
    void Send_Response(uint8_t msg_id, uint8_t *data, uint8_t len);
    uint16_t Calc_CRC16(uint8_t *pucFrame, uint32_t usLen);

#ifdef __cplusplus
}
#endif

#endif /* OTA_H */