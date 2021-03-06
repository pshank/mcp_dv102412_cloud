/*****************************************************************************
*
*  Exosite_Demo.c - Exosite cloud applications.
*  Copyright (C) 2013 Exosite LLC
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*    Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*
*    Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the   
*    distribution.
*
*    Neither the name of Exosite LLC nor the names of its contributors may
*    be used to endorse or promote products derived from this software
*    without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
*  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
*  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
*  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
*  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
*  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
*  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
*  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*****************************************************************************/
#include "../../exosite/exosite.h"
#include "MainDemo.h"
#include "Exosite_Demo.h"

//local defines
int unknown_status = 0;
int demoApp_timer = 3;

// local functions
int button_monitor(void);
int read_command(void);
int heart_beat_report(void);
int update_dev_ip(void);
void status_led1_handler(int count);
int status_led2_handler(int count, int delay);
void show_cloud_status(void);
int wifi_fw_detect(void);
int task_delay(int delay_time, int count);
int check_cloud_activated(void);

// externs

// global variables


/*****************************************************************************
*
*  demo_TickInit
*
*  \param  None
*
*  \return  None
*
*  \brief  Demo timer tick init
*
*****************************************************************************/
void demo_TickInit(void)
{
  // Use Timer 1 for 16-bit and 32-bit processors
  // 1:256 prescale
  T2CONbits.TCKPS = 3;
  // Base
  PR2 = 0xFFFF;
  // Clear counter
  TMR2 = 0;

  // Enable timer interrupt
  IPC2bits.T2IP = 2;	// Interrupt priority 2 (low)
  IFS0CLR = _IFS0_T2IF_MASK;
  IEC0SET = _IEC0_T2IE_MASK;

  // Start timer
  T2CONbits.TON = 1;
}


/*****************************************************************************
*
*  demo_tick_handler
*
*  \param  None
*
*  \return  None
*
*  \brief  Demo timer tick handler
*
*****************************************************************************/
void demo_tick_handler(void)
{
  if (cloud_mode < 1)
    return;

  if (led2_blinky_count > 0 && led2_count_rate > 0)
  {
    show_next = 0;
    if (demo_tick % (SEC_RATE / led2_count_rate) == 0)
    {
      LED2_INV();
      led2_blinky_count --;
    }
  } else if (led2_blinky_count == 0)
    if (demo_tick % (SEC_RATE * 2) == 0)
      show_next = 1;
}


/*****************************************************************************
*
*  _T2Interrupt
*
*  \param  None
*
*  \return  None
*
*  \brief  Timer 2 Interrupt
*
*****************************************************************************/
void __attribute((interrupt(ipl2), vector(_TIMER_2_VECTOR), nomips16)) _T2Interrupt(void)
{
  // Increment internal high tick counter
  demo_tick++;
  demo_tick_handler();

  // Reset interrupt flag
  IFS0CLR = _IFS0_T2IF_MASK;
}


/*****************************************************************************
*
*  task_delay
*
*  \param  None
*
*  \return  delay counting is done
*
*  \brief  Demo tasks delay function
*
*****************************************************************************/
int task_delay(int delay_time, int count)
{
  if (TickGet() - demo_delay_time_tick > delay_time)
  {
    demo_delay_count++;
    demo_delay_time_tick = TickGet();
  }

  if (count <= demo_delay_count)
  {
    demo_delay_count = 0;
    return 1;
  }
  else
    return 0;
}


/*****************************************************************************
*
*  Erase_App_Config
*
*  \param  None
*
*  \return  None
*
*  \brief  Erases the AppConfig in internal flash
*
*****************************************************************************/
void Erase_App_Config(void)
{
  char tmpCIK[40];
  char backupCIK = 0;
  LEDS_OFF();
  DelayMs(150);
  // 1) backup CIK, 2) erases Exosite meta, 3) Set CIK back
  if (Exosite_GetCIK(tmpCIK))
    backupCIK = 1;

  Exosite_Init("microchip", "dv102412", IF_WIFI, 1);

  if (backupCIK == 1)
    Exosite_SetCIK(tmpCIK);

  // blinking 3 times per second
  status_led1_handler(3);

  Reset();
  return;
}


