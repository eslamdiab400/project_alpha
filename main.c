#include "api_hal_gpio.h"
#include "stdint.h"
#include "stdbool.h"
#include "api_debug.h"
#include "api_os.h"
#include "api_hal_pm.h"
#include "api_os.h"
#include "api_event.h"

#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include "api_hal_gpio.h"
#include "api_hal_pm.h"
#include "sms_lib.c"
#include "api_os.h"
#include "api_debug.h"
#include "api_event.h"
#include "api_mqtt.h"
#include "api_network.h"
#include "api_socket.h"
#include "api_info.h"
#include "api_fs.h"
#include "api_charset.h"
#include "MQTT_CONFIG.h"
#include "main.h"
#define PDP_CONTEXT_APN       "cmnet"
#define PDP_CONTEXT_USERNAME  ""
#define PDP_CONTEXT_PASSWD    ""

#define MAIN_TASK_STACK_SIZE    (2048 * 2)
#define MAIN_TASK_PRIORITY      0
#define MAIN_TASK_NAME            "Main Test Task"

#define SECOND_TASK_STACK_SIZE    (2048 * 2)
#define SECOND_TASK_PRIORITY      1
#define SECOND_TASK_NAME          "Sub Main Task"

MQTT_Client_t* client;
static uint32_t reconnectInterval = 3000;
char **MASTER_NUMBERS;
char *MASTER_STATUS[100];
char *MASTER_DATA[100];
uint8_t Count_Of_Phone_Numbers=0;
char willMsg[50] = "GPRS 123456789012345 disconnected!";
uint8_t imei[16] = "";

static HANDLE mainTaskHandle = NULL;
static HANDLE secondTaskHandle = NULL;
static HANDLE semMqttStart = NULL;
MQTT_Connect_Info_t ci;

