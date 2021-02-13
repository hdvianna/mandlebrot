/**
 * Developed by Henrique Damasceno Vianna - https://www.linkedin.com/in/hdvianna/
 *
 * Solution for the exercise of high performance computing class from
 * the Unisinos Graduate Programa of Applied Computing - September/2016
 * 
 * Sorce code based in https://rosettacode.org/wiki/Mandelbrot_set#PPM_Interactive
 * 
 * Compile with:
 *   gcc mandelbrot.c -lpthread -lglut -lGLU -lGL -lm -o mandelbrot
 *
 * POOL_SIZE	META_WIDTH	META_HEIGHT	WORKERS	BUFFER_SIZE
 * 1			640			480			1		3
 * 4			320			240			2		6
 * 16			160			120			4		12
 * 64			80			60			8		24
 * 256			40			30			16		48
 * 1024			20			15			32		96
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GL/glut.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <pthread.h>
 
#define MAX_WIDTH 640
#define MAX_HEIGHT 480
#define MAX_ITER 256

int META_WIDTH = 20;
int META_HEIGHT = 15;

int BUFFER_SIZE = 1024;
int POOL_SIZE = 1024;
int NUMBER_OF_WORKERS = 32;

double scale = 1./MAX_ITER;
double cx = -.6, cy = 0;

typedef GLfloat point2[2];

typedef struct {
	unsigned int x1, y1, x2, y2;
} meta_work_t;

 typedef struct {
	 meta_work_t work;
	 double **result;
	 int min, max;
 } work_result_t;

pthread_t thid_work_producer;
pthread_t *thid_meta_workers;//thid_meta_workers[NUMBER_OF_WORKERS];
pthread_t thid_result_consumer;

/**
 * Protected variables of the metadata work pool
 */
meta_work_t *meta_pool; //meta_pool[POOL_SIZE]
pthread_mutex_t meta_pool_lock;
pthread_cond_t meta_pool_cond_produced;
pthread_cond_t meta_pool_cond_consumed;
int meta_pool_tail_pointer = -1;
int meta_pool_head_pointer = -1;

/**
 * Protected variables of the result pool
 */
work_result_t *result_pool; //result_pool[POOL_SIZE];
pthread_mutex_t result_pool_lock;
pthread_cond_t result_pool_cond_produced;
pthread_cond_t result_pool_cond_consumed;
int result_pool_tail_pointer = -1;
int result_pool_head_pointer = -1;

int argc; 
char** argv;

int printPoint(int x, int y) {
	point2 p;
	p[0] = (float) x;
	p[1] = (float) (y);	
	glBegin(GL_POINTS);
	glVertex2fv(p);
	glEnd();
}

void setupParameters(int option) {
	switch(option) {
		case 1:
			META_WIDTH = 640;
			META_HEIGHT = 480;
			POOL_SIZE = 1;
			NUMBER_OF_WORKERS = 1;
			BUFFER_SIZE = 3;
			break;
		case 2:
			META_WIDTH = 320;
			META_HEIGHT = 240;
			POOL_SIZE = 4;
			BUFFER_SIZE = 6;
			break;
		case 3:
			META_WIDTH = 160;
			META_HEIGHT = 120;
			POOL_SIZE = 16;
			NUMBER_OF_WORKERS = 4;
			BUFFER_SIZE = 12;
			break;
		case 4:
			META_WIDTH = 80;
			META_HEIGHT = 60;
			POOL_SIZE = 64;
			NUMBER_OF_WORKERS = 8;
			BUFFER_SIZE = 24;
			break;
		case 5:
			META_WIDTH = 40;
			META_HEIGHT = 30;
			POOL_SIZE = 256;
			NUMBER_OF_WORKERS = 16;
			BUFFER_SIZE = 48;
			break;
		case 6:
			META_WIDTH = 20;
			META_HEIGHT = 15;
			POOL_SIZE = 1024;
			NUMBER_OF_WORKERS = 32;
			BUFFER_SIZE = 96;
			break;
		case 7:
			META_WIDTH = 80;
			META_HEIGHT = 60;
			POOL_SIZE = 64;
			NUMBER_OF_WORKERS = 8;
			BUFFER_SIZE = 64;
			break;
		case 8:
			META_WIDTH = 40;
			META_HEIGHT = 30;
			POOL_SIZE = 256;
			NUMBER_OF_WORKERS = 16;
			BUFFER_SIZE = 256;
			break;
		case 9:
			META_WIDTH = 20;
			META_HEIGHT = 15;
			POOL_SIZE = 1024;
			NUMBER_OF_WORKERS = 32;
			BUFFER_SIZE = 1024;
			break;	
	}
	thid_meta_workers = (pthread_t*) malloc( NUMBER_OF_WORKERS * sizeof( pthread_t ));
	meta_pool = (meta_work_t*) malloc( POOL_SIZE * sizeof( meta_work_t ));
	result_pool = (work_result_t*) malloc( POOL_SIZE * sizeof( work_result_t ));
}