/*****************************************************************************
*
*  Store_App_Config
*
*  \param  None
*
*  \return  None
*
*  \brief  Stores the AppConfig into internal flash
*
*****************************************************************************/
void Store_App_Config(void)
{
  _store_ap_temp ap_temp;
  _store_ap_temp ap_temp_verify;
  memcpy(ap_temp.MySSID, AppConfig.MySSID, 32);
  memcpy(ap_temp.SecurityKey, AppConfig.SecurityKey, 64);
  ap_temp.SecurityKeyLength = AppConfig.SecurityKeyLength;
  ap_temp.SecurityMode = AppConfig.SecurityMode;
  ap_temp.dataValid = AppConfig.dataValid;
  ap_temp.networkType = AppConfig.networkType;
  ap_temp.saveSecurityInfo = AppConfig.saveSecurityInfo;

  Exosite_GetMRF((char *)&ap_temp_verify, sizeof(_store_ap_temp));
  if (strncmp((char *)ap_temp_verify.MySSID, (char *)ap_temp.MySSID, 32) &&
     strncmp((char *)ap_temp_verify.SecurityKey, (char *)ap_temp.SecurityKey, 64))
  {
    char tmpCIK[40];
    char backupCIK = 0;
    LEDS_OFF();

    // 1) backup CIK, 2) erases Exosite meta, 3) Set CIK back, 4) Set WiFi config back
    if (Exosite_GetCIK(tmpCIK))
      backupCIK = 1;

    Exosite_Init("microchip", "dv102412", IF_WIFI, 1);

    if (backupCIK == 1)
      Exosite_SetCIK(tmpCIK);

    Exosite_SetMRF((char *)&ap_temp, sizeof(_store_ap_temp));

    setup_wifi = 1;
    // blinking twice second
    status_led1_handler(2);

    Reset();
  }
  return;
}


/*****************************************************************************
*
*  Load_App_Config
*
*  \param  None
*
*  \return  None
*
*  \brief  Copies the AppConfig from internal flash
*
*****************************************************************************/
void Load_App_Config(void)
{
  _store_ap_temp temp;

  Exosite_GetMRF((char *)&temp, sizeof(_store_ap_temp));
  if (temp.MySSID[0] == 0x00 || temp.MySSID[0] == 0xff)
    return;

  if (temp.networkType == CFG_WF_INFRASTRUCTURE && temp.dataValid == 1)
  {
    memcpy(AppConfig.MySSID, temp.MySSID, 32);
    memcpy(AppConfig.SecurityKey, temp.SecurityKey, 64);
    AppConfig.SecurityKeyLength = temp.SecurityKeyLength;
    AppConfig.SecurityMode = temp.SecurityMode;
    AppConfig.dataValid = temp.dataValid;
    AppConfig.networkType = temp.networkType;
    AppConfig.saveSecurityInfo = 0;
    CFGCXT.security = temp.SecurityMode;
    memcpy(CFGCXT.key, temp.SecurityKey, 64);
    memcpy(CFGCXT.ssid, temp.MySSID, 32);
    CFGCXT.type = temp.networkType;
    CFGCXT.cfg_state = cfg_stopped;

    // re-config the WiFi Access Point
    WF_START_EASY_CONFIG();
  }

  return;
}


/*****************************************************************************
*
*  button_monitor
*
*  \param  None
*
*  \return  0 = working, 1 = not working
*
*  \brief  Monitors the button status
*
*****************************************************************************/
int button_monitor(void)
{
  if (button_state == 0 && once_update == 0)
    return 1;

  if (button_state == 1)
  {
    char inbuf_01[50];
    sprintf(inbuf_01, "push_button=%d", button_state);
    if(Exosite_Write(inbuf_01, strlen(inbuf_01)))
    {
      once_update = 1;
      button_state = 0;

      return 0;
    }
  }

  if (button_state == 0 && once_update == 1)
  {
    char inbuf_02[50];

    sprintf(inbuf_02, "push_button=%d", button_state);
    if(Exosite_Write(inbuf_02, strlen(inbuf_02)))
    {
      once_update = 0;

      return 1;
    }
  }

  return 0;
}


/*****************************************************************************
*
*  read_command
*
*  \param  None
*
*  \return  1 = Read command done, 0 = Reading ...
*
*  \brief  Reads command from cloud
*
*****************************************************************************/
int read_command(void)
{
  char pbuf[50];

  // Read LED status from cloud
  if(Exosite_Read("led", pbuf, 200))
  {
    if (!strncmp(pbuf, "0", 1))
      LED1_IO = 0;
    else if (!strncmp(pbuf, "1", 1))
      LED1_IO = 1;

    return 1;
  }

  return 0;
}


