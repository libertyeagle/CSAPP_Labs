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

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    // cache is a direct-mapped cache
    // there are 32 sets, and each set (block) is 32 bytes
    // 5 bits for block offset, 5 bits for set index

    int ii, jj, i, j;

    int temp_1;
    int temp_2;
    int temp_3;
    int temp_4;
    int temp_5;
    int temp_6;
    int temp_7;
    int temp_8;

    if (M == 32 && N == 32) {
        for (ii = 0; ii < 32; ii += 8)
            for (jj = 0; jj < 32; jj += 8) 
                for (i = ii; i < ii + 8; i++) {
                        // read an entire line of matrix A
                        temp_1 = A[i][jj];
                        temp_2 = A[i][jj + 1];
                        temp_3 = A[i][jj + 2];
                        temp_4 = A[i][jj + 3];
                        temp_5 = A[i][jj + 4];
                        temp_6 = A[i][jj + 5];
                        temp_7 = A[i][jj + 6];
                        temp_8 = A[i][jj + 7];

                        // then write to the entire column of B
                        B[jj][i] = temp_1;
                        B[jj + 1][i] = temp_2;
                        B[jj + 2][i] = temp_3;
                        B[jj + 3][i] = temp_4;
                        B[jj + 4][i] = temp_5;
                        B[jj + 5][i] = temp_6;
                        B[jj + 6][i] = temp_7;
                        B[jj + 7][i] = temp_8;
                    }
    }
    else if (M == 64 && N == 64) {
        for (ii = 0; ii < 64; ii += 8)
            for (jj = 0; jj < 64; jj += 8) {
                // handle the top 1/2 elements of A
                for (i = ii; i < ii + 4; i++) {
                    temp_1 = A[i][jj];
                    temp_2 = A[i][jj + 1];
                    temp_3 = A[i][jj + 2];
                    temp_4 = A[i][jj + 3];
                    temp_5 = A[i][jj + 4];
                    temp_6 = A[i][jj + 5];
                    temp_7 = A[i][jj + 6];
                    temp_8 = A[i][jj + 7];
                    
                    // transpose top-left sub-block
                    B[jj][i] = temp_1;
                    B[jj + 1][i] = temp_2;
                    B[jj + 2][i] = temp_3;
                    B[jj + 3][i] = temp_4;

                    // transpose top-right sub-block
                    B[jj][i + 4] = temp_5;
                    B[jj + 1][i + 4] = temp_6;
                    B[jj + 2][i + 4] = temp_7;
                    B[jj + 3][i + 4] = temp_8;
                }
                
                // handle bottom-left sub-block and top-right sub-block
                for (j = jj; j < jj + 4; j++) {
                    // read a column of bottom-left sub-block of A
                    temp_1 = A[ii + 4][j];
                    temp_2 = A[ii + 5][j];
                    temp_3 = A[ii + 6][j];
                    temp_4 = A[ii + 7][j];

                    // read a line of top-right sub-block of B
                    temp_5 = B[j][ii + 4];
                    temp_6 = B[j][ii + 5];
                    temp_7 = B[j][ii + 6];
                    temp_8 = B[j][ii + 7];

                    // store the result of a line of top-right sub-block of B
                    B[j][ii + 4] = temp_1;
                    B[j][ii + 5] = temp_2;
                    B[j][ii + 6] = temp_3;
                    B[j][ii + 7] = temp_4;

                    // store the result of a line of bottom-left sub-block of B
                    B[j + 4][ii] = temp_5;
                    B[j + 4][ii + 1] = temp_6;
                    B[j + 4][ii + 2] = temp_7;
                    B[j + 4][ii + 3] = temp_8;
                }

                // handle bottom-right sub-block
                for (i = ii + 4; i < ii + 8; i++) {
                    // read a line of A
                    temp_1 = A[i][jj + 4];
                    temp_2 = A[i][jj + 5];
                    temp_3 = A[i][jj + 6];
                    temp_4 = A[i][jj + 7];

                    // store a column of B
                    B[jj + 4][i] = temp_1;
                    B[jj + 5][i] = temp_2;
                    B[jj + 6][i] = temp_3;
                    B[jj + 7][i] = temp_4;
                }
            }

    }
    else {
        for (ii = 0; ii < 67; ii += 16)
            for (jj = 0; jj < 61; jj += 16) 
                for (i = ii; i < ii + 16 && i < 67; i++) 
                    for (j = jj; j < jj + 16 && j < 61; j++)
                        B[j][i] = A[i][j];
                 
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 
char trans_test_1_desc[] = "4 * 4 blocking transpose for 64 * 64";
void trans_test_1(int M, int N, int A[N][M], int B[M][N])
{
    int ii, jj, i;

    int temp_1;
    int temp_2;
    int temp_3;
    int temp_4;

    if (M == 64 && N == 64) {
        for (ii = 0; ii < 64; ii += 4)
            for (jj = 0; jj < 64; jj += 4) 
                for (i = ii; i < ii + 4; i++) {
                    temp_1 = A[i][jj];
                    temp_2 = A[i][jj + 1];
                    temp_3 = A[i][jj + 2];
                    temp_4 = A[i][jj + 3];

                    B[jj][i] = temp_1;
                    B[jj + 1][i] = temp_2;
                    B[jj + 2][i] = temp_3;
                    B[jj + 3][i] = temp_4;
                }
    }
}

char trans_test_2_desc[] = "8 * 8 blocking transpose for 32 * 32 - without unrolling";
void trans_test_2(int M, int N, int A[N][M], int B[M][N])
{
    int ii, jj, i, j;

    if (M == 32 && N == 32) {
        for (ii = 0; ii < 32; ii += 8)
            for (jj = 0; jj < 32; jj += 8) 
                for (i = ii; i < ii + 8; i++)
                    for (j = jj; j < jj + 8; j++)
                        B[j][i] = A[i][j]; 
    }
}

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

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

    registerTransFunction(trans_test_1, trans_test_1_desc); 
    registerTransFunction(trans_test_2, trans_test_2_desc); 
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

