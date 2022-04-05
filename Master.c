#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/wait.h>
#include <poll.h>
#include <signal.h>
#include "Player.h"
#include "Semaphore.h"
#include "Matrix.h"



int SO_BASE=0;
int SO_ALTEZZA=0;
int SO_NUM_G=0;
int SO_NUM_P=0;
int SO_FLAG_MIN=0;
int SO_FLAG_MAX=0;
long SO_MIN_HOLD_NSEC=0;
int SO_ROUND_SCORE=0;
int SO_MAX_TIME=0;
int SO_N_MOVES=0;
int STRATEGY=0;
int endgame=0;
int numFlags=0;

	struct sigaction{
	  void(*sa_handler)(int signum);
	  sigset_t sa_mask;
	  int sa_flags;
	};


void Round();
void endCommandHandlers(int sig){
    if(sig==SIGALRM){
        if(numFlags>0){
            kill(0,SIGUSR1);
            endgame=1;
        }
    }
    return;
}

void generateScores(int scores[],int size);
void drawMap(int pawns[],int matrixSem,int flags[],int numFlags);
void setstates(int val);

int main(){
    int masterSem;
    int playersSem;
    int matrixSem;
    int roundSem;
    int endRoundSem;
    int comunicationSem;
    int cleanPhaseSem;
    int startRoundSem;
    int masterPipe[2];
    int i,ifirstval,isecondval,*moves,roundFlags;
    int pawnArrayId;
    int flagArrayId;
    int scoreArrayId;
    int comunicationId;
    int *pawnArray;
    int *flagArray;
    int *scoreArray;
    int *comunicationVar;
    char msg[12],catchMsg[25],*firstval,*secondval,*ptr;
    int *scores;
    long nanoseconds=0;
    struct sigaction sa;
    struct  timeval startime,endtime;
    int status;
    int round=0;
    int max, maxid,sum;
    struct pollfd x;
    sigset_t my_mask;
    printf("Scegli la modalità di gioco: (0/Easy) (1/Hard): ");
    scanf("%d",&status);
    if(status!=0 && status!=1){
        printf("Errore \n");
        exit(1);
    }

    setstates(status);

    masterSem=semget(IPC_PRIVATE,1,IPC_CREAT | 0666);
    playersSem=semget(IPC_PRIVATE,SO_NUM_G,IPC_CREAT | 0666);
    matrixSem=semget(IPC_PRIVATE,SO_BASE*SO_ALTEZZA,IPC_CREAT | 0666);
    roundSem=semget(IPC_PRIVATE,1,IPC_CREAT | 0666);
    endRoundSem=semget(IPC_PRIVATE,1,IPC_CREAT | 0666);
    comunicationSem=semget(IPC_PRIVATE,1,IPC_CREAT | 0666);
    cleanPhaseSem=semget(IPC_PRIVATE,1,IPC_CREAT | 0666);
    startRoundSem=semget(IPC_PRIVATE,1,IPC_CREAT | 0666);

    pawnArrayId=shmget(IPC_PRIVATE, sizeof(int)*SO_NUM_P*SO_NUM_G,IPC_CREAT | 0666);
    flagArrayId=shmget(IPC_PRIVATE, sizeof(int)*SO_FLAG_MAX+1,IPC_CREAT | 0666);
    scoreArrayId=shmget(IPC_PRIVATE, sizeof(int)*SO_FLAG_MAX+1,IPC_CREAT | 0666);
    comunicationId=shmget(IPC_PRIVATE, sizeof(int),IPC_CREAT | 0666);

    moves=malloc(sizeof(int)*(SO_NUM_P*SO_NUM_G));
    scores=malloc(sizeof(int)*SO_NUM_G);
    sa.sa_handler=&endCommandHandlers;
    sa.sa_flags=0;
    sigemptyset(&my_mask);
    sa.sa_mask=my_mask;

    if(sigaction(SIGUSR2,&sa,NULL)==-1){
        printf("Errore nell'inserimento della sigaction");
        exit(0);
    }
    if(sigaction(SIGALRM,&sa,NULL)==-1){
        printf("Errore nell'inserimento della sigaction");
        exit(0);
    }
    if(sigaction(SIGUSR1,&sa,NULL)==-1){
        printf("Errore nell'inserimento della sigaction");
        exit(0);
    }
    /*
     *  Start of the setup phase
     */
    if(masterSem==-1){
        printf("Errore nella creazione del semaforo %s \n",strerror(errno));
        exit(0);
    }
    if(playersSem==-1){
        printf("Errore nella creazione del semaforo %s \n",strerror(errno));
        exit(0);
    }
    if(matrixSem==-1){
        printf("Errore nella creazione del semaforo %s \n",strerror(errno));
        exit(0);
    }
    if(roundSem==-1){
        printf("Errore nella creazione del semaforo %s \n",strerror(errno));
        exit(0);
    }
    if(endRoundSem==-1){
        printf("Errore nella creazione del semaforo %s \n",strerror(errno));
        exit(0);
    }
    if(comunicationSem==-1){
        printf("Errore nella creazione del semaforo %s \n",strerror(errno));
        exit(0);
    }
    if(cleanPhaseSem==-1){
        printf("Errore nella creazione del semaforo %s \n",strerror(errno));
        exit(0);
    }
    if(startRoundSem==-1){
        printf("Errore nella creazione del semaforo %s \n",strerror(errno));
        exit(0);
    }
    if(pawnArrayId==-1){
        printf("Errore nella creazione della memoria condivisa %s \n",strerror(errno));
        exit(0);
    }
    if(initSemAtVal(masterSem,0,0)==-1){
        printf("Errore nell'inizializzazione del semaforo %s\n",strerror(errno));
        exit(0);
    }
    if(initSemAtVal(comunicationSem,0,0)==-1){
        printf("Errore nell'inizializzazione del semaforo %s\n",strerror(errno));
        exit(0);
    }
    if(initSemAtVal(roundSem,0,0)==-1){
        printf("Errore nell'inizializzazione del semaforo %s\n",strerror(errno));
        exit(0);
    }
    if(initSemAtVal(endRoundSem,0,0)==-1){
        printf("Errore nell'inizializzazione del semaforo %s\n",strerror(errno));
        exit(0);
    }
    if(initSemAtVal(cleanPhaseSem,0,0)==-1){
        printf("Errore nell'inizializzazione del semaforo %s\n",strerror(errno));
        exit(0);
    }
    if(initSemAtVal(startRoundSem,0,0)==-1){
        printf("Errore nell'inizializzazione del semaforo %s\n",strerror(errno));
        exit(0);
    }
    if(initAllSemAtValue(playersSem,SO_NUM_G,0)==-1){
        printf("Errore nell'inizializzazione dei semafori %s\n",strerror(errno));
        exit(0);
    }
    if(initAllSemAtValue(matrixSem,SO_ALTEZZA*SO_BASE,1)==-1){
        printf("Errore nell'inizializzazione dei semafori %s\n",strerror(errno));
        exit(0);
    }
    if(pipe(masterPipe)==-1){
        printf("Errore nella creazione del pipe %s\n",strerror(errno));
        exit(0);
    }
    pawnArray=(int*)shmat(pawnArrayId,NULL,0);
    if((void *)pawnArray==(void*)-1){
        printf("Errore nell'ottenimento della memoria condivisa %s \n",strerror(errno));
        exit(0);
    }
    flagArray=(int*)shmat(flagArrayId,NULL,0);
    if((void *)flagArray==(void*)-1){
        printf("Errore nell'ottenimento della memoria condivisa %s \n",strerror(errno));
        exit(0);
    }
    scoreArray=(int*)shmat(scoreArrayId,NULL,0);
    if((void *)scoreArray==(void*)-1){
        printf("Errore nell'ottenimento della memoria condivisa %s \n",strerror(errno));
        exit(0);
    }
    comunicationVar=(int*)shmat(comunicationId,NULL,0);
    if((void *)comunicationVar==(void*)-1){
        printf("Errore nell'ottenimento della memoria condivisa %s \n",strerror(errno));
        exit(0);
    }
    *comunicationVar=-1;
    for(i=0;i<SO_NUM_P*SO_NUM_G;i++){
        pawnArray[i]=-1;
        moves[i]=SO_N_MOVES;
    }
    printf("\x1b[31m Inizio fase di set-up iniziale \x1b[0m \n");
    for(i=0;i<SO_NUM_G;i++){
        switch (fork()){
            case -1:
                printf("Errore nella fork %s\n",strerror(errno));
                exit(0);
                break;
            case 0:
                free(moves);
                free(scores);
                start(masterPipe,playersSem,matrixSem,masterSem,comunicationSem,roundSem,endRoundSem,cleanPhaseSem,startRoundSem,comunicationVar,pawnArray,flagArray,scoreArray,i);
                exit(0);
                break;
            default:
                scores[i]=0;
                break;
        }
    }
    srand(time(NULL));
    printf("\n");
    i=rand()%SO_NUM_G;
    releaseSem(playersSem,i);
    printf("\n");
    /*
    * End of the setup phase
    */
    reserveSems(masterSem,0,SO_NUM_G);
    printf("\x1b[31m Fine fase di set-up iniziale \x1b[0m \n");
    /*
    * Start of the round
    */
    do{
        for(i=0;i<SO_FLAG_MAX;i++){
            flagArray[i]=-1;
        }
        srand(time(NULL));
        numFlags=(rand() %(SO_FLAG_MAX - SO_FLAG_MIN + 1)) + SO_FLAG_MIN;
        roundFlags=numFlags;
        flagArray[0]=numFlags;
        for(i=1;i<=numFlags;i++){
            int maxPos;
            int minPos;
            int position;
            minPos=(i-1)*((SO_BASE*SO_ALTEZZA)/numFlags);
            if(i==numFlags){
                maxPos=SO_BASE*SO_ALTEZZA-1;
            }else{
                maxPos=((i)*((SO_BASE*SO_ALTEZZA)/numFlags))-1;
            }
            do{
                position=(rand() %(maxPos - minPos + 1)) + minPos;
            }while(positionInArray(position,pawnArray,SO_NUM_P*SO_NUM_G) || positionInArray(position,flagArray+1,numFlags));
            flagArray[i]=position;
        }
        generateScores(scoreArray,numFlags);
        max=0;
        for(i=0;i<SO_BASE/2-6;i++){
            printf("  ");
        }
        printf("\x1b[31m Inizio Round \x1b[0m \n\n");
        for(i=0;i<SO_NUM_P*SO_NUM_G;i++){
            if(i%SO_NUM_P==0){
                char colors[16][8]={"\x1b[31m","\x1b[35m","\x1b[33m","\x1b[34m","\x1b[32m","\x1b[36m","\x1b[37m","\x1b[90m","\x1b[91m","\x1b[92m","\x1b[93m","\x1b[94m","\x1b[95m","\x1b[96m","\x1b[97m","\x1b[0m"};
                if(i!=0)
                    printf("--Mosse rimanenti: %d \n",max);
                printf("%s",colors[i/SO_NUM_P%14 +1]);
                printf("\n---Player: %c\n",'A'+i/SO_NUM_P);
                printf("%s",colors[15]);
                printf("--Punteggio: %d \n",scores[i/SO_NUM_P]);
                max=0;
            }
            max=max+moves[i];
        }
        printf("--Mosse rimanenti: %d \n",max);
        drawMap(pawnArray,matrixSem,flagArray,roundFlags);
        printf("\n\n");
        if(releaseSems(roundSem,0,(SO_NUM_P*SO_NUM_G)+SO_NUM_G)==-1){
            printf("Errore nella release Sem %s\n",strerror(errno));
            exit(0);
        }if(reserveSems(masterSem,0,SO_NUM_P*SO_NUM_G)==-1){
            printf("Errore nella reserve Sem %s\n",strerror(errno));
        }

        round++;
        gettimeofday(&startime,NULL);
        releaseSems(startRoundSem,0,SO_NUM_P*SO_NUM_G);
        alarm(SO_MAX_TIME);
        do{
            short err;
            do{
                err=0;
                if(read(masterPipe[0],catchMsg, sizeof(catchMsg))==-1){
                    err=1;
                }
            }while(err && numFlags!=0 && !endgame);
            if(numFlags!=0 && !endgame){
                firstval=strtok(catchMsg,"-");
                ifirstval=strtol(firstval, &ptr, 10);
                firstval=strtok(NULL,"-");
                isecondval=strtol(firstval, &ptr, 10);
                if(pawnArray[ifirstval]==flagArray[isecondval]){
                    numFlags--;
                    if(numFlags<1){
                        alarm(0);
                    }
                    *comunicationVar=isecondval;
                    /*printf("BANDIERINA CATTURATA dal player %d da %d punti!\n\n",ifirstval/SO_NUM_P,scoreArray[isecondval]);*/
                    scores[ifirstval/SO_NUM_P]=scores[ifirstval/SO_NUM_P]+scoreArray[isecondval];
                    kill(0,SIGUSR2);
                    flagArray[isecondval]=-1;
                    reserveSems(comunicationSem,0,SO_NUM_P*SO_NUM_G+SO_NUM_G);
                }
            }
        }while(numFlags!=0 && !endgame);
        gettimeofday(&endtime,NULL);
        /*
         *  End of the round
         */
        reserveSems(endRoundSem,0,(SO_NUM_G*SO_NUM_P)+SO_NUM_G);
        x.fd=masterPipe[0];
        x.events=POLLIN;
        while(poll(&x, 1, 0)==1) {
            read(masterPipe[0],catchMsg, sizeof(catchMsg));
        }
        /*
         * Start of the cleaning phase
         */
        releaseSems(cleanPhaseSem,0,(SO_NUM_G*SO_NUM_P)+SO_NUM_G);
        for(i=0;i<SO_NUM_P*SO_NUM_G;i++){
            if(read(masterPipe[0],catchMsg, sizeof(catchMsg))==-1){
                printf("Errore nella read: %s\n",strerror(errno));
                exit(1);
            }
            firstval=strtok(catchMsg,"-");
            ifirstval=strtol(firstval, &ptr, 10);
            firstval=strtok(NULL,"-");
            isecondval=strtol(firstval, &ptr, 10);
            moves[ifirstval]=isecondval;
        }
        /*
         * End of the cleaning phase
         */
        reserveSems(endRoundSem,0,(SO_NUM_G*SO_NUM_P)+SO_NUM_G);
        /*
         *  Data print
         */
        max=0;
        for(i=0;i<SO_NUM_P*SO_NUM_G;i++){
            if(i%SO_NUM_P==0){
                char colors[16][8]={"\x1b[31m","\x1b[35m","\x1b[33m","\x1b[34m","\x1b[32m","\x1b[36m","\x1b[37m","\x1b[90m","\x1b[91m","\x1b[92m","\x1b[93m","\x1b[94m","\x1b[95m","\x1b[96m","\x1b[97m","\x1b[0m"};
                if(i!=0)
                    printf("--Mosse rimanenti: %d \n",max);
                printf("%s",colors[i/SO_NUM_P%14 +1]);
                printf("\n---Player: %c\n",'A'+i/SO_NUM_P);
                printf("%s",colors[15]);
                printf("--Punteggio: %d \n",scores[i/SO_NUM_P]);
                max=0;
            }
            max=max+moves[i];
        }
        printf("--Mosse rimanenti: %d \n",max);
        drawMap(pawnArray,matrixSem,flagArray,roundFlags);
        if(endgame){
            nanoseconds=nanoseconds+SO_MAX_TIME*1000000;
            printf("\nRound durato %d, durata totale %ld \n",SO_MAX_TIME,nanoseconds);
        }else{
            nanoseconds=nanoseconds+(endtime.tv_sec - startime.tv_sec) * 1000000 + endtime.tv_usec - startime.tv_usec;
            printf("\nRound durato %ld, durata totale %ld \n",((endtime.tv_sec - startime.tv_sec) * 1000000 + endtime.tv_usec - startime.tv_usec)/1000000,nanoseconds);
        }
        for(i=0;i<SO_BASE/2-5;i++){
            printf("  ");
        }
        printf("\x1b[31mFine Round \x1b[0m \n");
        printf("\n\n\n\n");
    }while(!endgame);
    /*
     *  End of the game
     */
    for(i=0;i<SO_BASE/2-7;i++){
        printf("  ");
    }
    printf("\x1b[31m Metriche finali \x1b[0m \n\n");
    for(i=0;i<SO_NUM_G*SO_NUM_P;i++){
        if(i%SO_NUM_P==0){
            char colors[16][8]={"\x1b[31m","\x1b[35m","\x1b[33m","\x1b[34m","\x1b[32m","\x1b[36m","\x1b[37m","\x1b[90m","\x1b[91m","\x1b[92m","\x1b[93m","\x1b[94m","\x1b[95m","\x1b[96m","\x1b[97m","\x1b[0m"};
            if(i!=0){
                printf("Rapporto Mosse usate/Mosse Totali %d/%d=%f\n",SO_N_MOVES*SO_NUM_P-ifirstval,SO_NUM_P*SO_N_MOVES,(float)(SO_N_MOVES*SO_NUM_P-ifirstval)/(float)(SO_NUM_P*SO_N_MOVES));
                if(scores[isecondval]==0)
                    printf("Rapporto Mosse usate/Punteggio %d/%d=%f\n",SO_N_MOVES*SO_NUM_P-ifirstval,scores[isecondval],0.0);
                else
                    printf("Rapporto Mosse usate/Punteggio %d/%d=%f\n",SO_N_MOVES*SO_NUM_P-ifirstval,scores[isecondval],(float)(SO_N_MOVES*SO_NUM_P-ifirstval)/(float)scores[isecondval]);
                printf("%s \n",colors[15]);
            }
            printf("%s",colors[i/SO_NUM_P%14 +1]);
            printf("\n---Player: %c\n",'A'+i/SO_NUM_P);
            ifirstval=moves[i];
            isecondval=i/SO_NUM_P;
        }else{
            ifirstval=ifirstval+moves[i];
        }
    }
    printf("Rapporto Mosse usate/Mosse Totali %d/%d=%f\n",SO_N_MOVES*SO_NUM_P-ifirstval,SO_NUM_P*SO_N_MOVES,(float)(SO_N_MOVES*SO_NUM_P-ifirstval)/(float)(SO_NUM_P*SO_N_MOVES));
    if(scores[isecondval]==0)
        printf("Rapporto Mosse usate/Punteggio %d/%d=%f\n",SO_N_MOVES*SO_NUM_P-ifirstval,scores[isecondval],0.0);
    else
        printf("Rapporto Mosse usate/Punteggio %d/%d=%f\n",SO_N_MOVES*SO_NUM_P-ifirstval,scores[isecondval],(float)(SO_N_MOVES*SO_NUM_P-ifirstval)/(float)scores[isecondval]);
    printf("%s \n","\x1b[0m");
    ifirstval=0;
    max=-1;
    maxid=-1;
    sum=0;
    for(i=0;i<SO_NUM_G;i++){
        if(scores[i]>max || max==-1){
            max=scores[i];
            maxid=i;
        }
        sum=sum+scores[i];
    }
    printf("Rapporto Punti Totali/Tempo di gioco: %d/%f=%f \n \n",sum,(float)nanoseconds/1000000,(float)sum/(float)(nanoseconds/1000000));
    printf("Player vincitore: %c !\n",'A'+maxid);
    printf("Round Giocati: %d \n",round);
    for(i=0;i<SO_BASE/2-5;i++){
        printf("  ");
    }
    printf("\x1b[31m Fine gioco \x1b[0m\n\n");
    for(i=0;i<SO_NUM_P*SO_NUM_G+SO_NUM_G;i++){
        wait(&status);
    }
    removeSemSet(masterSem);
    removeSemSet(playersSem);
    removeSemSet(matrixSem);
    removeSemSet(roundSem);
    removeSemSet(endRoundSem);
    removeSemSet(comunicationSem);
    removeSemSet(cleanPhaseSem);
    removeSemSet(startRoundSem);
    shmctl(pawnArrayId,0,IPC_RMID);
    shmctl(flagArrayId,0,IPC_RMID);
    shmctl(scoreArrayId,0,IPC_RMID);
    shmctl(comunicationId,0,IPC_RMID);
    close(masterPipe[0]);
    free(moves);
    free(scores);
}


