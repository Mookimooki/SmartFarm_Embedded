/*
 *  dht22.c:
 *	Simple test program to test the wiringPi functions
 *	Based on the existing dht11.c
 *	Amended by technion@lolware.net
 */

#include <wiringPi.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <mysql/mysql.h>
#include <string.h>
#include <errno.h>
#include <wiringPiSPI.h>

//queue
#define MAXTIMINGS 85
#define PUMP 21
#define MAX_QUEUE_SIZE 100

//MYSQL
#define DBHOST "ec2-52-79-81-142.ap-northeast-2.compute.amazonaws.com"
#define DBUSER "root"
#define DBPASS "root"
#define DBNAME "smartfarmdb"

//Light sensor
#define CS_MCP3208 8 //GPIO 8
#define SPI_CHANNEL 0
#define SPI_SPEED 1000000 //1Mhz
#define LIGHT_CRITERIA 2000

//LED
#define RGBLEDPOWER  19 //BCM_GPIO 19
#define RED 8 //27
#define GREEN   7 //28
#define BLUE    9 //29

//FAN
#define FAN 6

//DB upload Interval
#define DEFAULT_UPLOAD_INTERVAL 10

MYSQL *connector;
MYSQL_RES *result;
MYSQL_ROW row;

pthread_mutex_t time_mutex	= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t time_cond	= PTHREAD_COND_INITIALIZER;

pthread_mutex_t temp_mutex	= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t temp_cond	= PTHREAD_COND_INITIALIZER;

pthread_mutex_t light_mutex	= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t light_cond	= PTHREAD_COND_INITIALIZER;

pthread_mutex_t fan_mutex	= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t fan_cond		= PTHREAD_COND_INITIALIZER;

int done = 0;
int ret_temp;
int upload_interval = DEFAULT_UPLOAD_INTERVAL;

//for temperature
int queue_size_temp = 0;
int front_temp = 0;
int rear_temp = 0;
int queue_temp[MAX_QUEUE_SIZE];   //for temperature

//for light
int queue_size_light = 0;
int front_light = 0;
int rear_light = 0;
int queue_light[MAX_QUEUE_SIZE];  //for light

//for time
int queue_size_time = 0;
int front_time = 0;
int rear_time = 0;
char queue_time[MAX_QUEUE_SIZE][100];  //for time

static int DHTPIN = 7;
//static int DHTPIN = 11;
static int dht22_dat[5] = {0,0,0,0,0};

time_t ct;
struct tm tm;

void sig_handler(int signo);
int read_dht22_dat();
int read_mcp3208_adc(unsigned char adcChannel);

int push_temp(int value){
    if ((rear_temp + 1) % MAX_QUEUE_SIZE == front_temp) {
        return -1;
    }
	//printf("pusing temp: %d\n", value);
	queue_size_temp++;
    rear_temp = (rear_temp + 1) % MAX_QUEUE_SIZE;
    queue_temp[rear_temp] = value;
}

int pop_temp(){
    if(front_temp == rear_temp){
        return -1;
    }
	queue_size_temp--;
    front_temp = (front_temp+1) % MAX_QUEUE_SIZE;
    
    return queue_temp[front_temp];
}

int push_light(int value){
    if ((rear_light + 1) % MAX_QUEUE_SIZE == front_light) {
        return -1;
    }
	//printf("pusing light: %d\n", value);
    queue_size_light++;
    rear_light = (rear_light + 1) % MAX_QUEUE_SIZE;
    queue_light[rear_light] = value;
}

int pop_light(){
    if(front_light == rear_light){
        return -1;
    }
    queue_size_light--;
    front_light = (front_light +1) % MAX_QUEUE_SIZE;
    
    return queue_light[front_light];
}

int push_time(char *input) {
	if ((rear_time + 1) % MAX_QUEUE_SIZE == front_time) {
		return -1;
	}
	//printf("pushing time: %s\n", input);
	queue_size_time++;
	rear_time = (rear_time + 1) % MAX_QUEUE_SIZE;
    strcpy(queue_time[rear_time], input);
}

int pop_time(char *output) {
	if (front_time == rear_time) {
		return -1;
	}
	queue_size_time--;
	front_time = (front_time + 1) % MAX_QUEUE_SIZE;

	strcpy(output, queue_time[front_time]);
}

