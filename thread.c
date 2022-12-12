#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
// #include <termios.h>
// #include <term.h>
// #include <curses.h>
#include <wiringPi.h>
#include <string.h>
#include <wiringPiSPI.h>
#include <wiringSerial.h>
#include <math.h>

#define CS_GPIO 8 //CS
#define SPI_CH 0
#define SPI_SPEED 1000000 // 1MHz
#define SPI_MODE 3
#define BW_RATE 0x2C //
#define POWER_CTL 0x2D //Power Control Register
#define DATA_FORMAT 0x31
#define DATAX0 0x32 //X-Axis Data 0
#define PI 3.141592
#define BAUD_RATE 115200
#define P 18
#define R 23
#define D 24
#define B 17
#define G 27
#define LED_R 5
#define LED_L 6
#define BUZZER 21

// !!!수정 사항!!!
// 맥 환경이라서 <conio.h>을 못써서 임시로 이렇게 구현
// gpio 사용 시 달라질 예정!!
// static struct termios initial_settings, new_settings;
// static int peek_character = -1;


pthread_mutex_t mid;
// pthread_mutex_t mch;

int fd_serial ; //UART2 파일 서술자
int distance = 0;
int share_var = 1;
int gear = 0; // 0: P, 1: R, 2: D
int pedal = 0; // 0: Brake, 1: Gas
float steering;
// bool warning = false;
// bool end = false;
int ch = 0;

int crash = 0;

static const char* UART2_DEV = "/dev/ttyAMA1"; //UART2 연결을 위한 장치 파일
unsigned char serialRead(const int fd); //1Byte 데이터를 수신하는 함수
void serialWrite(const int fd, const unsigned char *c); //1Byte 데이터를 송신하는 함수

//1Byte 데이터를 수신하는 함수
unsigned char serialRead(const int fd)
{
    unsigned char x;
    if(read (fd, &x, 1) != 1) //read 함수를 통해 1바이트 읽어옴
        return -1;
    return x; //읽어온 데이터 반환
}

void serialWrite(const int fd, const unsigned char *c)
{
    write (fd, c, strlen(c));
}

//ADXL345
void readRegister_ADXL345(char registerAddress, int numBytes, char * values)
{
    values[0] = 0x80 | registerAddress;
    if(numBytes > 1) values[0] = values[0] | 0x40;
    digitalWrite(CS_GPIO, LOW); // Low : CS Active
    wiringPiSPIDataRW(SPI_CH, values, numBytes + 1);
    digitalWrite(CS_GPIO, HIGH); // High : CS Inactive
}
//ADXL345
void writeRegister_ADXL345(char address, char value)
{
    unsigned char buff[2];
    buff[0] = address;
    buff[1] = value;
    digitalWrite(CS_GPIO, LOW); // Low : CS Active
    wiringPiSPIDataRW(SPI_CH, buff, 2);
    digitalWrite(CS_GPIO, HIGH); // High : CS Inactive
}

void *gearFunc(void *);
void *pedalFunc(void *);
void *warningFunc(void *);
void *steeringFunc(void *);

int main(){
    if (wiringPiSetupGpio () < 0) return 1;
    if(wiringPiSPISetupMode(SPI_CH, SPI_SPEED,SPI_MODE) == -1) return 1;
    pinMode(P, INPUT);
    pinMode(R, INPUT);
    pinMode(D, INPUT);
    pinMode(G, INPUT);
    pinMode(B, INPUT);
    pinMode(LED_L, OUTPUT);
    pinMode(LED_R, OUTPUT);
    pinMode(BUZZER,OUTPUT);
    
    unsigned char dat[100]; //데이터 임시 저장 변수
    unsigned char dat2;
    pinMode(CS_GPIO, OUTPUT); //Chip Select OUTPUT
    digitalWrite(CS_GPIO, HIGH); //IDLE
    writeRegister_ADXL345(DATA_FORMAT,0x09); // +- 4G
    writeRegister_ADXL345(BW_RATE,0x0C); //Output Data Rage 400 Hz
    writeRegister_ADXL345(POWER_CTL,0x08); //

    if ((fd_serial = serialOpen (UART2_DEV, BAUD_RATE)) < 0){ //UART2 포트 오픈
        printf ("Unable to open serial device.\n");
        return 1;
    }

    pthread_t ptGear, ptPedal ,ptSteering; // , ptWarning;
    pthread_mutex_init(&mid, NULL);
    // // pthread_mutex_init(&mch, NULL);

    pthread_create(&ptGear, NULL, gearFunc, NULL);
    pthread_create(&ptPedal, NULL, pedalFunc, NULL);
    //pthread_create(&ptWarning, NULL, warningFunc, NULL);
    pthread_create(&ptSteering, NULL, steeringFunc, NULL);

    while(1)
    {
        if(digitalRead(P))
        {
            //digitalWrite(gpioOut, LOW);
            printf("P\n");
            ch = 112;
            delay(500);
        }
        if(digitalRead(R))
        {
            //digitalWrite(gpioOut, LOW);
            printf("R\n");
            ch = 114;
            delay(500);
        }
        if(digitalRead(D))
        {
            //digitalWrite(gpioOut, LOW);
            printf("D\n");
            ch = 100;
            delay(500);
        }
        if(digitalRead(G))
        {
            //digitalWrite(gpioOut, LOW);
            printf("G\n");
            ch = 103;
            delay(100);
        }
        if(digitalRead(B))
        {
            //digitalWrite(gpioOut, LOW);
            printf("B\n");
            ch = 98;
            delay(500);
        }
        if (crash >= 3){
            break;
        }
        
    }

    pthread_join(ptGear, NULL);
    pthread_join(ptPedal, NULL);
    // pthread_join(ptWarning, NULL);
    pthread_join(ptSteering, NULL);

    pthread_mutex_destroy(&mid);
    // // pthread_mutex_destroy(&mch);

    // close_keyboard();
    // exit(0);

    return 0;
}

