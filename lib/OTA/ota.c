#include "string.h"

#include "usart.h"
#include "flash.h"

#include "ota.h"

#define UID_BASE 0x1FFFF7E8UL

extern volatile uint32_t g_ota_flag;
extern volatile uint32_t g_fw_size;

static const uint8_t aucCRCHi[], aucCRCLo[];

// firmware and hardware
uint8_t hardware_version[3] = {PRODUCT_TYPE, IC_TYPE, HW_VERSION};
uint8_t firmware_version[3] = {FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR, FIRMWARE_VERSION_REVISION};

enum OTA_STATUS g_ota_status = OTA_STATUS_NOT_STARTED_IN_BOOT;

uint8_t valid_cmd_flag = 0;
uint64_t last_cmd_time = 0;
struct OTA_message active_rx_msg;
struct OTA_message valid_rx_msg;
struct OTA_message send_msg;

void OTA_Data_Frame_Receive(uint8_t one_char)
{
  static enum OTA_RECEIVE_STATE recv_state;

  if (valid_cmd_flag)
    return;

  switch (recv_state)
  {
  case OTA_STATE_RECV_FIX_FIRST:
    if (one_char == OTA_FIX_HEAD_FIRST_BYTE)
    {
      memset(&active_rx_msg, 0, sizeof(active_rx_msg));
      active_rx_msg.head.flag_first = one_char;
      // active_rx_msg.check += one_char;

      recv_state = OTA_STATE_RECV_FIX_SECOND;
    }
    break;
  case OTA_STATE_RECV_FIX_SECOND:
    if (one_char == OTA_FIX_HEAD_SECOND_BYTE)
    {
      active_rx_msg.head.flag_second = one_char;
      // active_rx_msg.check += one_char;

      recv_state = OTA_STATE_RECV_VERSION;
    }
    break;
  case OTA_STATE_RECV_VERSION:
    // if (one_char == FIX_HEAD_SECOND_BYTE)
    //{
    //     active_rx_msg.head.flag_second = one_char;
    //  active_rx_msg.check += one_char;
    recv_state = OTA_STATE_RECV_ID;
    //}
    break;
  case OTA_STATE_RECV_ID:
    if (one_char < OTA_ID_MESSGAE_MAX)
    {
      active_rx_msg.head.msg_id = one_char;
      // active_rx_msg.check += one_char;
      recv_state = OTA_STATE_RECV_LEN_LOW;
    }
    else
      recv_state = OTA_STATE_RECV_FIX_FIRST;
    break;
  case OTA_STATE_RECV_LEN_LOW:
    active_rx_msg.head.length = one_char;
    // active_rx_msg.check += one_char;
    // if (active_rx_msg.head.length == 0)
    //    recv_state = OTA_STATE_RECV_CHECK;
    // else
    recv_state = OTA_STATE_RECV_LEN_HIGH;
    break;
  case OTA_STATE_RECV_LEN_HIGH:
    active_rx_msg.head.length += one_char * 0x100;
    // active_rx_msg.check += one_char;
    if (active_rx_msg.head.length == 0)
      recv_state = OTA_STATE_RECV_CHECK;
    else
      recv_state = OTA_STATE_RECV_DATA;
    break;
  case OTA_STATE_RECV_DATA:
    active_rx_msg.data[active_rx_msg.recv_count++] = one_char;
    // active_rx_msg.check += one_char;
    if (active_rx_msg.recv_count >= active_rx_msg.head.length)
      recv_state = OTA_STATE_RECV_CHECK;
    break;
  case OTA_STATE_RECV_CHECK:
    recv_state = OTA_STATE_RECV_FIX_FIRST;
    active_rx_msg.check = active_rx_msg.head.length + active_rx_msg.head.msg_id + active_rx_msg.head.flag_second + active_rx_msg.head.flag_first;
    for (uint16_t i = 0; i < active_rx_msg.head.length; i++)
    {
      active_rx_msg.check += active_rx_msg.data[i];
    }

    if (active_rx_msg.check == one_char)
    {
      memcpy(&valid_rx_msg, &active_rx_msg, sizeof(valid_rx_msg));
      valid_cmd_flag = 1;
    }
    break;
  default:
    recv_state = OTA_STATE_RECV_FIX_FIRST;
  }
}