/*
* Function:Is_Master_Phone_Number
* -------------------------------
* Take:
* number= the phone number that sent sms
* Return:
* bool: it return True if the phone number be from the list of phones number that in the text file.
* return False if the number that sent the message is stranger.
* Description: this function used to compare the number that sent sms with the list of phone number that in the txt file.
*/
bool Is_Master_Phone_Number(char *number)
{ 
    for (int i = 0; i < Count_Of_Phone_Numbers; i++)
    {
        if (!strcmp(MASTER_NUMBERS[i], number))
            return true;
    }

    return false;
}
/*
* Function: Read_SMS
* ------------------
* Take: 
* phoneNumber= is the number that sent the SMS
* smsContent= the content of sms
* Return: Void
* Description:set the statues register of the number that sent the message "true" that meaning this number is active and responded
* to you when you send to him "are you ok"
* and put the smsContent in data array to Publish this messages to the MQTT server
*/
void Read_SMS(char *phoneNumber, char *smsContent)
{
    Trace(1, "Command: %s", smsContent);
	for(int i=0;i<Count_Of_Phone_Numbers;i++)
	{
		if(strcmp(MASTER_NUMBERS[i],phoneNumber)==0){
			MASTER_STATUS[i]="true";
		}
		MASTER_DATA[i]=smsContent;
	}
    
}
/*
* Function: MQTT_AttachActivate
* -----------------------------
* Take: Void
* Return: 
* bool: is Status of network get attach function ("True,False")
* Description: Connect to Network
*/
bool MQTT_AttachActivate()
{
    uint8_t status;
    bool ret = Network_GetAttachStatus(&status);
    if(!ret)
    {
        Trace(2,"get attach staus fail");
        return false;
    }
    Trace(2,"attach status:%d",status);
    if(!status)
    {
        ret = Network_StartAttach();
        if(!ret)
        {
            Trace(2,"network attach fail");
            return false;
        }
    }
    else
    {
        ret = Network_GetActiveStatus(&status);
        if(!ret)
        {
            Trace(2,"get activate staus fail");
            return false;
        }
        Trace(2,"activate status:%d",status);
        if(!status)
        {
            Network_PDP_Context_t context = {
                .apn        = PDP_CONTEXT_APN,
                .userName   = PDP_CONTEXT_USERNAME,
                .userPasswd = PDP_CONTEXT_PASSWD
            };
            Network_StartActive(context);
        }
    }
    return true;
}
/*
* Function: NetworkEventDispatch
* ------------------------------
* Take:
* pEvent: is pointer to structure that is contain the data of events
* Return: Void
* Description: check the event->id and excute the function of the id using switch case.
*/
static void NetworkEventDispatch(API_Event_t* pEvent)
{
    switch(pEvent->id)
    {
        case API_EVENT_ID_NETWORK_REGISTER_DENIED:
            Trace(2,"network register denied");
            break;

        case API_EVENT_ID_NETWORK_REGISTER_NO:
            Trace(2,"network register no");
            break;
        
        case API_EVENT_ID_NETWORK_REGISTERED_HOME:
        case API_EVENT_ID_NETWORK_REGISTERED_ROAMING:
            Trace(2,"network register success");
            MQTT_AttachActivate();
            break;

        case API_EVENT_ID_NETWORK_DETACHED:
            Trace(2,"network detached");
            MQTT_AttachActivate();
            break;
        case API_EVENT_ID_NETWORK_ATTACH_FAILED:
            Trace(2,"network attach failed");
            MQTT_AttachActivate();
            break;

        case API_EVENT_ID_NETWORK_ATTACHED:
            Trace(2,"network attach success");
            MQTT_AttachActivate();
            break;

        case API_EVENT_ID_NETWORK_DEACTIVED:
            Trace(2,"network deactived");
            MQTT_AttachActivate();
            break;

        case API_EVENT_ID_NETWORK_ACTIVATE_FAILED:
            Trace(2,"network activate failed");
            MQTT_AttachActivate();
            break;

        case API_EVENT_ID_NETWORK_ACTIVATED:
            Trace(2,"network activate success..");
            if(semMqttStart)
                OS_ReleaseSemaphore(semMqttStart);
            break;

        case API_EVENT_ID_SIGNAL_QUALITY:
            Trace(2,"CSQ:%d",pEvent->param1);
            break;

        default:
            break;
    }
}
/*
* Function: EventDispatch
* ------------------------------
* Take:
* pEvent: is pointer to structure that is contain the data of events
* Return: Void
* Description: check the event->id and excute the function of the id using switch case.
*/
static void EventDispatch(API_Event_t* pEvent)
{
    switch(pEvent->id)
    {    

        case API_EVENT_ID_NO_SIMCARD:
            Trace(2,"!!NO SIM CARD%d!!!!",pEvent->param1);
            break;
        case API_EVENT_ID_SIMCARD_DROP:
            Trace(2,"!!SIM CARD%d DROP!!!!",pEvent->param1);
            break;
        case API_EVENT_ID_SYSTEM_READY:

			Trace(2,"system initialize complete");
			
            break;
        case API_EVENT_ID_NETWORK_REGISTER_DENIED:
        case API_EVENT_ID_NETWORK_REGISTER_NO:
        case API_EVENT_ID_NETWORK_REGISTERED_HOME:
        case API_EVENT_ID_NETWORK_REGISTERED_ROAMING:
        case API_EVENT_ID_NETWORK_DETACHED:
        case API_EVENT_ID_NETWORK_ATTACH_FAILED:
        case API_EVENT_ID_NETWORK_ATTACHED:
        case API_EVENT_ID_NETWORK_DEACTIVED:
        case API_EVENT_ID_NETWORK_ACTIVATE_FAILED:
        case API_EVENT_ID_NETWORK_ACTIVATED:
        case API_EVENT_ID_SIGNAL_QUALITY:
            NetworkEventDispatch(pEvent);
            break;
 case API_EVENT_ID_SMS_SENT:
        Trace(1, "SMS sent");
        break;
    case API_EVENT_ID_SMS_RECEIVED:
        Trace(1, "Received message");
        char *header = pEvent->pParam1;
        char *content = pEvent->pParam2;

        Trace(1, "SMS length:%d,content:%s", strlen(content), content);

        char phoneNumber[20];

        Get_PhoneNumber(header, phoneNumber);

        if (Is_Master_Phone_Number(phoneNumber))
        {
            Trace(1, "SMS from master");

            Read_SMS(phoneNumber,content);
        }
        else
        {
            Trace(1, "SMS from stranger or [QC], header: %s", header);
        }
        OS_Free(phoneNumber);
        break;
	
        default:
            break;
    }
}
//********************That Is For Subscribe Data********************
/*void OnMqttReceived(void* arg, const char* topic, uint32_t payloadLen)
{
    Trace(1,"MQTT received publish data request, topic:%s, payload length:%d",topic,payloadLen);
}

void OnMqttReceiedData(void* arg, const uint8_t* data, uint16_t len, MQTT_Flags_t flags)
{
    Trace(1,"MQTT recieved publish data,  length:%d,data:%s",len,data);
    if(flags == MQTT_FLAG_DATA_LAST)
        Trace(1,"MQTT data is last frame");
}

 void OnMqttSubscribed(void* arg, MQTT_Error_t err)
 {
     if(err != MQTT_ERROR_NONE)
        Trace(1,"MQTT subscribe fail,error code:%d",err);
     else
        Trace(1,"MQTT subscribe success,topic:%s",(const char*)arg);
 }
*/
 