/*****************************************************************************
*
*  heart_beat_report
*
*  \param  None
*
*  \return  1 If reported is done, 0 If not done
*
*  \brief  Posts ping value to cloud
*
*****************************************************************************/
int heart_beat_report(void)
{
  char str_heartbeat[50];

  sprintf(str_heartbeat, "ping=%d", ping);
  if(Exosite_Write(str_heartbeat,strlen(str_heartbeat)))
  {
    ping++;
    if (ping == 100)
      ping = 0;

    return 1;
  }

  return 0;
}


/*****************************************************************************
*
*  update_dev_ip
*
*  \param  None
*
*  \return  1 - successful, 0 - failure
*
*  \brief  Updates the DHCP IP of device
*
*****************************************************************************/
int update_dev_ip(void)
{
  char str_ip[50];

  sprintf(str_ip, "local_ip=%d.%d.%d.%d&ssid=%s", AppConfig.MyIPAddr.v[0], AppConfig.MyIPAddr.v[1], AppConfig.MyIPAddr.v[2], AppConfig.MyIPAddr.v[3], AppConfig.MySSID);
  if(Exosite_Write(str_ip, strlen(str_ip)))
  {
    return 1;
  }

  return 0;
}


/*****************************************************************************
*
*  status_led1_handler
*
*  \param  int count
*
*  \return  None
*
*  \brief  Handles LED1, don't use it when the device in cloud demos.
*
*****************************************************************************/
void status_led1_handler(int count)
{
  int i = 0;

  LED1_IO = 0; //clean LED1 status
  DelayMs(150);

  // blinking
  for (i = 0; i < count; i++)
  {
    LED1_IO = 1;
    DelayMs(100);
    LED1_IO = 0;
    DelayMs(100);
  }

  DelayMs(150);

  return;
}


/*****************************************************************************
*
*  show_cloud_status
*
*  \param  None
*
*  \return  None
*
*  \brief  Indicates cloud status per second
*
*****************************************************************************/
void show_cloud_status(void)
{
  if (show_next == 0)
    return;

  latest_exo_code = Exosite_StatusCode();

  if (EXO_STATUS_OK == latest_exo_code)
  {
    led2_blinky_count = 1 * 2;
    tcp_fail_count = 0;
    unknown_status = 0;
  }
  else if (EXO_STATUS_BAD_TCP == latest_exo_code)
  {
    led2_blinky_count = 2 * 2;
    tcp_fail_count++;
  }
  else if (EXO_STATUS_BAD_SN == latest_exo_code)
  {
    led2_blinky_count = 3 * 2;
    tcp_fail_count = 0;
    unknown_status = 0;
  }
  else if (EXO_STATUS_BAD_CIK == latest_exo_code ||
          EXO_STATUS_NOAUTH == latest_exo_code ||
          EXO_STATUS_CONFLICT == latest_exo_code)
  {
    led2_blinky_count = 4 * 2;
    tcp_fail_count = 0;
    unknown_status = 0;
  }
  else
  {
    led2_blinky_count = 0;
    unknown_status++;
  }
  led2_count_rate = led2_blinky_count / 2;

  return;
}


/*****************************************************************************
*
*  wifi_fw_detect
*
*  \param  None
*
*  \return  1 - WiFi Firmware check done, 0 - WiFi Firmware check not done
*
*  \brief  Checks and indicates WiFi firmware version.
*
*****************************************************************************/
int wifi_fw_detect(void)
{
#if defined(MRF24WG)
  int i = 0;
  int fm_cnt = 0;
  if (once_init == 0)
  {
    tWFDeviceInfo deviceInfo;

    LED2_IO = 0;
    WF_GetDeviceInfo(&deviceInfo);  // only call this once, not continually
    if (deviceInfo.romVersion == 0x30)
      fm_cnt = 4;
    else if (deviceInfo.romVersion == 0x31)
      fm_cnt = 8;
    else
      fm_cnt = 12;

    once_init = 1;

    for (i = 0; i < fm_cnt; i++)
    {
      LED2_INV();
      DelayMs(200);
    }
  }
#endif

  return once_init;
}


