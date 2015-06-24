/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"
#include "contracts.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. The REQUIRES and ENSURES from 15-122 are included
 *     for your convenience. They can be removed if you like.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
	int i, j, k, l, t0,t1,t2,t3,t4,t5,t6,t7;
    REQUIRES(M > 0);
    REQUIRES(N > 0);
	if (M == 32){
		for (i=0; i<M; i+=8){
			for (j=0;j<N;j++){
				
				t0 =A[j][i+0];
				t1= A[j][i+1];
				t2 =A[j][i+2];
				t3= A[j][i+3];
				t4 =A[j][i+4];
				t5= A[j][i+5];
				t6 =A[j][i+6];
				t7= A[j][i+7];
				
				B[i+0][j]=t0;
				B[i+1][j]=t1;
				B[i+2][j]=t2;
				B[i+3][j]=t3;
				B[i+4][j]=t4;
				B[i+5][j]=t5;
				B[i+6][j]=t6;
				B[i+7][j]=t7;
				
				
			}
		}
	}
	else if (M==64){
		for (i=0;i<M;i+=8){
			for (j=0;j<N;j+=8){
				for (k=j+0;k<j+8;k+=2){
					t0 =A[k][i+0];
					t1= A[k][i+1];
					t2 =A[k][i+2];
					t3= A[k][i+3];
					t4 =A[k][i+4];
					t5= A[k][i+5];
					t6 =A[k][i+6];
					t7= A[k][i+7];
					
					B[i+0][k]=t0;
					B[i+0][k+1]=t7;
					B[i+1][k]=t1;
					B[i+1][k+1]=t6;
					B[i+2][k]=t2;
					B[i+2][k+1]=t5;
					B[i+3][k]=t3;
					B[i+3][k+1]=t4;
				}
				for (k=j+1;k<j+8;k+=2){
					t0 =A[k][i+0];
					t1= A[k][i+1];
					t2 =A[k][i+2];
					t3= A[k][i+3];
					t4 =A[k][i+4];
					t5= A[k][i+5];
					t6 =A[k][i+6];
					t7= A[k][i+7];
					
					B[i+7][k-1]=t0;
					B[i+6][k-1]=t1;
					B[i+5][k-1]=t2;
					B[i+4][k-1]=t3;
					B[i+4][k]=t4;
					B[i+5][k]=t5;
					B[i+6][k]=t6;
					B[i+7][k]=t7;
				}
				for (k=i+0;k<i+4;k++){
					t0 =B[k][j+1];
					t1= B[k][j+3];
					t2 =B[k][j+5];
					t3= B[k][j+7];
					t4 = i+7-(k-i);
					B[k][j+1]=B[t4][j+0];
					B[k][j+3]=B[t4][j+2];
					B[k][j+5]=B[t4][j+4];
					B[k][j+7]=B[t4][j+6];
					
					B[t4][j+0]=t0;
					B[t4][j+2]=t1;
					B[t4][j+4]=t2;
					B[t4][j+6]=t3;			
				}
			}
		}
	}
	else{
		t0 = 19;
		t1 = 17;
		for (i=0;i<M;i+=t0){
			for (j=0;j<N;j+=t1){
				for (k=i;k<i+t0 && k<M;k++){
					for (l=j;l<j+t1 && l<N;l++){
						B[k][l]=A[l][k];
					}
				}
			}
		}
	}	
	

    ENSURES(is_transpose(M, N, A, B));
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    REQUIRES(M > 0);
    REQUIRES(N > 0);

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

    ENSURES(is_transpose(M, N, A, B));
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

