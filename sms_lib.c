#ifndef _SMS_LIB_C_
#define _SMS_LIB_C_
#include "api_sms.h"
#include <api_debug.h>
#include "str_utils.c"
#include "api_os.h"

#include "api_hal_gpio.h"
#include "stdint.h"
#include "stdbool.h"
#include "api_debug.h"
#include "api_os.h"
#include "api_hal_pm.h"
#include "api_os.h"
#include "api_event.h"
#include "sms_lib.c"
   
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include "api_hal_gpio.h"
#include "api_hal_pm.h"

#include "api_os.h"
#include "api_debug.h"
#include "api_event.h"
#include "api_mqtt.h"
#include "api_network.h"
#include "api_socket.h"
#include "api_info.h"

/*
* Function: SMSInit
* -----------------
* Take: Void
* Return: Void
* Description: initilization of SMS and set Configration parameter.
*/
void SMSInit()
{
    if(!SMS_SetFormat(SMS_FORMAT_TEXT,SIM0))
    {
        Trace(1,"sms set format error");
        return;
    }
    SMS_Parameter_t smsParam = {
        .fo = 17 ,
        .vp = 167,
        .pid= 0  ,
        .dcs= 0  ,//0:English 7bit, 4:English 8 bit, 8:Unicode 2 Bytes
    };
    if(!SMS_SetParameter(&smsParam,SIM0))
    {
        Trace(1,"sms set parameter error");
        return;
    }
   
}
/*void SendUtf8Sms(char *phoneNumber, char *msg)
{
    uint8_t *unicode = NULL;
    uint32_t unicodeLen;
    clearStr(msg);
    Trace(1, "SMS to send:length=%d, msg:%s", strlen(msg), msg);

    if (!SMS_LocalLanguage2Unicode(msg, strlen(msg), CHARSET_UTF_8, &unicode, &unicodeLen))
    {
        Trace(1, "Converting local to unicode failed");
        return;
    }
    if (!SMS_SendMessage(phoneNumber, unicode, unicodeLen, SIM0))
    {
        Trace(1, "Sending SMS failed");
    }
    OS_Free(unicode);
}*/

/*
* Function: ClearSmsStorage
* -------------------------
* Take: Void
* Return: Void
* Description: get storage information of SIM Card to know the size of memory
* and then call function SMS_DeleteMessage to clear memory of SIM card.
*/
void ClearSmsStorage()
{
    SMS_Storage_Info_t storageInfo;

    SMS_GetStorageInfo(&storageInfo, SMS_STORAGE_SIM_CARD);
    Trace(1, "SMS storage of sim card: used=%d, total=%d", storageInfo.used, storageInfo.total);

    if (storageInfo.used > 0)
    {
        for (int i = 1; i <= storageInfo.total; i++)
        {
            if (!SMS_DeleteMessage(i, SMS_STATUS_ALL, SMS_STORAGE_SIM_CARD))
                Trace(1, "Deleting SIM sms fail at index %d", i);
            //else
            //Trace(1, "delete SIM sms success");
        }
        Trace(1, "Cleaning all SIM sms done");
    }
}
/*
* Function: ClearSmsStorage
* -------------------------
* Take:
* msgHeader= is string contain the phone number that sent this sms and the sms content.
* phoneNumber= The location of phone number storage
* Return: Void
* Description: use to split string to store the phone number in separated buffer  
*/
void Get_PhoneNumber(char *msgHeader, char *phoneNumber)
{
    //Trace(1, "msgHeader:%s", msgHeader);
    //"+84_sender_phone_number",fdsfsd,fdsfdsf,"+84_sms_center"
    int len = strlen(msgHeader);
    for (int i = 1; i < len; i++)
    {
        if (msgHeader[i] == '\"')
        {
            phoneNumber[i - 1] = 0;
            break;
        }
        phoneNumber[i - 1] = msgHeader[i];
    }
    //Trace(1, "phoneNumber:%s", phoneNumber);
}

#endif