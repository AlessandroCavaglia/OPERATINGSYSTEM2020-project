extern int reserveSem(int semId,int semNum);
extern int reserveSems(int semId,int semNum,int num);
extern int releaseSem(int semID,int semNum);
extern int releaseSems(int semID,int semNum,int num);
extern int initSemInUse(int semId,int semNum);
extern int removeSemSet(int semId);
extern int initAllSemAtValue(int semId,int quantity,short value);
extern int initSemAtVal(int semId,int semNum,int value);
extern int getSemVal(int semId,int semNum);
extern int nonBlockingReserveSem(int semId,int semNum);