void OTA_CMD_Parse(void)
{
  uint32_t data_offset = 0;
  uint32_t data_len = 0;
  int32_t res = -1;
  uint8_t temp_data_byte[18];
  uint16_t crc_calc, crc_get;
  static uint8_t erase_all_flag = 0;

  if (valid_cmd_flag == 0)
  {
    return;
  }
  if ((active_rx_msg.data[0] != MAIN_BOARD_ID) && (valid_rx_msg.head.msg_id != OTA_ID_HEARTBEAT))
  {
    valid_cmd_flag = 0;
    return;
  }

  last_cmd_time = HAL_GetTick();

  switch (valid_rx_msg.head.msg_id)
  {
  case OTA_ID_HEARTBEAT:
    temp_data_byte[0] = ALL_BOARD;
    temp_data_byte[1] = (0x01 << MAIN_BOARD_ID);
    temp_data_byte[2] = 0;
    Send_Response_Without_ID(OTA_ID_HEARTBEAT, temp_data_byte, 3);
    break;
  case OTA_ID_START_UPDATE:
    if (0 == erase_all_flag)
    {
      res = Erase_All_Page();
      erase_all_flag = 1;
    }
    else
    {
      res = 0;
    }
    g_ota_status = OTA_STATUS_NOT_STARTED_IN_BOOT;
    Send_Response(OTA_ID_START_UPDATE, (uint8_t *)&res, 1);
    g_fw_size = *(uint32_t *)&active_rx_msg.data[1];
    break;
  case OTA_ID_UPDATE:
    // update main data
    if (g_ota_status == OTA_STATUS_NOT_STARTED_IN_BOOT)
    {
      g_ota_status = OTA_STATUS_UPDATING;
    }
    data_offset = *(uint32_t *)&active_rx_msg.data[1];
    data_len = active_rx_msg.recv_count - 5;
    if (data_len + data_offset > (Get_Flash_Size() - 16 - 1) * 0x400)
    {
      res = -2;
      g_ota_status = OTA_STATUS_FAIL;
      break;
    }

    // check if data blank or match
    if (!Check_Blank(MAIN_PROGRAM_ADDR + data_offset, data_len))
    {
      if (!Check_Match(MAIN_PROGRAM_ADDR + data_offset, &active_rx_msg.data[5], data_len))
      {
        res = -2;
        g_ota_status = OTA_STATUS_FAIL;
      }
      else
      {
        res = 0;
      }
      erase_all_flag = 0;
    }
    else
    {
      res = Internal_Flash_Program(data_offset, &active_rx_msg.data[5], data_len);
      erase_all_flag = 0;
    }
    Send_Response(OTA_ID_UPDATE, (uint8_t *)&res, 1);
    break;
  case OTA_ID_GET_STATUS:
    Send_Response(OTA_ID_GET_STATUS, (uint8_t *)&g_ota_status, 1);
    break;
  case OTA_ID_UPDATE_OVER:
    // check crc
    crc_calc = Calc_CRC16((uint8_t *)MAIN_PROGRAM_ADDR, g_fw_size - 2);
    crc_get = *(uint16_t *)(MAIN_PROGRAM_ADDR + g_fw_size - 2);
    if (crc_calc == crc_get)
    {
      g_ota_status = OTA_STATUS_SUCCESS;
      // erase flag or not
      // Save_OTA_Flag();
    }
    else
    {
      g_ota_status = OTA_STATUS_FAIL;
    }

    // response
    Send_Response(OTA_ID_UPDATE_OVER, (uint8_t *)&g_ota_status, 1);
    break;

  case OTA_ID_RESET:
    // no data
    Send_Response(OTA_ID_RESET, (uint8_t *)&g_ota_status, 0);
    HAL_Delay(10);

    Jump_To_Main_Application();
    break;
  case OTA_ID_GET_VERSION:
    Send_Response(valid_rx_msg.head.msg_id, (uint8_t *)&temp_data_byte, 0);
    break;
  case OTA_ID_GET_INFO:
    temp_data_byte[0] = hardware_version[0];
    temp_data_byte[1] = hardware_version[1];
    temp_data_byte[2] = hardware_version[2];
    temp_data_byte[3] = firmware_version[0];
    temp_data_byte[4] = firmware_version[1];
    temp_data_byte[5] = firmware_version[2];
    memcpy(temp_data_byte + 6, (uint8_t *)UID_BASE, 12);
    Send_Response(valid_rx_msg.head.msg_id, (uint8_t *)&temp_data_byte, 18);
    break;
  }

  valid_cmd_flag = 0;
}

