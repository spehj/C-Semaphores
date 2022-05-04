#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <sys/times.h>

#define HEIGHT_C 480
#define WIDTH_C 640
#define DEPTH_C 3
#define HEIGHT_E 1080
#define WIDTH_E 1920
#define DEPTH_E 2 // screen depth
// Classroom
//#define HEIGHT_E 1024
//#define WIDTH_E 1280

struct tms cpu;
clock_t rt_proc, rt_start, rt_end;
float cpu_usr, cpu_sys, proc_elapsed;
pid_t pid1, pid2;
int ticks;

int main(int argc, char *argv[])
{
    system("v4l2-ctl --set-fmt-video=width=640,height=480,pixelformat=2");
    sleep(1);
    char *pom; // image from /dev/video0
    char *mid_pom;
    unsigned short *screen_img; // Image on the screen
    char vhod[] = "/dev/video0";
    char izhod[] = "/dev/fb0";

    int pipe_1[2], pipe_2[2]; // Both pipes
    pid_t pid_1, pid_2;       // Pids
    ssize_t n_pod_p1, p_pod_p1;
    ssize_t w_pod_p1; // p1
    ssize_t vrstica = WIDTH_C * DEPTH_C;
    ssize_t vrstica_e = WIDTH_C * DEPTH_E;
    ssize_t original_size = WIDTH_C * HEIGHT_C * DEPTH_C;
    ssize_t screen_size = WIDTH_E * HEIGHT_E * DEPTH_E;
    unsigned short *screen;
    int max_cycles = 100;
    int curr_cycle = 0;

    pom = malloc(vrstica);
    mid_pom = malloc(original_size);
    screen_img = malloc(vrstica_e);
    screen = malloc(screen_size);

    if (pipe(pipe_1) == -1)
    {
        printf("Napaka create pipe_1\n");
        exit(2);
    }
    if (pipe(pipe_2) == -1)
    {
        printf("Napaka create pipe_2\n");
        exit(3);
    }
    printf("Tiktakov na sekundo: %d\n", ticks = sysconf(_SC_CLK_TCK));
    pid_1 = fork();

    if (pid_1 == -1)
    {
        printf("Napaka pid_1\n");
    }
    if (pid_1 == 0)
    {
        // Zapri cevi 1 dostop do branja fd[0]
        close(pipe_1[0]);
        // Otrok 1
        // program 1
        n_pod_p1 = HEIGHT_C * WIDTH_C * DEPTH_C;
        unsigned int counter_p1 = 0;
        int fi = open(vhod, O_RDONLY);
        // int fo = open(izhod, O_WRONLY);
        /* Image stream */
        while (1)
        {
            // time
            rt_start = times(&cpu);

            p_pod_p1 = read(fi, pom, vrstica);
            counter_p1 += vrstica;

            if (counter_p1 == n_pod_p1)
            {
                counter_p1 = 0;
            }

            lseek(fi, counter_p1, SEEK_SET);
            // Write to pipe
            w_pod_p1 = write(pipe_1[1], pom, vrstica);

            if (p_pod_p1 == -1)
            {
                printf("%s: Error read %s\n", argv[0], argv[1]);
                exit(4);
            }

            if (w_pod_p1 != vrstica)
            {
                printf("%s: Error write %s\n", argv[0], argv[2]);
                exit(5);
            }
        }
    }
    else
    { // Roditelj 1

        pid_2 = fork();
        if (pid_2 == -1)
        {
            printf("Napaka pid_2\n");
        }
        if (pid_2 == 0)
        {
            // Otrok 2
            // Program 3

            ssize_t p_pod_p3; // How many bytes we read from raw image
            ssize_t w_pod_p3; // How many bytes we will write
            ssize_t c_pod_p3;
            ssize_t vrstica_p3 = 640 * 2;
            unsigned int counter_p3 = 0;
            unsigned int premik = 2 * WIDTH_E - vrstica_p3;
            // Clear screen writing zeros to all the pixels
            memset(screen, 0, WIDTH_E * HEIGHT_E * sizeof(short));
            int fo = open(izhod, O_WRONLY);
            if (fo == -1)
            {
                printf("Napaka open output file. %s\n", argv[0]);
                exit(3);
            }
            c_pod_p3 = write(fo, screen, screen_size);
            if (c_pod_p3 != screen_size)
            {
                printf("%s: Napaka write %s\n", argv[0]);
                exit(6);
            }
            lseek(fo, 0, SEEK_SET);

            while (1)
            {
                // Read rows
                p_pod_p3 = read(pipe_2[0], screen_img, vrstica_p3);
                // Write rows on the screen
                w_pod_p3 = write(fo, screen_img, vrstica_p3);
                // Count rows written
                counter_p3 += 1;
                // Skip pixels on the screen to write next row
                lseek(fo, premik, SEEK_CUR);
                // When image is written start writing from zero
                if (counter_p3 >= HEIGHT_C)
                {
                    lseek(fo, 0, SEEK_SET);
                    counter_p3 = 0;
                    if (curr_cycle < max_cycles)
                    {
                        curr_cycle++;
                    }
                    else if (curr_cycle >= max_cycles)
                    {
                        curr_cycle = 0;
                        rt_end = times(&cpu);
                        rt_proc = rt_end - rt_start;
                        printf("Cas v tiktakih:\n");
                        printf("Realni: %ld \nCPE_U : %ld \nCPE_S : %ld\n",
                               rt_proc, cpu.tms_utime, cpu.tms_stime);
                        cpu_usr = (float)cpu.tms_utime / (float)ticks;
                        cpu_sys = (float)cpu.tms_stime / (float)ticks;
                        proc_elapsed = (float)(rt_proc) / (float)ticks;
                        printf("Casi v sekundah\n");
                        printf("Realni: %6.3f \nCPE_U : %6.3f \nCPE_S : %6.3f\n",
                               proc_elapsed, cpu_usr, cpu_sys);
                    }
                }

                if (p_pod_p3 == -1)
                {
                    printf("%s: Napaka read %s\n", argv[0]);
                    exit(4);
                }
                if (w_pod_p3 != vrstica_p3)
                {
                    printf("%s: Napaka write %s\n", argv[0]);
                    exit(5);
                }
            }
        }
        else
        {
            // Roditelj 2
            // Program 2
            ssize_t p_pod_p2;
            ssize_t w_pod_p2;
            unsigned char red, green, blue;
            // ssize_t original_size = 640 * 480 * 3;
            // ssize_t vrstica = 640 * 3;
            // ssize_t vrstica_e = 640 * 2;
            unsigned short rgb565;
            // unsigned char *pom;
            unsigned short izhodna[vrstica_e];

            while (1)
            {
                p_pod_p2 = read(pipe_1[0], mid_pom, vrstica);
                if (p_pod_p2 == -1)
                {
                    printf("%s: Napaka read %s\n", argv[0]);
                    exit(4);
                }

                int j = 0;
                for (int i = 0; i < vrstica; i = i + 3)
                {
                    red = mid_pom[i];
                    green = mid_pom[i + 1];
                    blue = mid_pom[i + 2];

                    rgb565 = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
                    izhodna[j] = rgb565;
                    j++;
                }

                w_pod_p2 = write(pipe_2[1], izhodna, vrstica_e);

                if (w_pod_p2 != vrstica_e)
                {
                    printf("%s: Napaka write %s\n", argv[0]);
                    exit(5);
                }
            }
        }
    }
}
