#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include <sys/sem.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/time.h>
#include <poll.h>
#include <signal.h>
#include "Semaphore.h"
#include "Matrix.h"

extern int SO_BASE;
extern int SO_ALTEZZA;
extern long SO_MIN_HOLD_NSEC;
extern int SO_N_MOVES;
extern int flagCount;
extern int SO_NUM_P;
extern int SO_NUM_G;
extern int *communication;
extern int communicationSemG;
extern int masterSemG;

int stopTime;
int endGame;
int changeTarget;
int target;
int *removed;

struct sigaction{
  void(*sa_handler)(int signum);
  sigset_t sa_mask;
  int sa_flags;
};

struct timespec{
	time_t tv_sec;
	long tv_nsec;
};

void Pathfind(int id,int *memoryLocation,int pawnPipe[],int parentPipe[],int masterPipe[],int *positionX,int *positionY,int targetX,int targetY,int pId,int number);     /*Function used to start moving to the objective*/
short MoveXTarget(int id,int *memoryLocation,int *positionX,int *positionY,int targetX,int targetY,int first,int *dir);   /*Function used to move in the direction of the target X*/
short MoveYTarget(int id,int *memoryLocation,int *positionX,int *positionY,int targetX,int targetY,int first,int *dir);   /*Function used to move in the direction of the target Y*/
short MoveAntiYTarget(int id,int *memoryLocation,int *positionX,int *positionY,int targetX,int targetY);                  /*Function used to move in the opposite direction of the target X*/
short MoveAntiXTarget(int id,int *memoryLocation,int *positionX,int *positionY,int targetX,int targetY);                  /*Function used to move in the opposite direction of the raget Y*/

void endCommandHandler(int sig){
            flagCount=0;
            stopTime=1;
            endGame=1;
}

void captureHandler(int sig){
    flagCount--;
    if(*communication==target)
        changeTarget=1;
    releaseSem(communicationSemG,0);
}

void StartPawn(int matrixSem,int roundSem,int endRoundSem,int clearPhaseSem,int startRoundSem,int *memoryLocation,int pawnNumber,int playerNumber,int pawnPipe[],int parentPipe[],int masterPipe[]){
    int targetX,targetY,numRead,positionX,positionY,tmp[2];
    struct sigaction sa,sa2;
    sigset_t my_mask,my_mask2;
    posToMatrix(*memoryLocation,tmp);
    positionX=tmp[0];
    positionY=tmp[1];
    sa.sa_handler=&endCommandHandler;
    sa.sa_flags=0;
    sigemptyset(&my_mask);
    sa.sa_mask=my_mask;
    sa2.sa_handler=&captureHandler;
    sa2.sa_flags=0;
    sigemptyset(&my_mask2);
    sa2.sa_mask=my_mask2;
    if(sigaction(SIGUSR1,&sa,NULL)==-1){
        printf("Errore nell'inserimento della sigaction\n");
        exit(0);
    }
    if(sigaction(SIGUSR2,&sa2,NULL)==-1){
        printf("Errore nell'inserimento della sigaction\n");
        exit(0);
    }
    endGame=0;
    changeTarget=0;
    while(!endGame){
        char msg[25];
        struct pollfd x;
        char buff[12];
        char *ptr;
        int error=0;
        stopTime=0;
        do{
            error=0;
            if(reserveSem(roundSem,0)==-1){
                error=1;
            }
        }while(error);
        /*
         * Setup phase
         */
        numRead=read(pawnPipe[0],buff, sizeof(buff));
        if(numRead==-1){
            printf("Errore nella lettura \n");
        }
        if(numRead==0){
            return;
        }
        flagCount=strtol(buff, &ptr, 10);
        numRead=read(pawnPipe[0],buff, sizeof(buff));
        if(numRead==-1){
            printf("Errore nella lettura \n");
        }
        if(numRead==0){
            return;
        }
        target=strtol(buff, &ptr, 10);

        numRead=read(pawnPipe[0],buff, sizeof(buff));
        if(numRead==-1){
            printf("Errore nella lettura \n");
        }
        if(numRead==0){
            return;
        }
        targetX=strtol(buff, &ptr, 10);
        numRead=read(pawnPipe[0],buff, sizeof(buff));
        if(numRead==-1){
            printf("Errore nella lettura \n");
        }
        if(numRead==0){
            return;
        }
        targetY=strtol(buff, &ptr, 10);
        sprintf(buff,"%d",targetY);
        if(write(parentPipe[1],buff, sizeof(buff))==-1){
            printf("Errore nella scrittura su parentpipe %s\n",strerror(errno));
        }
        /*
         * End Of Setup phase
         */
        do{
            error=0;
            error=reserveSem(startRoundSem,0);
        }while(error==-1);
        /*
         *  Start of the round
         */
        stopTime=0;
        changeTarget=0;
        if(target!=SO_NUM_P*SO_NUM_G+1)
        Pathfind(matrixSem,memoryLocation,pawnPipe,parentPipe,masterPipe,&positionX,&positionY,targetX,targetY,pawnNumber+(playerNumber*SO_NUM_P),pawnNumber);
        if(releaseSem(endRoundSem,0)==-1){
            printf("Errore nella release sem %s\n",strerror(errno));
            exit(0);
        }
        /*
         *  End of the round
         */
        do{
            error=0;
            if(reserveSem(clearPhaseSem,0)==-1){
                error=1;
            }
        }while(error);
        /*
         *  Start of the cleaning phase
         */
        sprintf(msg,"%d-%d",pawnNumber+(playerNumber*SO_NUM_P),SO_N_MOVES);
        do{
            error=0;
            error=write(masterPipe[1],msg, sizeof(msg));
        }while(error==-1);
        x.fd=pawnPipe[0];
        x.events=POLLIN;
        while(poll(&x, 1, 0)==1) {
            read(pawnPipe[0],buff, sizeof(buff));
        }
        /*
         *  End of the cleaning phase
         */
        if(releaseSem(endRoundSem,0)==-1){
            printf("Errore nella release sem %s\n",strerror(errno));
            exit(0);
        }
    }
}

