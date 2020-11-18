#ifndef Prototype
#define Prototype
bool Is_Master_Phone_Number(char *number);
void Read_SMS(char *phoneNumber, char *smsContent);
bool MQTT_AttachActivate();
void MQTT_ConnectionCallBack(MQTT_Client_t *client, void *arg, MQTT_Connection_Status_t status);
void MQTT_PublishCallBack(void* arg, MQTT_Error_t err);
void MQTT_OnTimerPublish(void* param);
void MQTT_OnTimerStartConnect(void* param);
void MQTT_StartTimerPublish(uint32_t interval,MQTT_Client_t* client);
void MQTT_StartTimerConnect(uint32_t interval,MQTT_Client_t* client);
void SMS_ReplySms(void);
bool FS_ReadData(void);
void MQTT_Init(void);
void GPIOInit(void);
void ResetDataRegister(uint8_t i);
void FS_SaveData(char *Data);
void FS_Count_of_Numbers(char *Data);
bool FS_StoreData(uint8_t *path,char *Data,int64_t File_Size);
void SMSInit();
void ClearSmsStorage();
void Get_PhoneNumber(char *msgHeader, char *phoneNumber);
#endif