void *gearFunc(void *arg){
    while(1){
        if(ch == 100 && pedal == 0){
            serialWrite(fd_serial, "D\n");
            pthread_mutex_lock(&mid);
            gear = 2;
            pthread_mutex_unlock(&mid);
            usleep(30000);
        }
        else if(ch == 114 && pedal == 0){
            serialWrite(fd_serial, "R\n");
            pthread_mutex_lock(&mid);
            gear = 1;
            pthread_mutex_unlock(&mid);
            usleep(30000);

        }
        else if(ch == 112 && pedal == 0){
            serialWrite(fd_serial, "P\n");
            pthread_mutex_lock(&mid);
            gear = 0;
            pthread_mutex_unlock(&mid);
            usleep(30000);

        }
        if (distance < 0 || distance > 30){
            pthread_mutex_lock(&mid);
            printf("Crash!!!!");
            crash++;
            pthread_mutex_unlock(&mid);
            break;
        }
    }
    return NULL;
}

void *pedalFunc(void *arg){

    while(1){
        if(gear == 2 && ch == 103){
            serialWrite(fd_serial, "G\n");
            pthread_mutex_lock(&mid);
            pedal = 1;
            pthread_mutex_unlock(&mid);
            pthread_mutex_lock(&mid);
            distance++;
            pthread_mutex_unlock(&mid);
            usleep(30000);
            pthread_mutex_lock(&mid);
            ch = 0;
            pthread_mutex_unlock(&mid);
            usleep(10000);
            printf("%d\n",distance);
        }
        else if(gear == 1 && ch == 103){
            pthread_mutex_lock(&mid);
            pedal = 1;
            pthread_mutex_unlock(&mid);
            pthread_mutex_lock(&mid);
            distance--;
            pthread_mutex_unlock(&mid);
            usleep(30000);
            pthread_mutex_lock(&mid);
            ch = 0;
            pthread_mutex_unlock(&mid);
            usleep(10000);
            printf("%d\n",distance);
        }
        if(ch == 98){
            serialWrite(fd_serial, "B\n");
            pthread_mutex_lock(&mid);
            pedal = 0;
            pthread_mutex_unlock(&mid);
            usleep(10000);
        }
        if (distance < 0 || distance > 30){
            pthread_mutex_lock(&mid);
            printf("Crash!!!!");
            crash++;
            pthread_mutex_unlock(&mid);
            break;
        }
    }
    return NULL;
}

void *warningFunc(void *arg){
    while(1){
        if (distance >= 26){
            printf("Warning!!!\n");
            usleep(30000);
        }
        
        if (distance < 0 || distance > 30){
            pthread_mutex_lock(&mid);
            printf("Crash!!!!");
            crash++;
            pthread_mutex_unlock(&mid);
            break;
        }
    }
    return NULL;
}

void *steeringFunc(void *arg){
    unsigned char buffer[100];
    short x, y= 0 , z= 0;
    float x_, y_, z_;
    double roll, pitch;
    while(1){

        readRegister_ADXL345(DATAX0,6,buffer);
        x = ((short)buffer[2]<<8)|(short)buffer[1]; //X
        x_ = (float)x * 3.9 * 0.001;
        y = ((short)buffer[4]<<8)|(short)buffer[3]; //Y
        y_ = (float)y * 3.9 * 0.001;
        z = ((short)buffer[6]<<8)|(short)buffer[5]; //Z
        z_ = (float)z * 3.9 * 0.001;
        roll = 180 * atan(y_/sqrt(x_*x_ + z_*z_))/PI;
        pitch = 180 * atan(x_/sqrt(y_*y_ + z_*z_))/PI;

        char str[100];
        sprintf(str, "%f\n", roll);
        serialWrite(fd_serial, str);
        printf("roll : %f\n", roll);
        delay(200);
        
        if (distance < 0 || distance > 30){
            pthread_mutex_lock(&mid);
            printf("Crash!!!!");
            crash++;
            pthread_mutex_unlock(&mid);
            break;
        }
    }
    return NULL;
}