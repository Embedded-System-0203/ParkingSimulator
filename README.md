# ParkingSimulator

**초보 운전자를 위한 주차 시뮬레이터**

-   2022-2학기 임베디드시스템 2분반 3조 GNU (Group Name U기)

## Issue!! 개발 시 문제점 및 해결 방안

1. 2022.12.05 시리얼 포트 연결 이상 문제 : ~~보류~~ (2022.12.06 - 시리얼 to usb 모듈 구매)  
    ~~일부 해결~~ (2022.12.10) (유니티에서 라즈베리파이로 데이터 가져오는 과정에서 Segmentation fault 발생)  
    해결 (2022.12.11) (유니티 -> 라즈베리 파이 데이터 전송 세그멘테이션 오류 수정, 데이터 전송할 때 발생되는 이상한 공백문자들 자체가 안생기게 송수신 함수들을 새로 구현)


## 업무 분장

| 학번     | 이름   | 역할                                                             |
| -------- | ------ | ---------------------------------------------------------------- |
| 20180080 | 김건우 | 회로구성, 통신기능 개발, 멀티 프로세싱 기능 개발                 |
| 20180488 | 박준수 | 회로구성, 멀티 프로세싱 기능 개발, 회의 내용 기록 및 보고서 작성 |
| 20181294 | 황종현 | 유니티 개발, 통신기능 개발, 데모 영상                            |

-   역할 변동은 유연하게 있을 수 있음

## 아이디어 소개

  라즈베리파이와 유니티를 이용하여 초보운전자를 위한 주차 시뮬레이터를 제작하였다. 1인칭 시점이 아닌 3인칭 시점으로 위에서 아래로 내려다보는 느낌으로 시뮬레이터를 구성하여 실제 운전을 하기 이전 감각을 기르기 위한 시뮬레이터로 제작하였다.

## 전체 시스템 구조

<img width="1274" alt="design1" src="https://user-images.githubusercontent.com/58184008/208287581-a7111240-2339-4a09-bb9e-959ac47c13cd.png">
블루투스 UART 통신을 통하여 라즈베리파이에서는 가속도센서(steering wheel)의 값과 스위치로 눌러지는 값들을 pc로 지속적으로 보내게 된다.  
PC(unity)에서 또한 시뮬레이터 상에 나타나는 자동차의 충돌 여부나 경고 표시 여부를 계속해서 라즈베리파이로 보내게 된다.   
시뮬레이션의 종료 조건의 경우 유니티에서 라즈베리파이로 보내게 된다. 라즈베리파이 상에 돌아가고 있던 쓰레드들은 종료신호를 받으면 종료되며 모든 자식쓰레드가 종료 되면 메인 프로그램도 종료하게 된다.    

<br /><br /><br /><br />
<img width="1329" alt="design2" src="https://user-images.githubusercontent.com/58184008/208287587-c7c2752e-79d5-4e20-9dc5-f7f26b19af67.png">
시뮬레이터 컨트롤러의 구조는 다음과 같이 설계하였다.


## 개발 일정(12/3 ~ 12/15)

```mermaid
gantt
    title Parking Simulator
    dateFormat  YYYY-MM-DD
    section 조사
    자료 조사           :a1, 2022-12-03, 2d
    통신 테스트     :after a1  , 2d
    section 유니티
    유니티 개발     :a2, 2022-12-05, 7d
    section 회로 구성 및 프로그램 연동
    회로 구성     :a3, 2022-12-10, 2d
    데이터 처리 및 연동     :a4, 2022-12-11  , 3d
    section 최종 테스트
    최종 테스트    :after a4 , 2d
```

## 제한 사항 구현 내용 (멀티프로세스/쓰레드, IPC/뮤텍스)

### 1. 멀티 쓰레드 관련 구현 내용

```c
void *gearFunc(void *);     // 기어 스위치 처리 함수
void *pedalFunc(void *);    // 액셀, 브레이크 페달 처리 함수
void *warningFunc(void *);  // 주의 관련 처리 함수 (경고음부저, 경고표시LED)
void *steeringFunc(void *); // 핸들 처리 함수
```
자식 쓰레드에서 사용할 함수들을 각각 정의해준다.