int read_mcp3208_adc(unsigned char adcChannel)
{
    unsigned char buff[3];
    int adcValue = 0;

    buff[0] = 0x06 | ((adcChannel & 0x07) >> 2);
    buff[1] = ((adcChannel & 0x07) << 6);
    buff[2] = 0x00;
    digitalWrite(CS_MCP3208, 0);
    wiringPiSPIDataRW(SPI_CHANNEL, buff, 3);
    printf("%s", buff);

    buff[1] = 0x0f & buff[1];
    adcValue = (buff[1] << 8 ) | buff[2];
    digitalWrite(CS_MCP3208, 1);
    return adcValue;
}

static uint8_t sizecvt(const int read)
{
  /* digitalRead() and friends from wiringpi are defined as returning a value
  < 256. However, they are returned as int() types. This is a safety function */

  if (read > 255 || read < 0)
  {
    printf("Invalid data from wiringPi library\n");
    exit(EXIT_FAILURE);
  }
  return (uint8_t)read;
}

int read_dht22_dat()
{
  uint8_t laststate = HIGH;
  uint8_t counter = 0;
  uint8_t j = 0, i;

  dht22_dat[0] = dht22_dat[1] = dht22_dat[2] = dht22_dat[3] = dht22_dat[4] = 0;

  // pull pin down for 18 milliseconds
  pinMode(DHTPIN, OUTPUT);
  digitalWrite(DHTPIN, HIGH);
  delay(10);
  digitalWrite(DHTPIN, LOW);
  delay(18);
  // then pull it up for 40 microseconds
  digitalWrite(DHTPIN, HIGH);
  delayMicroseconds(40); 
  // prepare to read the pin
  pinMode(DHTPIN, INPUT);

  // detect change and read data
  for ( i=0; i< MAXTIMINGS; i++) {
    counter = 0;
    while (sizecvt(digitalRead(DHTPIN)) == laststate) {
      counter++;
      delayMicroseconds(1);
      if (counter == 255) {
        break;
      }
    }
    laststate = sizecvt(digitalRead(DHTPIN));

    if (counter == 255) break;

    // ignore first 3 transitions
    if ((i >= 4) && (i%2 == 0)) {
      // shove each bit into the storage bytes
      dht22_dat[j/8] <<= 1;
      if (counter > 50)
        dht22_dat[j/8] |= 1;
      j++;
    }
  }

  // check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
  // print it out if data is good
  if ((j >= 40) && 
      (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF)) ) {
        float t, h;
		
        h = (float)dht22_dat[0] * 256 + (float)dht22_dat[1];
        h /= 10;
        t = (float)(dht22_dat[2] & 0x7F)* 256 + (float)dht22_dat[3];
        t /= 10.0;
        if ((dht22_dat[2] & 0x80) != 0)  t *= -1;
		
		ret_temp = (int)t;

		//printf("Humidity = %.2f %% Temperature = %.2f *C \n", h, t );
		//printf("Humidity = %d Temperature = %d\n", ret_humid, ret_temp);
		
    return ret_temp;
  }
  else
  {
    //printf("Data not good, skip\n");
    return 0;
  }
}