void Pathfind(int id,int *memoryLocation,int pawnPipe[],int parentPipe[],int masterPipe[],int * positionX,int * positionY,int targetX,int targetY,int pId,int number){
    int dir=0;

    while((*positionX!=targetX || *positionY!=targetY) && flagCount>0 && !changeTarget && SO_N_MOVES>0){
        /*
         * While we didn't reach the target and the game isn't over and we don't have to change target and we still have moves
         * we try to move in the target direction
         */
        if(dir==0){
            MoveXTarget(id,memoryLocation,positionX,positionY,targetX,targetY,1,&dir);
            if(*positionX==targetX){
                dir=1;
            }
        }else{
            MoveYTarget(id,memoryLocation,positionX,positionY,targetX,targetY,1,&dir);
            if(*positionY==targetY){
                dir=0;
            }
        }
    }
    if(SO_N_MOVES<=0){
        stopTime=1;
    }
    if(flagCount>0 && !changeTarget && *positionX==targetX && *positionY==targetY){
        char msg[25];
        /*
         *  Target conquered
         */
        sprintf(msg,"%d-%d",pId,target);
        if(write(masterPipe[1],msg, sizeof(msg))==-1){
            printf("Errore nella scrittura del messaggio: %s\n",strerror(errno));
        }
        return;
    }
    if(changeTarget && flagCount>0){
        /*
         *  Start request for a new target
         */
        int numRead;
        char buff[12];
        char *ptr;
        short fail=0;
        sprintf(buff,"%d",number);
        write(parentPipe[1],buff, sizeof(buff));
        changeTarget=0;
        if(flagCount>0){
            do{
                fail=0;
                numRead=read(pawnPipe[0],buff, sizeof(buff));
                if(numRead==-1){
                    fail=1;
                }
                if(numRead==0){
                    return;
                }
            }while(fail && flagCount>0);
            if(flagCount>0)
                target=strtol(buff, &ptr, 10);
        }
        if(flagCount>0){
            do{
                fail=0;
                numRead=read(pawnPipe[0],buff, sizeof(buff));
                if(numRead==-1){
                    fail=1;
                }
                if(numRead==0){
                    return;
                }
            }while(fail && flagCount>0);
            if(flagCount>0)
                targetX=strtol(buff, &ptr, 10);
        }
        if(flagCount>0){
            do{
                fail=0;
                numRead=read(pawnPipe[0],buff, sizeof(buff));
                if(numRead==-1){
                    fail=1;
                }
                if(numRead==0){
                    return;
                }
            }while(fail && flagCount>0);
            if(flagCount>0)
                targetY=strtol(buff, &ptr, 10);
        }

        if(flagCount>0){
            Pathfind(id,memoryLocation,pawnPipe,parentPipe,masterPipe,positionX,positionY,targetX,targetY,pId,number);
        }
        /*
         *  End request for a new target
         */
    }
}



