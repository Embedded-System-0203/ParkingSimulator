#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
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
#define LED_BACK 13
#define BUZZER 2

pthread_mutex_t mid;

int fd_serial ; //UART2 파일 서술자
int distance = 0;
int share_var = 1;
int gear = 0; // 0: P, 1: R, 2: D
int pedal = 0; // 0: Brake, 1: Gas
float steering;
int ch = 0;
int crash = 0;

static const char* UART2_DEV = "/dev/ttyAMA1";
unsigned char serialRead(const int fd);
void serialWrite(const int fd, const unsigned char *c);

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


void readRegister_ADXL345(char registerAddress, int numBytes, char * values)
{

    values[0] = 0x80 | registerAddress;
    if(numBytes > 1) values[0] = values[0] | 0x40;
    digitalWrite(CS_GPIO, LOW);
    wiringPiSPIDataRW(SPI_CH, values, numBytes + 1);
    digitalWrite(CS_GPIO, HIGH);
}

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
    pinMode(LED_BACK, OUTPUT);
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

    digitalWrite(BUZZER, LOW);
    digitalWrite(LED_L, LOW);
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_BACK, LOW);

    pthread_t ptGear, ptPedal ,ptSteering, ptWarning;
    pthread_mutex_init(&mid, NULL);

    pthread_create(&ptGear, NULL, gearFunc, NULL);
    pthread_create(&ptPedal, NULL, pedalFunc, NULL);
    pthread_create(&ptWarning, NULL, warningFunc, NULL);
    pthread_create(&ptSteering, NULL, steeringFunc, NULL);

    while(1)
    {
        if(digitalRead(P))
        {
            printf("P\n");
            pthread_mutex_lock(&mid);
            ch = 112;
            pthread_mutex_unlock(&mid);
            
            delay(100);
        }
        if(digitalRead(R))
        {
            printf("R\n");
            pthread_mutex_lock(&mid);
            ch = 114;
            pthread_mutex_unlock(&mid);
            delay(100);
        }
        if(digitalRead(D))
        {
            printf("D\n");
            pthread_mutex_lock(&mid);
            ch = 100;
            pthread_mutex_unlock(&mid);
            delay(100);
        }
        if(digitalRead(G))
        {
            printf("G\n");
            pthread_mutex_lock(&mid);
            ch = 103;
            pthread_mutex_unlock(&mid);
            delay(100);
        }
        if(digitalRead(B))
        {
            printf("B\n");
            pthread_mutex_lock(&mid);
            ch = 98;
            pthread_mutex_unlock(&mid);
            delay(100);
        }
        if (crash >= 4){
            break;
        }
        
    }

    pthread_join(ptGear, NULL);
    pthread_join(ptPedal, NULL);
    pthread_join(ptWarning, NULL);
    pthread_join(ptSteering, NULL);

    pthread_mutex_destroy(&mid);

    delay(2000);
    digitalWrite(BUZZER, LOW);
    digitalWrite(LED_L, LOW);
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_BACK, LOW);
    return 0;
}