void generateScores(int scores[],int size){
    int totalscore=0;
    int i;
    srand(time(NULL));
    for(i=1;i<=size;i++){
        scores[i]=rand()%(SO_ROUND_SCORE-(size-i)-totalscore)+1;
        totalscore=totalscore+scores[i];
    }
    while(totalscore<SO_ROUND_SCORE){
        int id=rand()%size+1;
        scores[id]++;
        totalscore++;
    }
}

void drawMap(int pawns[],int matrixSem,int flags[],int numFlags){
    int *orderedPawns;
    int *orderePawnsP;
    int *tmp;
    int *tmpf;
    int counter=SO_NUM_G*SO_NUM_P;
    int minrel=-1;
    int i,index=0;
    char colors[16][8]={"\x1b[31m","\x1b[35m","\x1b[33m","\x1b[34m","\x1b[32m","\x1b[36m","\x1b[37m","\x1b[90m","\x1b[91m","\x1b[92m","\x1b[93m","\x1b[94m","\x1b[95m","\x1b[96m","\x1b[97m","\x1b[0m"};
    tmp=malloc(sizeof(int)*(SO_NUM_P*SO_NUM_G+numFlags));
    tmpf=malloc(sizeof(int)*(numFlags));
    for(i=0;i<SO_BASE/2-2;i++){
        printf("  ");
    }
    printf("Mappa\n");
    for(i=0;i<SO_NUM_G*SO_NUM_P;i++){
        tmp[i]=pawns[i];
    }
    for(i=1;i<=numFlags;i++){
        if(flags[i]!=-1){
            tmp[counter]=flags[i];
            tmpf[counter-SO_NUM_G*SO_NUM_P]=flags[i];
            counter++;
        }
    }

    orderedPawns=malloc(sizeof(int)*counter);
    orderePawnsP=malloc(sizeof(int)*counter);


    for(i=0;i<counter;i++){
        orderedPawns[i]=getLowest(tmp,counter,minrel);
        tmp[orderedPawns[i]]=-1;
        if(orderedPawns[i]<SO_NUM_G*SO_NUM_P){
            orderePawnsP[i]=pawns[orderedPawns[i]];
            minrel=pawns[orderedPawns[i]];
            orderedPawns[i]=orderedPawns[i]/SO_NUM_P;
        }else{
            orderePawnsP[i]=tmpf[orderedPawns[i]-(SO_NUM_P*SO_NUM_G)];
            minrel=tmpf[orderedPawns[i]-(SO_NUM_P*SO_NUM_G)];
            orderedPawns[i]=-1;
        }
    }
    for(i=0;i<SO_BASE*SO_ALTEZZA;i++){
        if(i%SO_BASE==0 && i!=0){
            printf("|\n");
        }
        if(orderePawnsP[index]!=i){
            printf("|0");
        }else{
            if(orderedPawns[index]==-1){
                printf("|");
                printf("%s",colors[0]);
                printf("*");
                printf("%s",colors[15]);
                index++;
            } else{
                printf("|");
                printf("%s",colors[orderedPawns[index]%14+1]);
                printf("%c",'A'+orderedPawns[index]);
                printf("%s",colors[15]);
                index++;
            }
        }
    }
    printf("|\n");
    free(orderedPawns);
    free(orderePawnsP);
}