void *detecting(void *arg) {
	//printf("detecting_start\n\n");
	char ex_time[100];
	int adcValue_light = 0;
	unsigned char adcChannel_light = 0;
    int ret=0;
    pinMode(RGBLEDPOWER, OUTPUT);
	
    while (1) {

		//Temperauture
        while (read_dht22_dat() == 0)
		    delay(5);
        pthread_mutex_lock(&temp_mutex);
		while (queue_size_temp == MAX_QUEUE_SIZE)
			pthread_cond_wait(&temp_cond, &temp_mutex);
		push_temp(ret_temp);
		pthread_cond_signal(&temp_cond);
		pthread_mutex_unlock(&temp_mutex);

		//FAN
		if(ret_temp > 27)
			pthread_cond_signal(&fan_cond);

		//Brightness
		pthread_mutex_lock(&light_mutex);
		adcValue_light = 4095 - read_mcp3208_adc(adcChannel_light);
		if (adcValue_light < LIGHT_CRITERIA)
		    digitalWrite(RGBLEDPOWER, 1);
        else
		    digitalWrite(RGBLEDPOWER, 0);
        while (queue_size_light == MAX_QUEUE_SIZE)
			pthread_cond_wait(&light_cond, &light_mutex);
		push_light(adcValue_light);
		pthread_cond_signal(&light_cond);
		pthread_mutex_unlock(&light_mutex);

		//Time
		pthread_mutex_lock(&time_mutex);
		while (queue_size_time == MAX_QUEUE_SIZE)
			pthread_cond_wait(&time_cond, &time_mutex);
	    ct = time(NULL);
	    tm = *localtime(&ct);
		sprintf(ex_time, "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
		push_time(ex_time);
		pthread_cond_signal(&time_cond);
		pthread_mutex_unlock(&time_mutex);

		delay(100);
	}
	return NULL;
}

void *upload_DB(void *arg){
	int temp;
    int light;
	char time[100];    
	char query[1024];
	int num_of_query;
		
	while (1) {
		sleep(upload_interval);
		printf("Start uploading\n");
		num_of_query = queue_size_time;
		while (num_of_query > 0) {
			pthread_mutex_lock(&temp_mutex);
			while (queue_size_temp == 0)
				pthread_cond_wait(&temp_cond, &temp_mutex);
			temp = pop_temp();
			pthread_mutex_unlock(&temp_mutex);

			pthread_mutex_lock(&light_mutex);
			while (queue_size_light == 0)
				pthread_cond_wait(&light_cond, &light_mutex);
			light = pop_light();
			pthread_mutex_unlock(&light_mutex);

			pthread_mutex_lock(&time_mutex);
			while (queue_size_light == 0)
				pthread_cond_wait(&time_cond, &time_mutex);
			pop_time(time);
			pthread_mutex_unlock(&time_mutex);

			sprintf(query, "INSERT INTO iot VALUES (\"%s\",%02d,%04d)", time, temp, light);

			if (mysql_query(connector, query))
			{
				fprintf(stderr, "%s\n", mysql_error(connector));
				printf("Write DB error\n");
			}
			printf("Temp = %d", temp);
			printf(", Light = %d", light);
			printf(", time = %s\n", time);
			num_of_query--;
		}
		printf("End uploading\n\n");
	}

    return NULL;
}

void *TurnningFAN(void *arg) {
	while (1) {
		pthread_mutex_lock(&fan_mutex);
		pthread_cond_wait(&fan_cond, &fan_mutex);

		digitalWrite(FAN, 1);
		sleep(5);
		digitalWrite(FAN, 0);
		sleep(5);

		pthread_mutex_unlock(&fan_mutex);
	}

	return NULL;
}

int main (int argc, char* argv[])
{
    if(argc > 2){
        fputs("Too many arguments\n", stderr);
        exit(1);
    }

    if(argc == 2){
        upload_interval = atoi(argv[1]);
    }
	ct = time(NULL);
	tm = *localtime(&ct);
	printf("start: %d-%d-%d %d:%d:%d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

	signal(SIGINT, (void*)sig_handler);
	pthread_t uploader_thread;
	pthread_t detect_thread;
	pthread_t fan_thread;
     
    // MySQL connection
    connector = mysql_init(NULL);
    if (!mysql_real_connect(connector, DBHOST, DBUSER, DBPASS, DBNAME, 3306, NULL, 0))
    {
        fprintf(stderr, "%s\n", mysql_error(connector));
        return 0;
    }
    if (wiringPiSetup() == -1)
        exit(EXIT_FAILURE) ;

    if (wiringPiSetupGpio() == -1)
        fprintf(stdout, "Unable to start wiringPi :%s\n", strerror(errno));

    if (wiringPiSPISetup(SPI_CHANNEL, SPI_SPEED) == -1)
    {
        fprintf(stdout, "wiringPiSPISetup Failed :%s\n", strerror(errno));
        return 1;
    }

    if (setuid(getuid()) < 0)
    {
        perror("Dropping privileges failed\n");
        exit(EXIT_FAILURE);
    }
	pinMode(CS_MCP3208, OUTPUT);
   
    printf("MySQL(AWS) opened.\n"); 
	while(1){
		pthread_create(&detect_thread, NULL, detecting, NULL);
		pthread_create(&uploader_thread, NULL, upload_DB, NULL);
		pthread_create(&fan_thread, NULL, TurnningFAN, NULL);
		sleep(3);		
		pthread_join(detect_thread, NULL);
	    pthread_join(uploader_thread, NULL);
		pthread_join(fan_thread, NULL);
	}
    mysql_close(connector);
	
    return 0;
}

void sig_handler(int signo){
    mysql_close(connector);	
    printf("MYSQL is closed\n");
    digitalWrite(RGBLEDPOWER, 0);
	digitalWrite(FAN, 0);
	printf("process stop\n");
	exit(0);
}
