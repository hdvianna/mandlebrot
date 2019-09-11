
#include "cuda_runtime.h"
#include "device_launch_parameters.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define MAX_WIDTH 640
#define MAX_HEIGHT 480
#define SIZE 307200 //640*480
#define MAX_ITER 256
#define BLOCKS 4

__global__ void  processMandelbrotAsVector(int *work) {
	int iter, min, max;
	double x, y, zx, zy, zx2, zy2;
	min = MAX_ITER; max = 0;
	double scale = 1. / MAX_ITER;
	double cx = -.6, cy = 0;

	int start = (threadIdx.x * MAX_WIDTH) + 1;
	//int end = start + (int) floor(MAX_HEIGHT * MAX_WIDTH/THREADS_PER_BLOCKS);

	for (int i = start, j = 0; j < MAX_WIDTH; i++, j++) {
		y = (threadIdx.x - MAX_HEIGHT / 2) * scale + cy;
		x = (j - MAX_WIDTH / 2) * scale + cx;
		iter = 0;

		zx = hypot(x - .25, y);
		if (x < zx - 2 * zx * zx + .25) iter = MAX_ITER;
		if ((x + 1)*(x + 1) + y * y < 1 / 16) iter = MAX_ITER;

		zx = zy = zx2 = zy2 = 0;
		for (; iter < MAX_ITER && zx2 + zy2 < 4; iter++) {
			zy = 2 * zx * zy + y;
			zx = zx2 - zy2 + x;
			zx2 = zx * zx;
			zy2 = zy * zy;
		}
		if (iter < min) min = iter;
		if (iter > max) max = iter;
		work[i] = iter;
	}
}

void printVector(int *work) {
	for (int i = 0; i < SIZE; i++) {
		printf("[%i]: %i\n", i, work[i]);
	}
}


int main()
{
	
	cudaError_t cudaStatus;
	int *work = 0;
	int *hwork = (int *) malloc(SIZE * sizeof(int));	
		
	// Choose which GPU to run on, change this on a multi-GPU system.
	cudaStatus = cudaSetDevice(0);
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaSetDevice failed!  Do you have a CUDA-capable GPU installed?");
		goto End;
	}

	cudaStatus = cudaMalloc((void**)&work, sizeof(int) * SIZE);
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaMalloc failed!");
		goto End;
	}

	processMandelbrotAsVector << <1, MAX_HEIGHT >> >(work);

	// Check for any errors launching the kernel
	cudaStatus = cudaGetLastError();
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "addKernel launch failed: %s\n", cudaGetErrorString(cudaStatus));
		goto End;
	}

	// cudaDeviceSynchronize waits for the kernel to finish, and returns
	// any errors encountered during the launch.
	cudaStatus = cudaDeviceSynchronize();
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaDeviceSynchronize returned error code %d after launching addKernel!\n", cudaStatus);
		goto End;
	}

	// Copy output vector from GPU buffer to host memory.
	cudaStatus = cudaMemcpy(hwork, work, SIZE * sizeof(int), cudaMemcpyDeviceToHost);
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaMemcpy failed!");
		goto End;
	}
	
	// cudaDeviceReset must be called before exiting in order for profiling and
	// tracing tools such as Nsight and Visual Profiler to show complete traces.
	cudaStatus = cudaDeviceReset();
	if (cudaStatus != cudaSuccess) {
		fprintf(stderr, "cudaDeviceReset failed!");
		goto End;
	}

	printVector(hwork);

End:
	char ch;
	printf("Programa finalizado. Digite qualquer tecla para continuar ... ");
	scanf("%c", &ch);

	return 0;


}