void Send_Response(uint8_t msg_id, uint8_t *data, uint8_t len)
{
  uint8_t *_tx_buf = (uint8_t *)&send_msg;
  send_msg.head.flag_first = OTA_FIX_HEAD_FIRST_BYTE;
  send_msg.head.flag_second = OTA_FIX_HEAD_SECOND_BYTE;
  send_msg.head.msg_id = msg_id;
  send_msg.head.length = len + 1;
  send_msg.check = 0;
  send_msg.data[0] = MAIN_BOARD_ID;

  if (data != 0 && len != 0)
  {
    memcpy(&send_msg.data[1], data, len);
  }
  for (uint32_t i = 0; i < send_msg.head.length + sizeof(send_msg.head); i++)
  {
    send_msg.check += _tx_buf[i];
  }
  send_msg.data[send_msg.head.length] = send_msg.check;

  HAL_UART_Transmit_DMA(&huart3, (uint8_t *)&send_msg, (sizeof(send_msg.head) + send_msg.head.length + 1));
}

void Send_Response_Without_ID(uint8_t msg_id, uint8_t *data, uint8_t len)
{
  uint8_t *_tx_buf = (uint8_t *)&send_msg;
  send_msg.head.flag_first = OTA_FIX_HEAD_FIRST_BYTE;
  send_msg.head.flag_second = OTA_FIX_HEAD_SECOND_BYTE;
  send_msg.head.msg_id = msg_id;
  send_msg.head.length = len;
  send_msg.check = 0;

  if (data != 0 && len != 0)
  {
    memcpy(&send_msg.data[0], data, len);
  }
  for (uint32_t i = 0; i < send_msg.head.length + sizeof(send_msg.head); i++)
  {
    send_msg.check += _tx_buf[i];
  }
  send_msg.data[send_msg.head.length] = send_msg.check;

  HAL_UART_Transmit_DMA(&huart3, (uint8_t *)&send_msg, (sizeof(send_msg.head) + send_msg.head.length + 1));
}

uint16_t Calc_CRC16(uint8_t *pucFrame, uint32_t usLen)
{
  uint8_t ucCRCHi = 0xFF;
  uint8_t ucCRCLo = 0xFF;
  uint16_t iIndex;
  while (usLen--)
  {
    iIndex = ucCRCLo ^ *(pucFrame++);
    ucCRCLo = (uint8_t)(ucCRCHi ^ aucCRCHi[iIndex]);
    ucCRCHi = aucCRCLo[iIndex];
  }
  return (uint16_t)(ucCRCHi << 8 | ucCRCLo);
}

static const uint8_t aucCRCHi[] = {
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40};

static const uint8_t aucCRCLo[] = {
    0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7,
    0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E,
    0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09, 0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9,
    0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC,
    0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
    0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32,
    0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D,
    0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A, 0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38,
    0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF,
    0x2D, 0xED, 0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
    0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1,
    0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4,
    0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F, 0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB,
    0x69, 0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA,
    0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
    0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0,
    0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97,
    0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C, 0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E,
    0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89,
    0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
    0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83,
    0x41, 0x81, 0x80, 0x40};