```c
// ...
    pthread_t ptGear, ptPedal ,ptSteering, ptWarning;
    pthread_mutex_init(&mid, NULL);

    pthread_create(&ptGear, NULL, gearFunc, NULL);
    pthread_create(&ptPedal, NULL, pedalFunc, NULL);
    pthread_create(&ptWarning, NULL, warningFunc, NULL);
    pthread_create(&ptSteering, NULL, steeringFunc, NULL);
    
    // ...
    
    pthread_join(ptGear, NULL);
    pthread_join(ptPedal, NULL);
    pthread_join(ptWarning, NULL);
    pthread_join(ptSteering, NULL);

    pthread_mutex_destroy(&mid);
// ...
```

### 2. 뮤텍스 관련 구현 내용
```c
int gear = 0;  // 0: P, 1: R, 2: D
int pedal = 0; // 0: Brake, 1: Gas
int ch = 0;    // P, R, D, Brake Pedal, Gas Pedal
int crash = 0; // 자식 스레드들을 멈추고, 프로그램을 종료하기 위한 변수
```
해당 프로그램에서 사용하는 공유 변수들이다. 이 값들이 제대로 계산되지 않으면 쓰레드들이 종료되지 않아 영원히 프로그램이 끝나지 않을 수 있다.

```c
// ...
        if (serialDataAvail(fd_serial)){
            dat3 = serialRead(fd_serial);
            printf("받은 데이터 : %c\n", dat3);
        }

        if(dat3 == '7'){
            pthread_mutex_lock(&mid);
            crash++;
            pthread_mutex_unlock(&mid);
            break;
        }
// ...
```
쓰레드들끼리 충돌이 나서 연산이 되지 않는 것을 방지하기 위해 공유변수가 사용되는 임계 영역에 뮤텍스 lock을 걸어준다.

```c
void *gearFunc(void *arg){

    unsigned char dat3;
        
    while(1){
        if (serialDataAvail(fd_serial)){
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
```
여러 처리 함수 중 기어 변환 처리를 하는 함수인 gearFunc의 모습이다.



## 데모 영상

- **사고 버전**  
https://www.youtube.com/watch?v=ugl6CPhcHsQ

- **베스트 드라이버 버전**  
https://www.youtube.com/watch?v=FYrLk8sjMGM

## 회의 내용

-   2022.12.10(토)

0.  참석인원 : 김건우, 박준수, 황종현

1.  자동차 조작 방법

    -   스위치 (P,R,D, Gas Pedal, Brake Pedal) : 기어 변환은 Brake 눌려 있는지 확인 후 1번만 눌러서 기어 변환
        Gas Pedal, Brake Pedal은 계속 눌러서 실행하도록 함
    -   가속도 센서 : roll 값 범위를 기준으로 핸들(steering wheel) 방향 설정 -> \[6, 75\](CCW) \[-5, 5\](origin) \[-75, -6\](CW)
        센서에서 받아온 값을 변환하여 줄 것인가 or 해당 범위 안으로 들어오게 되면 값을 조금씩 더하여 서서히 돌릴 것인가 방법 결정 필요

2.  Thread 구성
    -   구성 : 가속도 센서 값 처리, LED와 스피커 값 처리, 기어 변환 처리, Pedal 변환 처리
    -   Lock 걸어줄 요소 : 기어 값
3.  기타
    -   맵 구성 : 주차 공간을 나타내는 선 이외에 다른 차(장애물)을 설치하여 충돌 시 게임 종료하도록 할 것
    -   통신 관련 이슈 : [세그멘테이션 오류](https://github.com/Embedded-System-0203/Uart_unity#%EB%9D%BC%EC%A6%88%EB%B2%A0%EB%A6%AC-%ED%8C%8C%EC%9D%B4-c-%EC%86%8C%EC%8A%A4%EC%BD%94%EB%93%9C)
