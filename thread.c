#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include <term.h>
#include <curses.h>


// !!!수정 사항!!!
// 맥 환경이라서 <conio.h>을 못써서 임시로 이렇게 구현
// gpio 사용 시 달라질 예정!!
static struct termios initial_settings, new_settings;
static int peek_character = -1;


pthread_mutex_t mid;
// pthread_mutex_t mch;

int distance = 0;

int share_var = 1;
int gear = 0; // 0: P, 1: R, 2: D
int pedal = 0; // 0: Brake, 1: Gas
float steering;
bool warning = false;
bool end = false;
int ch = 0;

int crash = 0;

void *gearFunc(void *);
void *pedalFunc(void *);
void *warningFunc(void *);
void *steeringFunc(void *);

// Mac issue
void init_keyboard()
{
    tcgetattr(0,&initial_settings);
    new_settings = initial_settings;
    new_settings.c_lflag &= ~ICANON;
    new_settings.c_lflag &= ~ECHO;
    // new_settings.c_lflag &= ~ISIG;
    new_settings.c_cc[VMIN] = 1;
    new_settings.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &new_settings);
}

// Mac issue
void close_keyboard()
{
    tcsetattr(0, TCSANOW, &initial_settings);
}

// Mac issue
int kbhit()
{
    char ch;
    int nread;

    if(peek_character != -1)
        return 1;
    new_settings.c_cc[VMIN]=0;
    tcsetattr(0, TCSANOW, &new_settings);
    nread = read(0,&ch,1);
    new_settings.c_cc[VMIN]=1;
    tcsetattr(0, TCSANOW, &new_settings);

    if(nread == 1) {
        peek_character = ch;
        return 1;
    }
    return 0;
}

// Mac issue
int readch()
{
    char ch;

    if(peek_character != -1) {
        ch = peek_character;
        peek_character = -1;
        return ch;
    }
    read(0,&ch,1);
    return ch;
}

int main(){
    pthread_t ptGear, ptPedal, ptWarning; //,ptSteering;
    pthread_mutex_init(&mid, NULL);
    // pthread_mutex_init(&mch, NULL);

    pthread_create(&ptGear, NULL, gearFunc, NULL);
    pthread_create(&ptPedal, NULL, pedalFunc, NULL);
    pthread_create(&ptWarning, NULL, warningFunc, NULL);
    // pthread_create(&ptSteering, NULL, steeringFunc, NULL);

    // !!Mac issue!!
    init_keyboard();
    while(ch != 'q') {
        // printf("looping\n");
        // sleep(1);
        if(kbhit()) {
            ch = readch();
            // printf("you hit %d\n",ch);
            // printf("you hit %c\n",ch);
            // printf("%d\n",gear);
            // printf("%c\n",ch);
        }
        if (crash >= 3){
            break;
        }
    }

    pthread_join(ptGear, NULL);
    pthread_join(ptPedal, NULL);
    pthread_join(ptWarning, NULL);
    // pthread_join(ptSteering, NULL);

    pthread_mutex_destroy(&mid);
    // pthread_mutex_destroy(&mch);

    close_keyboard();
    exit(0);

    return 0;
}

void *gearFunc(void *arg){
    while(1){
        if(ch == 100 && pedal == 0){
            pthread_mutex_lock(&mid);
            gear = 2;
            pthread_mutex_unlock(&mid);
            usleep(30000);
        }
        else if(ch == 114 && pedal == 0){
            pthread_mutex_lock(&mid);
            gear = 1;
            pthread_mutex_unlock(&mid);
            usleep(30000);

        }
        else if(ch == 112 && pedal == 0){
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
void *steeringFunc(void *arg){}