void startObjects() {
	pthread_mutex_init (&meta_pool_lock, NULL);	
	pthread_cond_init(&meta_pool_cond_produced, NULL);
	pthread_cond_init(&meta_pool_cond_consumed, NULL);
	pthread_mutex_init (&result_pool_lock, NULL);
	pthread_cond_init(&result_pool_cond_produced, NULL);
	pthread_cond_init(&result_pool_cond_consumed, NULL);
}

void destroyObjects() {
	pthread_mutex_destroy(&meta_pool_lock);	
	pthread_cond_destroy(&meta_pool_cond_produced);
	pthread_cond_destroy(&meta_pool_cond_consumed);
	pthread_mutex_destroy(&result_pool_lock);	
	pthread_cond_destroy(&result_pool_cond_produced);
	pthread_cond_destroy(&result_pool_cond_consumed);
}

int isMetaPoolEmpty() {
	return meta_pool_head_pointer == meta_pool_tail_pointer;
}

int isMetaPoolComplete() {
	return (sizeof(meta_pool)/sizeof(meta_work_t)) == POOL_SIZE;
}

int isMetaPoolBufferFull() {
	return (meta_pool_tail_pointer - meta_pool_head_pointer) == BUFFER_SIZE;
}

int allMetaConsumed() {
	return isMetaPoolEmpty() && meta_pool_head_pointer == POOL_SIZE - 1;
}

int hasMeta() {
	return meta_pool_tail_pointer > meta_pool_head_pointer;
}

void pushMeta(meta_work_t meta_work) {
	meta_pool_tail_pointer++;			
	meta_pool[meta_pool_tail_pointer] = meta_work;				
}

meta_work_t popMeta() {
	meta_pool_head_pointer++;
	return meta_pool[meta_pool_head_pointer];
}

int isResultPoolEmpty() {
	return result_pool_head_pointer == result_pool_tail_pointer;
}

int isResultPoolComplete() {
	return (sizeof(result_pool)/sizeof(work_result_t)) == POOL_SIZE;
}

int isResultPoolBufferFull() {
	return (result_pool_tail_pointer - result_pool_head_pointer) == BUFFER_SIZE;
}

int allResultConsumed() {
	return isResultPoolEmpty() && result_pool_head_pointer == POOL_SIZE - 1;
}

int hasResult() {
	return result_pool_tail_pointer > result_pool_head_pointer;
}

void pushResult(work_result_t result) {
	result_pool_tail_pointer++;			
	result_pool[result_pool_tail_pointer] = result;				
}

work_result_t popResult() {
	result_pool_head_pointer++;
	return result_pool[result_pool_head_pointer];
}

void * produceMetaWork() {
	for(int y = 0; y < MAX_HEIGHT; y+=META_HEIGHT) {
		for(int x = 0; x < MAX_WIDTH; x+=META_WIDTH) {
			pthread_mutex_lock(&meta_pool_lock);
			while(isMetaPoolBufferFull()) {
				pthread_cond_wait(&meta_pool_cond_consumed, &meta_pool_lock);
			}
			pthread_mutex_unlock (&meta_pool_lock);
			meta_work_t meta_work;
			meta_work.y1 = y;
			meta_work.y2 = (y + META_HEIGHT) - 1;	
			meta_work.x1 = x;
			meta_work.x2 = (x + META_WIDTH);
			pthread_mutex_lock(&meta_pool_lock);
			pushMeta(meta_work);
			pthread_cond_signal (&meta_pool_cond_produced);
			pthread_mutex_unlock (&meta_pool_lock);
		}
	}	
}

void startMetaProducer() {
	pthread_create (&thid_work_producer, NULL, produceMetaWork, NULL);
}

void joinMetaProducer() {
	pthread_join(thid_work_producer, NULL);
}

double **newResult() {
	double **result =  (double**) malloc( META_HEIGHT * sizeof( double* ));
	for(int i=0; i < META_HEIGHT; i++) {
		result[i] = (double*) malloc( META_WIDTH * sizeof( double ));
	}
	return result;
}

work_result_t consumeMetaWork(meta_work_t work) {	
	int i, j, iter, min, max;
	double **result = newResult();	
	double x, y, zx, zy, zx2, zy2;
	min = MAX_ITER; max = 0;
	for (i = work.y1; i <= work.y2; i++) {
		y = (i - MAX_HEIGHT/2) * scale + cy;
		for (j = work.x1; j  <= work.x2; j++) {
			x = (j - MAX_WIDTH/2) * scale + cx;
			iter = 0;

			zx = hypot(x - .25, y);
			if (x < zx - 2 * zx * zx + .25) iter = MAX_ITER;
			if ((x + 1)*(x + 1) + y * y < 1/16) iter = MAX_ITER;

			zx = zy = zx2 = zy2 = 0;
			for (; iter < MAX_ITER && zx2 + zy2 < 4; iter++) {
				zy = 2 * zx * zy + y;
				zx = zx2 - zy2 + x;
				zx2 = zx * zx;
				zy2 = zy * zy;
			}
			if (iter < min) min = iter;
			if (iter > max) max = iter;
			result[i-work.y1][j-work.x1] = iter; //Produz o pixel
		}
	}
	work_result_t work_result;
	work_result.work = work;	
	work_result.result = result;
	work_result.min = min;
	work_result.max = max;
	return work_result;
}