/*****************************************************************************
*
*  check_network_connected
*
*  \param  None
*
*  \return  TRUE if network ready, FALSE if not
*
*  \brief  Checks to see that network is still connected.
*
*****************************************************************************/
unsigned char
check_network_connected(void)
{
  int network_connected  = 0;
  if (!wifi_fw_detect())
    return 0;

  // loads pre-WiFi config
  if (setup_wifi == 1)
  {
    if (task_delay(TICK_SECOND, 1))
    {
      Load_App_Config();
      setup_wifi = 0;
    }
  }

  if (WFisConnected() != TRUE)
  {
    LED1_IO = 0;
    LED2_IO = 0;
    network_connected = 0;
    if (CFGCXT.type != WF_SOFT_AP && AppConfig.networkType == WF_INFRASTRUCTURE && network_connected == 0)
    {
      if (task_delay(TICK_MINUTE, 1))
        wifi_fail_count++;
    }
  }

  if (DHCPIsBound(0))
    network_connected = 1;
  else
    network_connected = 0;

  if (tcp_fail_count > 5 || unknown_status > 5)
  {
    LED1_IO = 0;
    LED2_IO = 0;
  }
  if (wifi_fail_count > 2)
  {
    wifi_fail_count = 0;
    Erase_App_Config();
    Reset();
  }
  if (tcp_fail_count > 50 || unknown_status > 50)
  {
    tcp_fail_count = 0;
    unknown_status = 0;
    wifi_fail_count = 0;
    network_connected = 0;
    Reset();
  }
  return network_connected;
}


/*****************************************************************************
*
*  check_cloud_activated
*
*  \param  None
*
*  \return  0 - activate fail, 1 - activate success
*
*  \brief  Checks cloud status is activated
*
*****************************************************************************/
int check_cloud_activated(void)
{
  int activate_status = 0;

  if (nvmram_verify != 0)
  {
    // 1) backup WiFi config 2) erases Exosite meta 3) set WiFi config in flash
    _store_ap_temp ap_temp;
    Exosite_GetMRF((char *)&ap_temp, sizeof(_store_ap_temp));
    Exosite_Init("microchip", "dv102412", IF_WIFI, 1);
    Exosite_SetMRF((char *)&ap_temp, sizeof(_store_ap_temp));
    nvmram_verify = 0;
  }

  if (nvmram_verify == 0)
  {
    activate_status = Exosite_Activate();
  }

  return activate_status;
}


/*****************************************************************************
*
*  Exosite_Demo
*
*  \param  None
*
*  \return  None
*
*  \brief  Main function of Exosite demo code
*
*****************************************************************************/
void Exosite_Demo(void)
{
  if (!check_network_connected())
  {
    cloud_mode = 0;
    return;
  }
  cloud_mode = 1;

  // worked on demo app every 1/3 second
  if (!task_delay(TICK_SECOND * 5 / demoApp_timer, 1))
    return;

  int code = Exosite_StatusCode();
  if (EXO_STATUS_BAD_CIK == code || EXO_STATUS_NOAUTH == code || cloud_status == -1)
  {
    if (check_cloud_activated() == 0)
      cloud_status = -1;
    else
      cloud_status = 0;
  }
  else
  {
    if (CLOUD_HOME == cloud_status)
    {
      nvmram_verify = 1; // when you want to provision again, you should verify the nvram
      if (update_dev_ip())
      {
        cloud_status++;
        demoApp_timer = 1;
      }
      else
        demoApp_timer = 5;
    }
    else if (CLOUD_READING == cloud_status)
    {
      if (read_command())
      {
        cloud_status++;
        demoApp_timer = 1;
      }
      else
        demoApp_timer = 5;
    }
    else if (CLOUD_WRITING == cloud_status)
    {
      if (heart_beat_report())
      {
        cloud_status++;
        demoApp_timer = 1;
      }
      else
        demoApp_timer = 5;
    }
    else if (CLOUD_BUTTON_STATUS_UPDATE == cloud_status)
    {
      if (button_monitor())
      {
        cloud_status = CLOUD_READING;
        demoApp_timer = 1;
      }
      else
        demoApp_timer = 5;
    }
  }

  // indicate last status
  show_cloud_status();

  return;
}