/*
* Function: MQTT_ConnectionCallBack
* ------------------------------
* Take:
* client： MQTT client object
* arg： parameter from MQTT_Connect function
* err： error code
* Description: is the callback function of MQTT_Publish
* Trace the status of connection
*/
void MQTT_ConnectionCallBack(MQTT_Client_t *client, void *arg, MQTT_Connection_Status_t status)//callback for mqtt_connection function
{
    Trace(1,"MQTT connection status:%d",status);
  
    if(status == MQTT_CONNECTION_ACCEPTED)
    {
        Trace(1,"MQTT succeed connect to broker");
        //!!! DO NOT subscribe here(interrupt function), do MQTT subscribe in task, or it will not excute
    }
    else
    {
        Trace(1,"MQTT connect to broker fail,error code:%d",status);
    }
    Trace(1,"MQTT OnMqttConnection() end");
}
/*
* Function: MQTT_PublishCallBack
* ------------------------------
* Take:
* arg： param from Publish function
* err： error code
* Description: is the callback function of MQTT_Publish
* Trace the status of publishing
*/
void MQTT_PublishCallBack(void* arg, MQTT_Error_t err)//callback for mqtt_publish function
{
    if(err == MQTT_ERROR_NONE)
        Trace(1,"MQTT publish success");
    else
        Trace(1,"MQTT publish error, error code:%d",err);
}
/*
* Function: MQTT_OnTimerPublish
* -----------------------------
* Take:
* param= MQTT client object
* Return: Void
* Description: Publish messages to the MQTT server (broker)
* after publishing ->>call ResetDataRegister function to reset the arrays to be ready to the next process
*/
void MQTT_OnTimerPublish(void* param)
{
	MQTT_Error_t err;
    MQTT_Client_t* client = (MQTT_Client_t*)param;

    Trace(1,"MQTT OnTimerPublish");
	for(uint8_t i=0;i<Count_Of_Phone_Numbers;i++)
	{
    err = MQTT_Publish(client,PUBLISH_TOPIC,MASTER_DATA[i],strlen(MASTER_DATA[i]),1,2,0,MQTT_PublishCallBack,NULL);
    if(err != MQTT_ERROR_NONE)
        Trace(1,"MQTT publish error, error code:%d",err);
	else
	{	
		OS_Sleep(4000);
		ResetDataRegister(i);
	}
	}
	
}
/*
* Function: ResetDataRegister
* ---------------------------
* Take: 
* i= the sequance of number whose data you want reset 
* Return: Void
* Description: reset arrays of pointers (Data,Status) 
*/
void ResetDataRegister(uint8_t i)
{
		MASTER_STATUS[i]="false";
		MASTER_DATA[i]="NO_DATA";
}
/*
* Function: MQTT_StartTimerPublish
* --------------------------------
* Take:
* interval= timing time to execute callback function
* client= passes the parameters of the callback function
* Return: Void
* Description: call OS_StartCallbackTimer to create the timer to wait interval time which you select.
  after this time -->>execute callback function"MQTT_OnTimerPublish".
*/
void MQTT_StartTimerPublish(uint32_t interval,MQTT_Client_t* client)
{
    OS_StartCallbackTimer(mainTaskHandle,interval,MQTT_OnTimerPublish,(void*)client);
}
/*
* Function: MQTT_OnTimerStartConnect
* ----------------------------------
* Take: 
* param= MQTT client object
* Return: Void
* Description: Connect the MQTT server(broker) using MQTT_Connect Function
* if MQTT connect fail ->>reconnect again after reconnectInterval
*/
void MQTT_OnTimerStartConnect(void* param)
{
    MQTT_Error_t err;
    MQTT_Client_t* client = (MQTT_Client_t*)param;
    uint8_t status = MQTT_IsConnected(client);
    Trace(1,"mqtt status:%d",status);

    err = MQTT_Connect(client,BROKER_IP,BROKER_PORT,MQTT_ConnectionCallBack,NULL,&ci);
    if(err != MQTT_ERROR_NONE)
    {
        Trace(1,"MQTT connect fail,error code:%d",err);
        reconnectInterval += 1000;
        if(reconnectInterval >= 60000)
            reconnectInterval = 60000;
        MQTT_StartTimerConnect(reconnectInterval,client);
    }
}
/*
* Function: MQTT_StartTimerConnect
* --------------------------------
* Take:
* interval= timing time to execute callback function
* client= passes the parameters of the callback function
* Return: Void
* Description: call OS_StartCallbackTimer to create the timer to wait interval time which you select.
  after this time -->>execute callback function"MQTT_OnTimerStartConnect".
*/
void MQTT_StartTimerConnect(uint32_t interval,MQTT_Client_t* client)
{
    OS_StartCallbackTimer(mainTaskHandle,interval,MQTT_OnTimerStartConnect,(void*)client);
}
/*
* Function: SMS_ReplySms
* Take: Void
* Return: Void
* Description: is to reply sms after "REPLY_INTERVAL" time to the device that didnt responde.
* check Status array and know which nember not respond and resend to this number again.
*/
void SMS_ReplySms(void)
{	 Trace(1,"Numbers Of Phones=%d",Count_Of_Phone_Numbers);

	for (int i=0;i<Count_Of_Phone_Numbers;i++)
	Trace(1,"MASTERNUMBERS=%s  MASTER_STATUS[i]=%s   MASTER_DATA[i]=%s",MASTER_NUMBERS[i],MASTER_STATUS[i],MASTER_DATA[i]);
	for(int i=0;i<Count_Of_Phone_Numbers;i++)
	{
		if(strcmp(MASTER_STATUS[i],"false")==0){
            Trace(1,"master_status=%d",i);
			  SMS_SendMessage(MASTER_NUMBERS[i],msg,strlen(msg),SIM0);
		}
			   	OS_Sleep(4000);
	}
}
/*
* Function: FS_Count_of_Numbers
* -----------------------------
* Take:
* Data= The location of the read data storage
* Return: Void
* Description: count how many "\n" numbers in the file using strtok function.
* put the final value of counting in Count_Of_Phone_Numbers.
*/
 void FS_Count_of_Numbers(char *Data)
{ 
  uint8_t* Pointer_to_data;
 	 Pointer_to_data=strtok(Data,"\n");
  while(Pointer_to_data!=NULL)
  {
     Pointer_to_data = strtok (NULL,"\n");
  Count_Of_Phone_Numbers++;
  }
  Trace(1,"Count_Of_Phone_Numbers=%d",Count_Of_Phone_Numbers);

}
 /*
* Function: FS_SaveData 
* ---------------------
* Take:
* Data= The location of the read data storage
* Return: Void
* Description: Allocate array of pointers to store the data separetly and this allocation depen on how many numbers in file
* after allocation -->store the Data in this array of pointers using "Strtok" function >>>Sequentially<<<.
*/
 void FS_SaveData(char *Data)
 {
	 uint8_t* Pointer_to_data;
     MASTER_NUMBERS=OS_Malloc(Count_Of_Phone_Numbers * sizeof(char*));
	 Count_Of_Phone_Numbers=0;
	 Pointer_to_data=strtok(Data,"\n");
     while(Pointer_to_data!=NULL)
     {
     MASTER_NUMBERS[Count_Of_Phone_Numbers]=OS_Malloc(strlen(Pointer_to_data) * sizeof(char));
     MASTER_NUMBERS[Count_Of_Phone_Numbers]=Pointer_to_data;
     Pointer_to_data = strtok (NULL,"\n");
     Count_Of_Phone_Numbers++;
     }
 }
 /*
* Function: FS_ReadData
* ---------------------
* Take: 
* path= is the path of the file that you need to open
* Data= The location of the read data storage
* File_Size= Length of read data
* Return: succeed or failed to open the text file from SD card
* Description: open the text file then read data from text file and store it in array"Data"
* call FS_SaveDatan Function to Store the data nembers in array of pointers separetly to be easy to access later
* finally close the file.
*/
bool FS_StoreData(uint8_t *path,char *Data,int64_t File_Size)
  {
	int32_t fd;
	int32_t ret;
	fd = API_FS_Open(path, (FS_O_RDONLY|FS_O_CREAT), 0);//open file
 if ( fd < 0)
 {			
    Trace(1,"Open file failed:%d",fd);
	return false;
 }
    ret = API_FS_Read(fd,Data,File_Size) ;//function that read the size of file
	Data[File_Size]='\0';
    FS_SaveData(Data);
	API_FS_Close(fd); 
 }
