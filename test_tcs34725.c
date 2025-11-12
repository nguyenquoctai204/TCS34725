#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#define DEVICE_PATH "/dev/tcs34725"

#define TCS34725_IOCTL_MAGIC 't'
#define TCS34725_IOCTL_READ_R _IOR(TCS34725_IOCTL_MAGIC, 1, int)
#define TCS34725_IOCTL_READ_G _IOR(TCS34725_IOCTL_MAGIC, 2, int)
#define TCS34725_IOCTL_READ_B _IOR(TCS34725_IOCTL_MAGIC, 3, int)
#define TCS34725_IOCTL_READ_C _IOR(TCS34725_IOCTL_MAGIC, 4, int)
#define TCS34725_IOCTL_READ_STATUS _IOR(TCS34725_IOCTL_MAGIC, 5, int)
#define TCS34725_IOCTL_SET_GAIN _IOW(TCS34725_IOCTL_MAGIC, 7, int)


int main() {
    int fd;
    int r,g,b,c;
    int data;
    int ready = 0;
    int gain; // 0=1x, 1=4x, 2=16x, 3=60x


    printf("Enter TCS34725 sensitive: ");
    scanf("%d",&gain);

    // Open TCS34725 device
    fd = open(DEVICE_PATH, O_RDONLY);  
    if (fd < 0) {
        perror("Failed to open the device");
        return errno;
    }
    
    //set gain
    if (ioctl(fd, TCS34725_IOCTL_SET_GAIN, &gain) < 0) {
        perror("Failed to set gain");
    }
    else {
        usleep(1200000); // wait to complete setting
    }
    
    // Read status
    if (ioctl(fd, TCS34725_IOCTL_READ_STATUS, &ready) == -1) {
        perror("IOCTL failed");
    }
    if (ready)
        printf("Data is valid!\n");
    else
        printf("Data is not valid.\n");

    // Read RED value
    if (ioctl(fd, TCS34725_IOCTL_READ_R, &data) < 0) {
        perror("Failed to read RED");
    } else {
        printf("RED: %d\n", data);
        r=data;
    }

    // Read GREEN value
    if (ioctl(fd, TCS34725_IOCTL_READ_G, &data) < 0) {
        perror("Failed to read GREEN");
    } else {
        printf("GREEN: %d\n", data);
        g=data;
    }

    // Read BLUE value
    if (ioctl(fd, TCS34725_IOCTL_READ_B, &data) < 0) {
        perror("Failed to read BLUE");
    } else {
        printf("BLUE: %d\n", data);
        b=data;
    }

    // Read CLEAR value
    if (ioctl(fd, TCS34725_IOCTL_READ_C, &data) < 0) {
        perror("Failed to read CLEAR");
    } else {
        printf("CLEAR: %d\n", data);
        c=data;
    }

    // Determine dominant color
    if (r >= g && r >= b)
        printf("-> Dominant Color: RED\n");
    else if (g >= r && g >= b)
        printf("-> Dominant Color: GREEN\n");
    else
        printf("-> Dominant Color: BLUE\n");

    close(fd);
    return 0;
}
