#ifndef MQTT_CONFIG
#define MQTT_CONFIG
//**************MQTT*****************
#define BROKER_IP  "broker.mqttdashboard.com"
#define BROKER_PORT 1883
#define CLIENT_USER "eslam"
#define CLIENT_PASS "eslam"
#define SUBSCRIBE_TOPIC "testtopic/1"
#define PUBLISH_TOPIC   "testtopic/1"
//**************TIMER****************
#define PUBLISH_INTERVAL 180000 //3M
#define CONNECTION_INTERVAL 120000//2M
#define REPLY_INTERVAL 60000//1M

//**************SMS******************
#define msg "areyouok"
#define Number_size 13
#define Number_Size_And_Termination 14

//**************TF CARD*******************
#define TF_CARD_TEST_FILE_NAME "/t/test_TF_card.txt"
//**************DEBOUNCING***********
#define SWITCH_DEBOUNCING 1500 //1M&30S "DELLAY"

#endif