void assignValue(FILE *file,int *target,int type){
    char *act,*number,*ptr;
    int i,index;
    act=malloc(sizeof(char));
    *act='5';
    number=malloc(sizeof(char)*20);
    index=0;
    while(*act!=' ' && *act != EOF){
        *act=fgetc(file);
    }
    *act='0';
    while(*act!=' ' && *act != EOF){
        *act=fgetc(file);
        number[index]=*act;
        index++;
    }
    if(!type)
        *target=strtol(number, &ptr, 10);
    number=malloc(sizeof(char)*20);
    index=0;
    *act='0';
    while(*act!=' ' && *act!='\n' && *act != EOF){
        *act=fgetc(file);
        number[index]=*act;
        index++;
    }
    if(type)
        *target=strtol(number, &ptr, 10);
    free(act);
    free(number);
}

void assignValueL(FILE *file,long *target,int type){
    char *act,*number,*ptr;
    int i,index;
    act=malloc(sizeof(char));
    *act='5';
    number=malloc(sizeof(char)*20);
    index=0;
    while(*act!=' ' && *act != EOF){
        *act=fgetc(file);
    }
    *act='0';
    while(*act!=' ' && *act != EOF){
        *act=fgetc(file);
        number[index]=*act;
        index++;
    }
    if(!type)
        *target=strtol(number, &ptr, 10);
    number=malloc(sizeof(char)*20);
    index=0;
    *act='0';
    while(*act!=' ' && *act!='\n' && *act != EOF){
        *act=fgetc(file);
        number[index]=*act;
        index++;
    }
    if(type)
        *target=strtol(number, &ptr, 10);
    free(act);
    free(number);
}

void setstates(int type){
    FILE *config=fopen("config.txt","r");
    assignValue(config,&SO_NUM_G,type);
    assignValue(config,&SO_NUM_P,type);
    assignValue(config,&SO_MAX_TIME,type);
    assignValue(config,&SO_BASE,type);
    assignValue(config,&SO_ALTEZZA,type);
    assignValue(config,&SO_FLAG_MIN,type);
    assignValue(config,&SO_FLAG_MAX,type);
    assignValue(config,&SO_ROUND_SCORE,type);
    assignValue(config,&SO_N_MOVES,type);
    assignValueL(config,&SO_MIN_HOLD_NSEC,type);
    assignValue(config,&STRATEGY,type);
}