short MoveXTarget(int id,int *memoryLocation,int *positionX,int *positionY,int targetX,int targetY,int first,int *dir){
    struct timespec tim, tim2;
    short controlValue;
    if(*positionX>targetX){
        if(*positionX-1<0 && flagCount>0 && !changeTarget){
            return 0;
        }
        if((controlValue=nonBlockingReserveSem(id,matrixToPos(*positionX-1,*positionY)))<0){
                if(first && flagCount>0 && !changeTarget){
                    short check=MoveYTarget(id,memoryLocation,positionX,positionY,targetX,targetY,0,dir);
                    if(!check)
                        check=MoveAntiYTarget(id,memoryLocation,positionX,positionY,targetX,targetY);
                    if(!check){
                        *dir=1;
                    }
                    return 0;
                }else{
                    return 0;
                }
        }else{
            if(releaseSem(id,matrixToPos(*positionX,*positionY))==-1){
                printf("Errore nella release sem MOVEXTARGET: %s\n",strerror(errno));
                exit(1);
            }
            *positionX=*positionX-1;
            *memoryLocation=matrixToPos(*positionX,*positionY);SO_N_MOVES=SO_N_MOVES-1;
        }
    }else if(*positionX<targetX){
        if(*positionX+1>=SO_ALTEZZA && flagCount>0 && !changeTarget){
            return 0;
        }
        if((controlValue=nonBlockingReserveSem(id,matrixToPos(*positionX+1,*positionY)))<0){
            if(controlValue==-1){
                printf("Errore nella Reserve sem MOVEXTARGET: %s\n",strerror(errno));
                exit(1);
            }else{
                if(first && flagCount>0 && !changeTarget){
                    short check=MoveYTarget(id,memoryLocation,positionX,positionY,targetX,targetY,0,dir);
                    if(!check) {
                        check=MoveAntiYTarget(id,memoryLocation,positionX,positionY,targetX,targetY);
                    }
                    if(!check){
                        *dir=1;
                    }
                    return 0;
                }else{
                    return 0;
                }
            }
        }else{
            if(releaseSem(id,matrixToPos(*positionX,*positionY))==-1){
                printf("Errore nella release sem MOVEXTARGET: %s\n",strerror(errno));
                exit(1);
            }
            *positionX=*positionX+1;
            *memoryLocation=matrixToPos(*positionX,*positionY);SO_N_MOVES=SO_N_MOVES-1;
        }
    }else{
        int randNum=rand() % 2 + 1;
        if(randNum==1){
            if(*positionX-1<0 && flagCount>0 && !changeTarget){
                return 0;
            }
            if((controlValue=nonBlockingReserveSem(id,matrixToPos(*positionX-1,*positionY)))<0){
                return 0;
            }else{
                if(releaseSem(id,matrixToPos(*positionX,*positionY))==-1){
                    printf("Errore nella release sem MOVEXTARGET: %s\n",strerror(errno));
                    exit(1);
                }
                *positionX=*positionX-1;
                *memoryLocation=matrixToPos(*positionX,*positionY);SO_N_MOVES=SO_N_MOVES-1;
            }
        }else{
            if(*positionX+1>=SO_ALTEZZA && flagCount>0 && !changeTarget){
                return 0;
            }
            if((controlValue=nonBlockingReserveSem(id,matrixToPos(*positionX+1,*positionY)))<0){
                    return 0;
            }else{
                if(releaseSem(id,matrixToPos(*positionX,*positionY))==-1){
                    printf("Errore nella release sem MOVEXTARGET: %s\n",strerror(errno));
                    exit(1);
                }
                *positionX=*positionX+1;
                *memoryLocation=matrixToPos(*positionX,*positionY);SO_N_MOVES=SO_N_MOVES-1;
            }
        }

    }
    tim.tv_sec = 0;
    tim.tv_nsec = SO_MIN_HOLD_NSEC;
    nanosleep(&tim,&tim2);
    return 1;
}