void *gearFunc(void *arg){

    unsigned char dat3;
        
    while(1){
        if (serialDataAvail(fd_serial))
        {
            dat3 = serialRead(fd_serial);
            printf("받은 데이터 : %c\n", dat3);
        }

        if(dat3 == '7'){
            pthread_mutex_lock(&mid);
            crash++;
            pthread_mutex_unlock(&mid);
            break;
        }
        if(ch == 100 && pedal == 0){
            serialWrite(fd_serial, "D\n");
            pthread_mutex_lock(&mid);
            gear = 2;
            pthread_mutex_unlock(&mid);
            delay(200);
            pthread_mutex_lock(&mid);
            ch = 0;
            pthread_mutex_unlock(&mid);
            delay(200);
        }
        else if(ch == 114 && pedal == 0){
            serialWrite(fd_serial, "R\n");
            pthread_mutex_lock(&mid);
            gear = 1;
            pthread_mutex_unlock(&mid);
            delay(200);
            pthread_mutex_lock(&mid);
            ch = 0;
            pthread_mutex_unlock(&mid);
            delay(200);
        }
        else if(ch == 112 && pedal == 0){
            serialWrite(fd_serial, "P\n");
            pthread_mutex_lock(&mid);
            gear = 0;
            pthread_mutex_unlock(&mid);
            delay(200);
            pthread_mutex_lock(&mid);
            ch = 0;
            pthread_mutex_unlock(&mid);
            delay(200);
        }
    }
}
void *pedalFunc(void *arg){

    unsigned char dat3;
        
    while(1){
        if (serialDataAvail(fd_serial))
        {
            dat3 = serialRead(fd_serial);
            printf("받은 데이터 : %c\n", dat3);
        }

        if(dat3 == '7'){
            pthread_mutex_lock(&mid);
            crash++;
            pthread_mutex_unlock(&mid);
            break;
        }
        if(gear == 2 && ch == 103){
            serialWrite(fd_serial, "G\n");
            pthread_mutex_lock(&mid);
            pedal = 1;
            pthread_mutex_unlock(&mid);
            pthread_mutex_lock(&mid);
            distance++;
            pthread_mutex_unlock(&mid);
            delay(200);
            pthread_mutex_lock(&mid);
            ch = 0;
            pthread_mutex_unlock(&mid);
            delay(200);
            printf("%d\n",distance);
        }
        else if(gear == 1 && ch == 103){
            serialWrite(fd_serial, "G\n");
            pthread_mutex_lock(&mid);
            pedal = 1;
            pthread_mutex_unlock(&mid);
            pthread_mutex_lock(&mid);
            distance--;
            pthread_mutex_unlock(&mid);
            delay(200);
            pthread_mutex_lock(&mid);
            ch = 0;
            pthread_mutex_unlock(&mid);
            delay(200);
            printf("%d\n",distance);
        }
        if(ch == 98){
            serialWrite(fd_serial, "B\n");
            pthread_mutex_lock(&mid);
            pedal = 0;
            pthread_mutex_unlock(&mid);
            delay(200);
        }
    }
    return NULL;
}

void *warningFunc(void *arg){
    unsigned char dat3;
    while(1){

        if (serialDataAvail(fd_serial))
        {
            dat3 = serialRead(fd_serial);
            printf("받은 데이터 : %c\n", dat3);
        }

        if(dat3== '1'){
            digitalWrite(LED_L, HIGH);
            
        }
        if(dat3== '2'){//off
            digitalWrite(LED_L, LOW);

        }
        if(dat3== '3'){
            digitalWrite(LED_R, HIGH);

        }
        if(dat3== '4'){//off
            digitalWrite(LED_R, LOW);

        }
        if(dat3== '5'){
            digitalWrite(LED_BACK, HIGH);
            
        }
        if(dat3== '6'){//off
            digitalWrite(LED_BACK, LOW);
        }
        if (digitalRead(LED_L) || digitalRead(LED_R) || digitalRead(LED_BACK)){
            if (!digitalRead(BUZZER)){
                digitalWrite(BUZZER, HIGH);
            }
        }
        if (!digitalRead(LED_L) && !digitalRead(LED_R) && !digitalRead(LED_BACK)){
            if (digitalRead(BUZZER)){
                digitalWrite(BUZZER, LOW);
            }
        }
        if(dat3 == '7'){
            pthread_mutex_lock(&mid);
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
    double threshold = 75.0;

    unsigned char dat3;
        
    while(1){
        if (serialDataAvail(fd_serial))
        {
            dat3 = serialRead(fd_serial);
            printf("받은 데이터 : %c\n", dat3);
        }

        if(dat3 == '7'){
            pthread_mutex_lock(&mid);
            crash++;
            pthread_mutex_unlock(&mid);
            break;
        }

        readRegister_ADXL345(DATAX0,6,buffer);
        x = ((short)buffer[2]<<8)|(short)buffer[1]; //X
        x_ = (float)x * 3.9 * 0.001;
        y = ((short)buffer[4]<<8)|(short)buffer[3]; //Y
        y_ = (float)y * 3.9 * 0.001;
        z = ((short)buffer[6]<<8)|(short)buffer[5]; //Z
        z_ = (float)z * 3.9 * 0.001;
        roll = 180 * atan(y_/sqrt(x_*x_ + z_*z_))/PI;
        pitch = 180 * atan(x_/sqrt(y_*y_ + z_*z_))/PI;

        // roll : -75 ~ 75 (이 범위에서의 값만 전송되도록 처리)/ -5 ~ 5 --> 0 (이건 unity에서 처리)
        if (roll > threshold)
            roll = threshold;
        if (roll < -threshold)
            roll = -threshold;

        char str[100];
        sprintf(str, "%f\n", roll);

        serialWrite(fd_serial, str);

        printf("roll : %f\n", roll);

        delay(200);
    }
    return NULL;
}