/*
* Function: FS_ReadData
* ---------------------
* Take: Void
* Return: succeed or failed to open the text file from SD card
* Description: is used to read data from sd card first open the text file and return false if "open file failed"
* after opening the file get the file size to allocate new array"x" which has the same size of the text file
* then call function API_FS_Read to read the data and store it in array"x" put the data not be seprated
* then call function FS_Count_of_Numbers to"""estimate how many numbers""" in this file using (strtok) function
* we use this function to know the number of lines to allocate array of pointers"x" that depend on this process 
* then call function to close this file to start new process on this file
* the nxt process is to call FS_StoreData function to open the same file again
* and read the same data and store it in array of pointers"x" to seprate the string numbers
* then call ResetDataRegister function to reset data and status registers to initial value.
*/
bool FS_ReadData(void)
{
	int32_t fd;//the return value of open function
    int32_t ret;//return value of read function ("success , fail")
    uint8_t *path = TF_CARD_TEST_FILE_NAME;//the path of the file tha you will read
    int64_t File_Size;// the size of the file
	static char *Data;
    fd = API_FS_Open(path, (FS_O_RDONLY|FS_O_CREAT), 0);//open file
	if ( fd < 0)
	{			
        Trace(1,"Open file failed:%d",fd);
		return false;
    }
    File_Size=API_FS_GetFileSize(fd);//read the size of meomory
    Data=OS_Malloc(File_Size) ;
	Trace(1,"Size.........=%d ",File_Size);
    ret = API_FS_Read(fd,Data,File_Size) ;//function that read the size of file
    FS_Count_of_Numbers(Data);
	API_FS_Close(fd);
    FS_StoreData(path,Data,File_Size);
   	for (uint8_t i=0;i<Count_Of_Phone_Numbers;i++)
	{
		 ResetDataRegister(i);
	}
}
/*
* Function: GPIO_IntCallBack
* --------------------------
* Take: "param"= is pointer to structure that is contain the data of interrupt status
* Return: Void
* Description: this Function is to send SMS to the numbers that was saved from SD card
* and call functions that start the timer of SMS_ReplySms function ,timer of MQTT_Connetion Function
* and timer of MQTT_publish Function when the timer of those functions is end the functions code will execute.
*/
void GPIO_IntCallBack(GPIO_INT_callback_param_t* param)
{
	 Trace(1,"OnPinFalling");
     Trace(1,"gpio detect falling edge!pin:%d",param->pin);
	 Trace(1,"Numbers Of Phones=%d",Count_Of_Phone_Numbers);
    for (int i=0;i<Count_Of_Phone_Numbers;i++)
	 Trace(1,"MASTERNUMBERS=%s  MASTER_STATUS[i]=%s   MASTER_DATA[i]=%s",MASTER_NUMBERS[i],MASTER_STATUS[i],MASTER_DATA[i]);
	for (int i = 0; i < Count_Of_Phone_Numbers; i++)
		{
			  SMS_SendMessage(MASTER_NUMBERS[i],msg,strlen(msg),SIM0);
		}

    OS_StartCallbackTimer(mainTaskHandle,REPLY_INTERVAL,SMS_ReplySms,NULL);//Timer For Reply Sms to The Device That Didnt Respond
    MQTT_StartTimerConnect(CONNECTION_INTERVAL,client);//Timer For Connection of Mqtt
	MQTT_StartTimerPublish(PUBLISH_INTERVAL,client);//Timer For Publish
}
/*
* Function: MQTT_Init
* -------------------
* Take: Void
* Return: Void
* Description: to initilize MQTT protocol and set configration like "client user and pass" & and create a new client.
*/
void MQTT_Init(void)
{
	Trace(1,"start mqtt test");
    INFO_GetIMEI(imei);
    Trace(1,"IMEI:%s",imei);
    client = MQTT_ClientNew();
    memset(&ci,0,sizeof(MQTT_Connect_Info_t));
    ci.client_id = imei;
    ci.client_user = CLIENT_USER;
    ci.client_pass = CLIENT_PASS;
    ci.keep_alive = 20;
    ci.clean_session = 1;
    ci.use_ssl = false;
    ci.will_qos = 2;
    ci.will_topic = "will";
    ci.will_retain = 1;
    memcpy(strstr(willMsg,"GPRS")+5,imei,15);
    ci.will_msg = willMsg;
}
/*
* Function: GPIOInit
* ------------------
* Take: Void 
* Return: Void
* Description: Initilization GPIO pins and set configration of this pins that initilize GPIO_PIN2 as interrupt input pin
* this pin is connected to external pushbuttom when this pushbuttom is pressed the interrupt callback function will call.
*/
void GPIOInit(void)
{
	  GPIO_config_t gpioInt = {
        .mode               = GPIO_MODE_INPUT_INT,
        .pin                = GPIO_PIN2,
        .defaultLevel       = GPIO_LEVEL_HIGH,
        .intConfig.debounce = SWITCH_DEBOUNCING,
        .intConfig.type     = GPIO_INT_TYPE_FALLING_EDGE,
        .intConfig.callback = GPIO_IntCallBack
    };
		GPIO_Init(gpioInt);
}
/*
* Function: SecondTask
* ------------------
* Take: Void Pointer
* Return: Void
* Description: create semaphore and wiat to release that in NetworkEventDispatch Function 
* then when semaphore is released the next step is Delet semaphore
* then Call MQTT initilization function,GPIO initilization function,SMS initilization function
* then Call Function that delete sms storage in SIM card "SIM memory"
* Then Call Function FS_ReadData to read the text file that contains phone numbers from SD Card
* then enter to while loop that is trace the phone numbers and the data that is received from this phone 
* and the status of this number (send data or not).
*/
void SecondTask(void *pData)
{
    semMqttStart = OS_CreateSemaphore(0);
    OS_WaitForSemaphore(semMqttStart,OS_WAIT_FOREVER);
    OS_DeleteSemaphore(semMqttStart);
    semMqttStart = NULL;
    MQTT_Init();
    GPIOInit();
	ClearSmsStorage();
	SMSInit();
    FS_ReadData();
    while(1)
    {
    for (int i=0;i<Count_Of_Phone_Numbers;i++)
	Trace(1,"MASTERNUMBERS=%s  MASTER_STATUS[i]=%s   MASTER_DATA[i]=%s",MASTER_NUMBERS[i],MASTER_STATUS[i],MASTER_DATA[i]);
	   	OS_Sleep(8000);
    }
}
/*
* Function: MainTask
* ------------------
* Take: Void Pointer
* Return: Void
* Description: create second task & create loop to check events.
*/
void MainTask(void *pData)
{
    API_Event_t* event=NULL;
    secondTaskHandle = OS_CreateTask(SecondTask,
        NULL, NULL, SECOND_TASK_STACK_SIZE, SECOND_TASK_PRIORITY, 0, 0, SECOND_TASK_NAME);
    while(1)
    {
        if(OS_WaitEvent(mainTaskHandle, (void**)&event, OS_TIME_OUT_WAIT_FOREVER))
        {
            EventDispatch(event);
            OS_Free(event->pParam1);
            OS_Free(event->pParam2);
            OS_Free(event);
        }
    }
}
/*
* Function: proj_Main 
* -------------------
* Take: Void
* Return: Void
* Description: create the main task.
*/
void proj_Main(void)
{
    mainTaskHandle = OS_CreateTask(MainTask,
        NULL, NULL, MAIN_TASK_STACK_SIZE, MAIN_TASK_PRIORITY, 0, 0, MAIN_TASK_NAME);
    OS_SetUserMainHandle(&mainTaskHandle);
}



