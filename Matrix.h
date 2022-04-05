extern void posToMatrix(int pos,int val[]);     /*Function that returns the x and y value of a position in the matrix*/
extern int matrixToPos(int x,int y);    /*Function that returns the position in the matrix given the x and y coordinates */
extern int positionInArray(int pos,int arr[],int lenght);   /*Function that returns if a element is inside a given array of given length */
extern int allInvalid(int arr[],int lenght);    /* Function that returns if all the elements of the array have value -1 */
extern int distance(int pos1,int pos2);     /* Function that returns the distance from two positions of the matrix */
extern int getLowest(int arr[],int size,int min); /*Function that returns the smallest element of an array given a relative minimum */
