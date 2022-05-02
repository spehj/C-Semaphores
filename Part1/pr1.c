#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/ipc.h>

#define HEIGHT_C 480
#define WIDTH_C 640
#define DEPTH_C 3

// definiraj glede na rezolucijo ekrana
#define HEIGHT_E 1080
#define WIDTH_E 1920
#define DEPTH_E 2 // screen depth, 16bpp

#define NPOD1 HEIGHT_C * WIDTH_C * DEPTH_C
#define NPOD2 HEIGHT_C * WIDTH_C * DEPTH_E


#define SEM_1 0 // read
#define SEM_2 1	// write
#define SEM_3 0	// read
#define SEM_4 1	// write

char vhod[] = "/dev/video0";
char izhod[] = "/dev/fb0";

// Code for processes
int proces1(), proces2(), proces3();
// Shared memory ID
int shm_ID1, shm_ID2;
// Semaphore ID
int sem_ID1, sem_ID2; 

int inx, i, j;
char *shm_Read1, *shm_Write1, *shm_Read2, *shm_Write2;
struct sembuf semafor1;
struct sembuf semafor2;

pid_t pid1, pid2;

int main(int argc, char *argv[]){
    system("v4l2-ctl --set-fmt-video=width=640,height=480,pixelformat=2");
    sleep(1);
    
    // Shared memory
	// shmget allocates a System V shared memory segment
	// shmget returns the identifier of the System V shared memory (associated with key argument)
	// key parameter can be a key returned from ftok() function (if we would heave 3 programs not related)
	// key parameter have a value IPC_PRIVATE to indicate that the shared memory cannot be accessed by other processes

	// NPOD1 is a memory for image from the camera
	// IPC_CREAT vreates a new segment
    if((shm_ID1 = shmget(IPC_PRIVATE, NPOD1, IPC_CREAT | 0666)) == -1)
		perror("shmget1 err");
    if((shm_ID2 = shmget(IPC_PRIVATE, NPOD2, IPC_CREAT | 0666)) == -1)
		perror("shmget2 err");

    // Semaphores
	// with semget get a System V semaphore set identifier
	// Here we create 2 semaphore sets with 2 semaphores
	sem_ID1 = semget(IPC_PRIVATE, 2, 0666);
    sem_ID2 = semget(IPC_PRIVATE, 2, 0666);

	// Semctl are System V semaphore control operations
	// Semnum is the n-th semafore from the semaphore set
	// SETVAL: set the semaphore value
	semctl(sem_ID1, SEM_1, SETVAL, 1); 
	semctl(sem_ID1, SEM_2, SETVAL, 0);
    semctl(sem_ID2, SEM_3, SETVAL, 0);
    semctl(sem_ID2, SEM_4, SETVAL, 1);

	// fork()
	pid1 = fork();
	if(pid1==0){
		proces1();
	}
	else{
		pid2=fork();
		if(pid2==0){
			proces2();
		}
		else{
			proces3();
		}
	}
	exit(0);
}


int proces1(){
	char *pom;
    int fi;
    ssize_t n_pod;

    n_pod = NPOD1;
    pom = malloc(n_pod);

    fi = open(vhod, O_RDONLY);
	//shmat() attaches the System V shared memory segment identified by shmid to the address space of the calling process.
	// if smhaddr is NULL, system chooses a suitable (unused) address for the segment
	// 0: Use the existing segment associated with the key.
	if((shm_Write1 = shmat(shm_ID1, NULL, 0 )) == (void *) -1 ){
		perror("shmat err");
		exit(2);
	}
	
	while (1){
		read(fi, pom, n_pod);

		semafor1.sem_num = SEM_1;
		semafor1.sem_op = -1;
		semafor1.sem_flg = 0;
		semop(sem_ID1, &semafor1, 1);

        memcpy(shm_Write1, pom, n_pod);

		semafor1.sem_num = SEM_2;
		semafor1.sem_op = 1;
		semafor1.sem_flg = 0;
		semop(sem_ID1, &semafor1, 1);
	}
}


int proces2(){
	char red, green, blue;
    ssize_t original_size = 640 * 480 * 3;
    ssize_t resized_size = 640 * 480 * 2;
    unsigned short rgb565;
    char *pom;
    unsigned short izhodna[resized_size];

    pom = malloc(original_size);

	if((shm_Read1 = shmat(shm_ID1, NULL, 0 )) == (void *) -1 ){
		perror("shmat err"); 
		exit(2);
	}
	if((shm_Write2 = shmat(shm_ID2, NULL, 0 )) == (void *) -1 ){
		perror("shmat err");
		exit(2);
	}
	
	while(1){
		semafor1.sem_num = SEM_2;
		semafor1.sem_op = -1;
		semafor1.sem_flg = 0;
		semop(sem_ID1, &semafor1, 1);	

		memcpy(pom, shm_Read1, original_size);

        semafor1.sem_num = SEM_1;
		semafor1.sem_op = 1;
		semafor1.sem_flg = 0;
		semop(sem_ID1, &semafor1, 1);

		int j=0;
		for(int i = 0; i < original_size; i = i + 3){
			red = pom[i];
            green = pom[i + 1];
            blue = pom[i + 2];

            rgb565 = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
            izhodna[j] = rgb565;
            j++;
		}
		
        semafor2.sem_num = SEM_3;
		semafor2.sem_op = -1;
		semafor2.sem_flg = 0;
		semop(sem_ID2, &semafor2, 1);

		memcpy(shm_Write2, izhodna, resized_size);

		semafor2.sem_num = SEM_4;
		semafor2.sem_op = 1;
		semafor2.sem_flg = 0;
		semop(sem_ID2, &semafor2, 1);
		}
}


int proces3(){
	unsigned short *pom; 
    int fo;
    ssize_t n_pod; 

    n_pod = NPOD2; //(640x480x2)
    pom = malloc(n_pod);

    unsigned short *screen;                           
    ssize_t screen_size = WIDTH_E * HEIGHT_E * DEPTH_E; 

    screen = malloc(screen_size);

    fo = open(izhod, O_WRONLY);

	if((shm_Read2 = shmat(shm_ID2, NULL, 0 )) == (void *) -1 ){
		perror("shmat err"); exit(2);
	}

	while(1){
		semafor2.sem_num = SEM_4;
		semafor2.sem_op = -1;
		semafor2.sem_flg = 0;
		semop(sem_ID2, &semafor2, 1);
		
		memcpy(pom, shm_Read2, n_pod);

		semafor2.sem_num = SEM_3;
		semafor2.sem_op = 1;
		semafor2.sem_flg = 0;
		semop(sem_ID2, &semafor2, 1);
		
        for (int i = 0; i < HEIGHT_C; i++)
        {
            // columns
            for (int j = 0; j < WIDTH_C; j++)
            {
                screen[i * WIDTH_E + j] = pom[i * WIDTH_C + j];
            }
        }
		write(fo, screen, screen_size);
		lseek(fo, 0, SEEK_SET);
	}
}