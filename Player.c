#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include <sys/sem.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <poll.h>
#include <signal.h>
#include "Matrix.h"
#include "Semaphore.h"
#include "Pawn.h"

extern int SO_BASE;
extern int SO_ALTEZZA;
extern int SO_NUM_G;
extern int SO_NUM_P;
extern long SO_MIN_HOLD_NSEC;
extern int STRATEGY;
extern int endgame;

int *communication;
int *flagArrR;
int communicationSemG;
int masterSemG;

int flagCount;


struct pawn{
    int pawnPipe;
    int target;
    int strat;
};

struct sigaction{
  void(*sa_handler)(int signum);
  sigset_t sa_mask;
  int sa_flags;
};


void captureHandlerr(int sig){
    flagArrR[*communication]=-1;
    flagCount--;
    releaseSem(communicationSemG,0);
}

void endCommandHandlerr(int sig){
        flagCount=0;
        endgame=1;
}

int getBestTarget(int flags[],int size);
int nearestTarget(int size,int position);

void start(int MasterPipe[],int playersSemId,int matrixSemId,int masterSem,int communicationSem,int roundSem,int endRoundSem,int clearSem,int startRoundSem,int *communicationVar,int *pawnArr,int *flagArr,int *scoreArr,int playerNumber){
    int counter=0;
    int position;
    int parentPipe[2];
    int numflags=-1;
    int tmp[2];
    struct pawn *pawns;
    char msg[12];
    short error;
    struct pollfd x;
    int bestTarget;
    struct sigaction sa,sa2;
    sigset_t my_mask,my_mask2;
    pawns=malloc(sizeof(struct pawn)*SO_NUM_P);
    communicationSemG=communicationSem;
    communication=communicationVar;
    masterSemG=masterSem;
    sa.sa_handler=&endCommandHandlerr;
    sa.sa_flags=0;
    sigemptyset(&my_mask);
    sa.sa_mask=my_mask;
    sa2.sa_handler=&captureHandlerr;
    sa2.sa_flags=0;
    sigemptyset(&my_mask2);
    sa2.sa_mask=my_mask2;
    if(sigaction(SIGUSR1,&sa,NULL)==-1){
        printf("Errore nell'inserimento della sigaction \n");
        exit(0);
    }
    if(sigaction(SIGUSR2,&sa2,NULL)==-1){
        printf("Errore nell'inserimento della sigaction \n");
        exit(0);
    }
    /*
     *  Setup Phase
     */
    if(pipe(parentPipe)==-1){
        printf("Errore nella creazione del Pipe %s\n",strerror(errno));
        exit(0);
    }
    if(close(MasterPipe[0])==-1){
        printf("Errore nella chiusura del Pipe %s\n",strerror(errno));
        exit(0);
    }
    srand(time(NULL));
    do{
        int pawnPipe[2];
        int pid;
        int maxPos;
        int minPos;
        if(pipe(pawnPipe)==-1){
            printf("Errore nella creazione del Pipe %s \n",strerror(errno));
            exit(0);
        }
        if(reserveSem(playersSemId,playerNumber)==-1){
            printf("Errore nella reserve %s\n",strerror(errno));
            exit(0);
        }
        minPos=counter*((SO_BASE*SO_ALTEZZA)/SO_NUM_P);
        if(counter==SO_NUM_P){
            maxPos=SO_BASE*SO_ALTEZZA-1;
        }else{
            maxPos=((counter+1)*((SO_BASE*SO_ALTEZZA)/SO_NUM_P))-1;
        }
        do{
            position=(rand() %(maxPos - minPos + 1)) + minPos;
        }while(positionInArray(position,pawnArr,SO_NUM_G*SO_NUM_P));
        pawnArr[counter+(playerNumber*SO_NUM_P)]=position;

        pid=fork();
        switch (pid){
            case -1:
                printf("Errore nel fork %s\n",strerror(errno));
                exit(0);
                break;
            case 0:
                if(close(parentPipe[0])==-1){
                    printf("Errore nella chiusura del Pipe %s\n",strerror(errno));
                    exit(0);
                }
                StartPawn(matrixSemId,roundSem,endRoundSem,clearSem,startRoundSem,pawnArr+(counter+(playerNumber*SO_NUM_P)),counter,playerNumber,pawnPipe,parentPipe,MasterPipe);
                if(close(parentPipe[1])==-1){
                    printf("Errore nella chiusura del Pipe %s\n",strerror(errno));
                }
                if(close(pawnPipe[0])==-1){
                    printf("Errore nella chiusura del Pipe %s\n",strerror(errno));
                }
                if(close(pawnPipe[1])==-1){
                    printf("Errore nella chiusura del Pipe %s\n",strerror(errno));
                }
                if(close(MasterPipe[1])==-1){
                    printf("Errore nella chiusura del Pipe %s\n",strerror(errno));
                }
                exit(0);
                break;
            default:
                    if(close(pawnPipe[0])==-1){
                        printf("Errore nella chiusura del Pipe %s\n",strerror(errno));
                        exit(0);
                    }
                    pawns[counter].pawnPipe=pawnPipe[1];
                    pawns[counter].target=-1;
                    pawns[counter].strat=-1;
                    reserveSem(matrixSemId,position);
                break;
        }
        if(playerNumber+1<SO_NUM_G){
            if(releaseSem(playersSemId,playerNumber+1)){
                printf("Errore nella release sem %s \n",strerror(errno));
                exit(0);
            }
        }else{
            if(releaseSem(playersSemId,0)==-1){
                printf("Errore nella release sem %s \n",strerror(errno));
                exit(0);
            }
        }
        counter++;
    }while(counter<SO_NUM_P);
    releaseSem(masterSem,0);
    /*
     * End Setup phase
     */
    do{
        int i;
        if(reserveSem(roundSem,0)==-1){
            printf("Errore nella reserve sem 1 %s \n",strerror(errno));
            exit(0);
        }
        /*
         *  Start of the round
         */
        numflags=flagArr[0];
        free(flagArrR);
        flagArrR=malloc(sizeof(int)*(numflags+1));
        for(position=1;position<numflags+1;position++){
            flagArrR[position]=flagArr[position];
        }
        flagCount=numflags;
        if(STRATEGY==1)
            bestTarget=getBestTarget(scoreArr,numflags);
        for(i=0;i<SO_NUM_P;i++){
            pawns[i].target=-1;
        }
        for(i=1; i<= numflags;i++){
            int y;
            int minDistance=-1;
            int index=-1;
            for(y=0;y<SO_NUM_P;y++){
                if(distance(pawnArr[y+(playerNumber*SO_NUM_P)],flagArr[i])<minDistance || minDistance==-1){
                    minDistance=distance(pawnArr[y+(playerNumber*SO_NUM_P)],flagArr[i]);
                    index=y;
                }
            }
            pawns[index].target=i;
        }
        for(position=0;position<SO_NUM_P;position++){
            if(pawns[position].target!=-1){
                counter=pawns[position].target;
            }else {
                if(STRATEGY==1){
                    int randVal=rand()%99;
                    if(randVal>70){
                        counter=bestTarget;
                    } else{
                        counter=SO_NUM_P*SO_NUM_G+1;
                    }
                }
            }
            posToMatrix(flagArr[counter],tmp);
            sprintf(msg,"%d",flagCount);
            if(write(pawns[position].pawnPipe,msg, sizeof(msg))==-1){
                printf("Errore nella scrittura su pipe %s \n",strerror(errno));
                exit(0);
            }
            sprintf(msg,"%d",counter);
            if(write(pawns[position].pawnPipe,msg, sizeof(msg))==-1){
                printf("Errore nella scrittura su pipe %s \n",strerror(errno));
                exit(0);
            }
            sprintf(msg,"%d",tmp[0]);
            if(write(pawns[position].pawnPipe,msg, sizeof(msg))==-1){
                printf("Errore nella scrittura su pipe %s \n",strerror(errno));
                exit(0);
            }
            sprintf(msg,"%d",tmp[1]);
            if(write(pawns[position].pawnPipe,msg, sizeof(msg))==-1){
                printf("Errore nella scrittura su pipe %s \n",strerror(errno));
                exit(0);
            }
            if(read(parentPipe[0],msg, sizeof(msg))==-1){
                printf("errore nella lettura del parentpipe %s\n",strerror(errno));
            }
            pawns[position].target=counter;
            releaseSem(masterSem,0);
        }
        do{
            int servingPlayer;
            char *ptr;
            if(flagCount>0){
                short check;
                do{
                    check=0;
                    if(read(parentPipe[0],msg, sizeof(msg))==-1){
                        check=1;
                    }
                }while(check && flagCount>0);
            }
            if(flagCount>0){
                servingPlayer=strtol(msg,&ptr,10);
                counter=nearestTarget(numflags,pawnArr[servingPlayer+(playerNumber*SO_NUM_P)]);
                pawns[servingPlayer].strat=0;

                if(flagCount>0){
                    sprintf(msg,"%d",counter);
                    write(pawns[servingPlayer].pawnPipe,msg, sizeof(msg));
                    posToMatrix(flagArrR[counter],tmp);
                    sprintf(msg,"%d",tmp[0]);
                    write(pawns[servingPlayer].pawnPipe,msg, sizeof(msg));
                    sprintf(msg,"%d",tmp[1]);
                    write(pawns[servingPlayer].pawnPipe,msg, sizeof(msg));
                }
            }
        }while(flagCount>0);
        if(releaseSem(endRoundSem,0)==-1){
            printf("Errore nella release sem %s\n",strerror(errno));
            exit(0);
        }
        /*
        *  End of the round
        */
        do{
            error=0;
            if(reserveSem(clearSem,0)==-1){
                error=1;
            }
        }while(error);
        /*
         *  Start cleaning phase
         */
        x.fd=parentPipe[0];
        x.events=POLLIN;
        while(poll(&x, 1, 0)==1) {
            read(parentPipe[0],msg, sizeof(msg));
        }
        if(releaseSem(endRoundSem,0)==-1){
            printf("Errore nella release sem %s\n",strerror(errno));
            exit(0);
        }
        /*
         * End of the cleaning phase
         */
    }while(!endgame);
    /*
     *  End of the game
     */
    if(close(parentPipe[0])==-1){
        printf("Errore nella chiusura del Pipe %s\n",strerror(errno));
    }
    if(close(MasterPipe[1])==-1){
        printf("Errore nella chiusura del Pipe %s\n",strerror(errno));
    }
    free(pawns);
}

int getBestTarget(int flags[],int size){
    int id=1;
    int i;
    for(i=1;i<=size;i++)
        if(flags[i]>flags[id]){
            id=i;
        }
    return id;
}

int nearestTarget(int size,int position){
    int dist=-1;
    int id=-1;
    int i;
    for(i=1;i<=size;i++){
        if(dist==-1 && flagArrR[i]!=-1){
            dist=distance(flagArrR[i],position);
            id=i;
        }else{
            if(flagArrR[i]!=-1){
                if(distance(flagArrR[i],position)<dist){
                    dist=distance(flagArrR[i],position);
                    id=i;
                }
            }
        }
    }
    return id;
}



