#include <stdio.h>
#include <stdlib.h>
extern int SO_BASE;
extern int SO_ALTEZZA;

void posToMatrix(int pos,int val[]){
    val[0]=pos/SO_BASE;
    val[1]=pos % SO_BASE;
}

int matrixToPos(int x,int y){
    return x*SO_BASE+y;
}

int positionInArray(int pos,int arr[],int lenght){
    int i;
    for(i=0;i<lenght;i++){
        if(arr[i]==pos)
            return 1;
    }
    return 0;
}

int allinvalid(int arr[],int lenght){
    int i;
    for(i=0;i<lenght;i++){
        if(arr[i]!=-1){
            return 0;
        }
    }
    return 1;
}

int distance(int pos1,int pos2){
    int poss1[2], poss2[2];
    posToMatrix(pos1,poss1);
    posToMatrix(pos2,poss2);
    return abs(poss1[0]-poss2[0])+abs(poss1[1]-poss2[1]);
}

int getLowest(int arr[],int size,int min){
    int smallest=SO_ALTEZZA*SO_BASE;
    int id=-1;
    int i;
    for(i=0;i<size;i++){
        if(arr[i]>min && arr[i]<smallest){
            smallest=arr[i];
            id=i;
        }
    }
    return id;
}