short MoveYTarget(int id,int *memoryLocation,int *positionX,int *positionY,int targetX,int targetY,int first,int *dir){
    struct timespec tim, tim2;
    short controlValue;
    if(*positionY>targetY && flagCount>0 && !changeTarget){
        if(*positionY-1<0){
            return 0;
        }
        if((controlValue=nonBlockingReserveSem(id,matrixToPos(*positionX,*positionY-1)))<0){
                if(first && flagCount>0 && !changeTarget){
                    short check=MoveXTarget(id,memoryLocation,positionX,positionY,targetX,targetY,0,dir);
                    if(!check)
                        check=MoveAntiXTarget(id,memoryLocation,positionX,positionY,targetX,targetY);
                    if(!check)
                        *dir=1;
                    return 0;
                }else{
                    return 0;
                }
        }else{
            if(releaseSem(id,matrixToPos(*positionX,*positionY))==-1){
                printf("Errore nella release sem MOVEXTARGET: %s\n",strerror(errno));
                exit(1);
            }
            *positionY=*positionY-1;
            *memoryLocation=matrixToPos(*positionX,*positionY);SO_N_MOVES=SO_N_MOVES-1;
        }
    }else if(*positionY<targetY){
        if(*positionY+1>=SO_BASE && flagCount>0 && !changeTarget){
            return 0;
        }
        if((controlValue=nonBlockingReserveSem(id,matrixToPos(*positionX,*positionY+1)))<0){
                if(first && flagCount>0 && !changeTarget){
                    short check=MoveXTarget(id,memoryLocation,positionX,positionY,targetX,targetY,0,dir);
                    if(!check)
                        check=MoveAntiXTarget(id,memoryLocation,positionX,positionY,targetX,targetY);
                    if(!check)
                        *dir=1;
                    return 0;
                }else{
                    return 0;
                }
        }else{
            if(releaseSem(id,matrixToPos(*positionX,*positionY))==-1){
                printf("Errore nella release sem MOVEXTARGET: %s\n",strerror(errno));
                exit(1);
            }
            *positionY=*positionY+1;
            *memoryLocation=matrixToPos(*positionX,*positionY);SO_N_MOVES=SO_N_MOVES-1;
        }
    }else{
        int randNum=rand() % 2 + 1;
        if(randNum==1){
            if(*positionY-1<0 && flagCount>0 && !changeTarget){
                return 0;
            }
            if((controlValue=nonBlockingReserveSem(id,matrixToPos(*positionX,*positionY-1)))<0){
                    return 0;
            }else{
                if(releaseSem(id,matrixToPos(*positionX,*positionY))==-1){
                    printf("Errore nella release sem MOVEXTARGET: %s\n",strerror(errno));
                    exit(1);
                }
                *positionY=*positionY-1;
                *memoryLocation=matrixToPos(*positionX,*positionY);SO_N_MOVES=SO_N_MOVES-1;
            }
        }else{
            if(*positionY+1>=SO_BASE && flagCount>0 && !changeTarget){
                return 0;
            }
            if((controlValue=nonBlockingReserveSem(id,matrixToPos(*positionX,*positionY+1)))<0){
                    return 0;
            }else{
                if(releaseSem(id,matrixToPos(*positionX,*positionY))==-1){
                    printf("Errore nella release sem MOVEXTARGET: %s\n",strerror(errno));
                    exit(1);
                }
                *positionY=*positionY+1;
                *memoryLocation=matrixToPos(*positionX,*positionY);SO_N_MOVES=SO_N_MOVES-1;
            }
        }
    }
    tim.tv_sec = 0;
    tim.tv_nsec = SO_MIN_HOLD_NSEC;
    nanosleep(&tim,&tim2);
    return 1;
}

