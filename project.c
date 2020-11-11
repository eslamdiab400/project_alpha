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
#include "api_fs.h"
#include "api_charset.h"
#include "MQTT_CONFIG.h"
#define PDP_CONTEXT_APN       "cmnet"
#define PDP_CONTEXT_USERNAME  ""
#define PDP_CONTEXT_PASSWD    ""

#define MAIN_TASK_STACK_SIZE    (2048 * 2)
#define MAIN_TASK_PRIORITY      0
#define MAIN_TASK_NAME            "Main Test Task"

#define SECOND_TASK_STACK_SIZE    (2048 * 2)
#define SECOND_TASK_PRIORITY      1
#define SECOND_TASK_NAME          "MQTT Test Task"



MQTT_Client_t* client;
uint8_t Numbers=0;
char MASTER_NUMBERS[100][Number_Size_And_Termination];
char *MASTER_STATUES[100];
char *MASTER_DATA[100];
char willMsg[50] = "GPRS 123456789012345 disconnected!";
uint8_t imei[16] = "";

static HANDLE mainTaskHandle = NULL;
static HANDLE secondTaskHandle = NULL;
static HANDLE interruptTaskHandle = NULL;
static HANDLE semMqttStart = NULL;
MQTT_Connect_Info_t ci;

bool Is_Master_Phone_Number(char *number)
{ 
    for (int i = 0; i < Numbers; i++)
    {
        if (!strcmp(MASTER_NUMBERS[i], number))
            return true;
    }

    return false;
}
void Handle_SMS(char *phoneNumber, char *smsContent)
{
    Trace(1, "Command: %s", smsContent);
	for(int i=0;i<Numbers;i++)
	{
		if(strcmp(MASTER_NUMBERS[i],phoneNumber)==0){
			MASTER_STATUES[i]="true";
		}
		MASTER_DATA[i]=smsContent;
	}
    
}
bool AttachActivate()
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
            AttachActivate();
            break;

        case API_EVENT_ID_NETWORK_DETACHED:
            Trace(2,"network detached");
            AttachActivate();
            break;
        case API_EVENT_ID_NETWORK_ATTACH_FAILED:
            Trace(2,"network attach failed");
            AttachActivate();
            break;

        case API_EVENT_ID_NETWORK_ATTACHED:
            Trace(2,"network attach success");
            AttachActivate();
            break;

        case API_EVENT_ID_NETWORK_DEACTIVED:
            Trace(2,"network deactived");
            AttachActivate();
            break;

        case API_EVENT_ID_NETWORK_ACTIVATE_FAILED:
            Trace(2,"network activate failed");
            AttachActivate();
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

        char *phoneNumber[20];

        Get_PhoneNumer(header, phoneNumber);

        if (Is_Master_Phone_Number(phoneNumber))
        {
            Trace(1, "SMS from master");

            Handle_SMS(phoneNumber,content);
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
void OnMqttConnection(MQTT_Client_t *client, void *arg, MQTT_Connection_Status_t status)//callback for mqtt_connection function
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

static uint32_t reconnectInterval = 3000;
void MQTT_StartTimerPublish(uint32_t interval,MQTT_Client_t* client);
void MQTT_StartTimerConnect(uint32_t interval,MQTT_Client_t* client);

void OnPublish(void* arg, MQTT_Error_t err)//callback for mqtt_publish function
{
    if(err == MQTT_ERROR_NONE)
        Trace(1,"MQTT publish success");
    else
        Trace(1,"MQTT publish error, error code:%d",err);
}
void OnTimerPublish(void* param)
{
	MQTT_Error_t err;
    MQTT_Client_t* client = (MQTT_Client_t*)param;

    Trace(1,"MQTT OnTimerPublish");
	for(uint8_t i=0;i<Numbers;i++)
	{
    err = MQTT_Publish(client,PUBLISH_TOPIC,MASTER_DATA[i],strlen(MASTER_DATA[i]),1,2,0,OnPublish,NULL);
    if(err != MQTT_ERROR_NONE)
        Trace(1,"MQTT publish error, error code:%d",err);
				   	OS_Sleep(4000);
	}
	for (uint8_t i=0;i<Numbers;i++)
	{
		MASTER_STATUES[i]="false";
		MASTER_DATA[i]="NO_DATA";
	}
}

void MQTT_StartTimerPublish(uint32_t interval,MQTT_Client_t* client)
{
    OS_StartCallbackTimer(mainTaskHandle,interval,OnTimerPublish,(void*)client);
}

void OnTimerStartConnect(void* param)
{
    MQTT_Error_t err;
    MQTT_Client_t* client = (MQTT_Client_t*)param;
    uint8_t status = MQTT_IsConnected(client);
    Trace(1,"mqtt status:%d",status);

    err = MQTT_Connect(client,BROKER_IP,BROKER_PORT,OnMqttConnection,NULL,&ci);
    if(err != MQTT_ERROR_NONE)
    {
        Trace(1,"MQTT connect fail,error code:%d",err);
        reconnectInterval += 1000;
        if(reconnectInterval >= 60000)
            reconnectInterval = 60000;
        MQTT_StartTimerConnect(reconnectInterval,client);
    }
}

void MQTT_StartTimerConnect(uint32_t interval,MQTT_Client_t* client)
{
    OS_StartCallbackTimer(mainTaskHandle,interval,OnTimerStartConnect,(void*)client);
}

void ReplySms(void)
{	 Trace(1,"Numbers Of Phones=%d",Numbers);

	for (int i=0;i<Numbers;i++)
	Trace(1,"MASTERNUMBERS=%s  MASTER_STATUES[i]=%s   MASTER_DATA[i]=%s",MASTER_NUMBERS[i],MASTER_STATUES[i],MASTER_DATA[i]);
	for(int i=0;i<Numbers;i++)
	{
		if(strcmp(MASTER_STATUES[i],"false")==0){
            Trace(1,"master_status=%d",i);
			  SMS_SendMessage(MASTER_NUMBERS[i],msg,strlen(msg),SIM0);
		}
			   	OS_Sleep(4000);

	}
	
}
void FS_voidReadData(void)
{
	int32_t fd;//the return value of open function
    int32_t ret;//return value of read function ("success , fail")
    uint8_t *path = TF_CARD_TEST_FILE_NAME;//the path of the file tha you will read
int64_t Size;// the size of the file
uint8_t Number_Of_Phones=0;
    fd = API_FS_Open(path, (FS_O_RDONLY|FS_O_CREAT), 0);//open sd card
	if ( fd < 0)
	{			
        Trace(1,"Open file failed:%d",fd);
    }
	Trace(1,"API_FS_GetFileSize");
Size=API_FS_GetFileSize(fd);//read the size of meomory
	Trace(1,"exit API_FS_GetFileSize");
	Trace(1,"Size.........=%d ",Size);


uint8_t colmn=0;
uint8_t Row=0;
uint8_t Data[Size];
uint8_t number_of_lines=0;
memset(Data,0,sizeof(Data));
ret = API_FS_Read(fd,Data,Size) ;//function that read the size of file
for(int64_t i=0;i<Size;i++)
{
	if(Data[i]=='\n')
	{
		MASTER_NUMBERS[Row][colmn]='\0';
		Row++;
		number_of_lines++;
		colmn=0;
	}
	else
	{
	MASTER_NUMBERS[Row][colmn]=Data[i];
	
	colmn++;
	}
	}
	Number_Of_Phones=(Size/Number_size)-(number_of_lines/Number_size);//get number of phones
	Numbers=Number_Of_Phones;
    API_FS_Close(fd);
	for (uint8_t i=0;i<Numbers;i++)
	{
		MASTER_STATUES[i]="false";
		MASTER_DATA[i]="NO_DATA";
	}
		 Trace(1,"Numbers Of Phone=%d",Numbers);
for (int i=0;i<Numbers;i++)
	Trace(1,"MASTERNUMBERS=%s  MASTER_STATUES[i]=%s   MASTER_DATA[i]=%s",MASTER_NUMBERS[i],MASTER_STATUES[i],MASTER_DATA[i]);

}
void OnPinFalling(GPIO_INT_callback_param_t* param)
{

	Trace(1,"OnPinFalling");
    Trace(1,"gpio detect falling edge!pin:%d",param->pin);
	 Trace(1,"Numbers Of Phones=%d",Numbers);
for (int i=0;i<Numbers;i++)
	Trace(1,"MASTERNUMBERS=%s  MASTER_STATUES[i]=%s   MASTER_DATA[i]=%s",MASTER_NUMBERS[i],MASTER_STATUES[i],MASTER_DATA[i]);
	for (int i = 0; i < Numbers; i++)
		{
			  SMS_SendMessage(MASTER_NUMBERS[i],msg,strlen(msg),SIM0);

		}

    OS_StartCallbackTimer(mainTaskHandle,REPLY_INTERVAL,ReplySms,NULL);//Timer For Reply Sms to The Device That Didnt Respond
    MQTT_StartTimerConnect(CONNECTION_INTERVAL,client);//Timer For Connection of Mqtt
	MQTT_StartTimerPublish(PUBLISH_INTERVAL,client);//Timer For Publish
	

}
void MQTT_voidInitilizationFunction(void)
{
	    Trace(1,"start mqtt test");
    INFO_GetIMEI(imei);
    Trace(1,"IMEI:%s",imei);
    client = MQTT_ClientNew();
    MQTT_Error_t err;
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
void GPIO_voidInitilizationOfPins(void)
{
	  GPIO_config_t gpioInt = {
        .mode               = GPIO_MODE_INPUT_INT,
        .pin                = GPIO_PIN2,
        .defaultLevel       = GPIO_LEVEL_HIGH,
        .intConfig.debounce = SWITCH_DEBOUNCING,
        .intConfig.type     = GPIO_INT_TYPE_FALLING_EDGE,
        .intConfig.callback = OnPinFalling
    };
		GPIO_Init(gpioInt);
}
void SecondTask(void *pData)
{
    semMqttStart = OS_CreateSemaphore(0);
    OS_WaitForSemaphore(semMqttStart,OS_WAIT_FOREVER);
    OS_DeleteSemaphore(semMqttStart);
    semMqttStart = NULL;
    SMSInit();
    MQTT_voidInitilizationFunction();
    GPIO_voidInitilizationOfPins();
    FS_voidReadData();
    while(1)
    {for(uint8_t i=0;i<Numbers;i++)
        Trace(1,"status[%d]=%s",i,MASTER_STATUES[i]);
	   	OS_Sleep(8000);
    }
}

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

void mqtt_Main(void)
{
    mainTaskHandle = OS_CreateTask(MainTask,
        NULL, NULL, MAIN_TASK_STACK_SIZE, MAIN_TASK_PRIORITY, 0, 0, MAIN_TASK_NAME);
    OS_SetUserMainHandle(&mainTaskHandle);
}