void * metaWorker(void *idx) {
	int id = *(int*) idx;
	while(!allMetaConsumed()) {
		pthread_mutex_lock(&meta_pool_lock);
		while(!hasMeta() && !allMetaConsumed()) {
			pthread_cond_wait(&meta_pool_cond_produced, &meta_pool_lock);
		}

		if(!allMetaConsumed()) {
			meta_work_t meta = popMeta();			
			pthread_mutex_unlock(&meta_pool_lock);
			work_result_t result = consumeMetaWork(meta);
			pthread_cond_signal (&meta_pool_cond_consumed);
			pthread_mutex_lock(&result_pool_lock);
			while(isResultPoolBufferFull()) {
				pthread_cond_wait(&result_pool_cond_consumed, &result_pool_lock);
			}
			pushResult(result);						
			pthread_cond_signal (&result_pool_cond_produced);
			pthread_mutex_unlock(&result_pool_lock);
		} else {
			pthread_mutex_unlock(&meta_pool_lock);
		}		
	}
	pthread_cond_signal (&meta_pool_cond_produced);
}

void startMetaWorkers() {
	int *idx;
	for(int i = 0; i < NUMBER_OF_WORKERS; i++) {
		idx = (int*) malloc(sizeof(int));
		*idx = i;		
		pthread_create (&thid_meta_workers[i], NULL, metaWorker, (void*) idx);
	}
}

void joinMetaWorkers() {
	for(int i = 0; i < NUMBER_OF_WORKERS; i++) {		
		pthread_join(thid_meta_workers[i], NULL);
	}	
}

void displayResult(work_result_t work_result) {
	for(int i=0; i < META_HEIGHT; i++) {
		for(int j=0; j <= META_WIDTH; j++) {
			double color = 1-(work_result.result[i][j]/MAX_ITER);
			glColor3f(color, color, color);
			printPoint(work_result.work.x1 + j, work_result.work.y1 + i);
		}
	}	
}

void display() {
	glClear(GL_COLOR_BUFFER_BIT);
	while(!allResultConsumed()) {
		pthread_mutex_lock(&result_pool_lock);
		while(!hasResult() && !allResultConsumed()) {
			pthread_cond_wait(&result_pool_cond_produced, &result_pool_lock);
		}
		if(!allResultConsumed()) {			
			work_result_t result = popResult();
			pthread_cond_signal (&result_pool_cond_consumed);
			pthread_mutex_unlock(&result_pool_lock);
			displayResult(result);
			glFlush();		
		} else {
			pthread_mutex_unlock(&result_pool_lock);
		}
	}	
}

void setUpGlut(int argc, char** argv) {
	glutInit(&argc, argv);
	glutInitDisplayMode (GLUT_SINGLE | GLUT_RGB);
	glutInitWindowSize (MAX_WIDTH, MAX_HEIGHT);
	glutInitWindowPosition (0, 0);
    glutCreateWindow ("Mandelbrot");
	glClearColor(1.0, 1.0, 1.0, 1.0);
    glColor3f(0.0, 0.0, 0.0);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0.0, (double) MAX_WIDTH, 0.0, (double) MAX_HEIGHT);
    glMatrixMode(GL_MODELVIEW);
	glutDisplayFunc(display);	
	glutMainLoop();	
}

void * consumerResult() {
	setUpGlut(argc, argv);
}

void startResultConsumer() {
	pthread_create (&thid_result_consumer, NULL, consumerResult, NULL);
}

void joinResultConsumer() {
	pthread_join(thid_result_consumer, NULL);
}

/**
 * Main process
 */
void process(int option) {
	setupParameters(option);
	startObjects();

	startMetaProducer();
	startMetaWorkers();		
	startResultConsumer();

	joinMetaProducer();
	joinMetaWorkers();
	joinResultConsumer();

	destroyObjects();	
}


int option() {
	printf("OPTION	|POOL_SIZE	|WIDTH		|HEIGHT		|WORKERS	|BUFFER\n");
	printf("-------------------------------------------------------------------------------\n");
	printf("1 	|1		|640		|480		|1		|3\n");
	printf("2 	|4		|320		|240		|2		|6\n");
	printf("3 	|16		|160		|120		|4		|12\n");
	printf("4 	|64		|80		|60		|8		|24\n");
	printf("5 	|256		|40		|30		|16		|48\n");
	printf("6 	|1024		|20		|15		|32		|96\n");
	printf("7 	|64		|80		|60		|8		|64\n");
	printf("8 	|256		|40		|30		|16		|256\n");
	printf("9 	|1024		|20		|15		|32		|1024\n");
	
	char soption[1024];
	int option;
	while (1) {
		printf("Select one option [1-9]:");
		scanf("%s", soption);
		sscanf(soption, "%d", &option);
		if (option >= 1 && option <= 9) {
			return option; 
		} else {
			printf("Invalid option\n");
		}
	}		
}

int main(int _argc, char** _argv) {
	argc = _argc; 
	argv = _argv;
	process(option());	
	return 0;
}
