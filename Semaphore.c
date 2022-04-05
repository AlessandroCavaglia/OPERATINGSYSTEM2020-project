#include <sys/sem.h>
#include <stdlib.h>
#include <errno.h>

union senum{
    int val;
    struct semid_ds* buf;
    unsigned short* array;
#if defined(__linux__)
    struct seminfo* __buf;
#endif
};

int reserveSem(int semId,int semNum){
    struct sembuf sops;
    sops.sem_num=semNum;
    sops.sem_op=-1;
    sops.sem_flg=0;
    return semop(semId,&sops,1);
}

int reserveSems(int semId,int semNum,int num){
     struct sembuf sops;
     sops.sem_num=semNum;
     sops.sem_op=-num;
     sops.sem_flg=0;
     return semop(semId,&sops,1);
}

int nonBlockingReserveSem(int semId,int semNum){
    int retvalue;
    struct sembuf sops;
    sops.sem_num=semNum;
    sops.sem_op=-1;
    sops.sem_flg=IPC_NOWAIT;
    retvalue=semop(semId,&sops,1);
    if(retvalue==-1){
        if(errno==EAGAIN)
            retvalue=-2;
    }
    errno=0;
    return retvalue;
}

int releaseSem(int semID,int semNum){
    struct sembuf sops;
    sops.sem_num=semNum;
    sops.sem_op=1;
    sops.sem_flg=0;
    return semop(semID,&sops,1);
}

int releaseSems(int semID,int semNum,int num){
    struct sembuf sops;
    sops.sem_num=semNum;
    sops.sem_op=num;
    sops.sem_flg=0;
    return semop(semID,&sops,1);
}

int initAllSemAtValue(int semId,int quantity,int value){
    union senum arg;
    int i=0;
    arg.val=quantity;
    arg.array=malloc(sizeof(short)*quantity);
    for(i=0;i<quantity;i++)
        arg.array[i]=value;
    return semctl(semId,1,SETALL,arg);
}

int initSemAtVal(int semId,int semNum,int value){
    union senum arg;
    arg.val=value;
    return semctl(semId,semNum,SETVAL,arg);
}

int getSemVal(int semId,int semNum){
    union senum arg;
    arg.val=0;
    return semctl(semId,semNum,GETVAL,arg);
}

int removeSemSet(int semId){
    union senum arg;
    return semctl(semId,0,IPC_RMID,arg);
}