short MoveAntiYTarget(int id,int*memoryLocation,int *positionX,int *positionY,int targetX,int targetY){
    struct timespec tim, tim2;
    short controlValue;
    if(*positionY>targetY){
        if(*positionY+1>=SO_BASE && flagCount>0 && !changeTarget){
            return 0;
        }
        if((controlValue=nonBlockingReserveSem(id,matrixToPos(*positionX,*positionY+1)))<0){
                return 0;
        }else{
            if(releaseSem(id,matrixToPos(*positionX,*positionY))==-1){
                printf("Errore nella release sem MOVEXTARGET: %s\n",strerror(errno));
                exit(1);
            }
            *positionY=*positionY+1;
            *memoryLocation=matrixToPos(*positionX,*positionY);SO_N_MOVES=SO_N_MOVES-1;
        }
    }else if(*positionY<targetY){
        if(*positionY-1<0 && flagCount>0 && !changeTarget){
            return 0;
        }
        if((controlValue=nonBlockingReserveSem(id,matrixToPos(*positionX,*positionY-1)))<0){
                return 0;
        }else{
            if(releaseSem(id,matrixToPos(*positionX,*positionY))==-1){
                printf("Errore nella release sem MOVEXTARGET: %s\n",strerror(errno));
                exit(1);
            }
            *positionY=*positionY-1;
            *memoryLocation=matrixToPos(*positionX,*positionY);SO_N_MOVES=SO_N_MOVES-1;
        }
    }else{
        int randNum=rand() % 2 + 1;
        if(randNum==1){
            if(*positionY-1<0 && flagCount>0 && !changeTarget){
                return 0;
            }
            if((controlValue=nonBlockingReserveSem(id,matrixToPos(*positionX,*positionY-1)))<0){
                    return 0;
            }else{
                if(releaseSem(id,matrixToPos(*positionX,*positionY))==-1){
                    printf("Errore nella release sem MOVEXTARGET: %s\n",strerror(errno));
                    exit(1);
                }
                *positionY=*positionY-1;
                *memoryLocation=matrixToPos(*positionX,*positionY);SO_N_MOVES=SO_N_MOVES-1;
            }
        }else{
            if(*positionY+1>=SO_BASE && flagCount>0 && !changeTarget){
                return 0;
            }
            if((controlValue=nonBlockingReserveSem(id,matrixToPos(*positionX,*positionY+1)))<0){
                    return 0;
            }else{
                if(releaseSem(id,matrixToPos(*positionX,*positionY))==-1){
                    printf("Errore nella release sem MOVEXTARGET: %s\n",strerror(errno));
                    exit(1);
                }
                *positionY=*positionY+1;
                *memoryLocation=matrixToPos(*positionX,*positionY);SO_N_MOVES=SO_N_MOVES-1;
            }
        }

    }
    tim.tv_sec = 0;
    tim.tv_nsec = SO_MIN_HOLD_NSEC;
    nanosleep(&tim,&tim2);
    return 1;
}

short MoveAntiXTarget(int id,int* memoryLocation,int *positionX,int *positionY,int targetX,int targetY){
    struct timespec tim, tim2;
    short controlValue;
    if(*positionX>targetX){
        if(*positionX+1>=SO_ALTEZZA && flagCount>0 && !changeTarget){
            return 0;
        }
        if((controlValue=nonBlockingReserveSem(id,matrixToPos(*positionX+1,*positionY)))<0){
                return 0;
        }else{
            if(releaseSem(id,matrixToPos(*positionX,*positionY))==-1){
                printf("Errore nella release sem MOVEXTARGET: %s\n",strerror(errno));
                exit(1);
            }
            *positionX=*positionX+1;
            *memoryLocation=matrixToPos(*positionX,*positionY);SO_N_MOVES=SO_N_MOVES-1;
        }
    }else if(*positionX<targetX){
        if(*positionY-1<0 && flagCount>0 && !changeTarget){
            return 0;
        }
        if((controlValue=nonBlockingReserveSem(id,matrixToPos(*positionX-1,*positionY)))<0){
                return 0;
        }else{
            if(releaseSem(id,matrixToPos(*positionX,*positionY))==-1){
                printf("Errore nella release sem MOVEXTARGET: %s\n",strerror(errno));
                exit(1);
            }
            *positionX=*positionX-1;
            *memoryLocation=matrixToPos(*positionX,*positionY);SO_N_MOVES=SO_N_MOVES-1;
        }
    }else{
        int randNum=rand() % 2 + 1;
        if(randNum==1){
            if(*positionX-1<0 && flagCount>0 && !changeTarget){
                return 0;
            }
            if((controlValue=nonBlockingReserveSem(id,matrixToPos(*positionX-1,*positionY)))<0){
                    return 0;
            }else{
                if(releaseSem(id,matrixToPos(*positionX,*positionY))==-1){
                    printf("Errore nella release sem MOVEXTARGET: %s\n",strerror(errno));
                    exit(1);
                }
                *positionX=*positionX-1;
                *memoryLocation=matrixToPos(*positionX,*positionY);SO_N_MOVES=SO_N_MOVES-1;
            }
        }else{
            if(*positionX+1>=SO_ALTEZZA && flagCount>0 && !changeTarget){
                return 0;
            }
            if((controlValue=nonBlockingReserveSem(id,matrixToPos(*positionX+1,*positionY)))<0){
                    return 0;
            }else{
                if(releaseSem(id,matrixToPos(*positionX,*positionY))==-1){
                    printf("Errore nella release sem MOVEXTARGET: %s\n",strerror(errno));
                    exit(1);
                }
                *positionX=*positionX+1;
                *memoryLocation=matrixToPos(*positionX,*positionY);SO_N_MOVES=SO_N_MOVES-1;
            }
        }
    }
    tim.tv_sec = 0;
    tim.tv_nsec = SO_MIN_HOLD_NSEC;
    nanosleep(&tim,&tim2);
    return 1;
}
