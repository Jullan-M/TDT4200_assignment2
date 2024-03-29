#include <stdlib.h>
#include <stdio.h>
#include "bitmap.h"
#include <mpi.h>

#define XSIZE 2560 // Size of before image
#define YSIZE 2048

void invert_colors(uchar* image, int local_ysz) {
    for (int y = 0; y < local_ysz; y++) {
        for (int x = 0; x < XSIZE; x++) {
            for (int c = 0; c < 3; c++) {
                image[3 * XSIZE * y + x * 3 + c] = 255 - image[3 * XSIZE * y + x * 3 + c];
            }
        }
    }
}

void flip_image(uchar* image, int local_ysz) {
    // First we make a copy of the image
    uchar *temp = malloc(XSIZE * local_ysz * 3);
    for (int y = 0; y < local_ysz; y++) {
        for (int x = 0; x < XSIZE; x++) {
            for (int c = 0; c < 3; c++) {
                temp[3 * XSIZE * y + x * 3 + c] = image[3 * XSIZE * y + x * 3 + c];
            }
        }
    }

    // Now the flipping begins
    for (int y = 0; y < local_ysz; y++) {
        for (int x = 0; x < XSIZE; x++) {
            for (int c = 0; c < 3; c++) {
                image[3 * XSIZE * y + (XSIZE - x - 1) * 3 + c] = temp[3 * XSIZE * y + x * 3 + c];
            }
        }
    }
    free(temp);
}

void all_blacks_are_now_blue(uchar* image, int local_ysz) {
    for (int y = 0; y < local_ysz; y++) {
        for (int x = 0; x < XSIZE; x++) {
            uchar* b = & image[3 * XSIZE * y + x * 3 + 0];
            uchar* g = & image[3 * XSIZE * y + x * 3 + 1];
            uchar* r = & image[3 * XSIZE * y + x * 3 + 2];

            if (*r < 10 && *g < 10 && *b < 10) {
                *r = 0;
                *b = 255;
                *g = 0;
            }
        }
    }
}

void double_image_size(const uchar* image_old, uchar* image_new, int local_ysz) {
    // Duplicate each pixel in original image to 2x2 areas in new image.
    for (int y = 0; y < local_ysz; y++) {
        for (int x = 0; x < XSIZE; x++) {
            for (int c = 0; c < 3; c++) {
                image_new[3 * (2 * XSIZE) * (2*y) + (2*x) * 3 + c] = image_old[3 * XSIZE * y + x * 3 + c];
                image_new[3 * (2 * XSIZE) * (2*y) + (2*x + 1) * 3 + c] = image_old[3 * XSIZE * y + x * 3 + c];
                image_new[3 * (2 * XSIZE) * (2*y + 1) + (2*x) * 3 + c] = image_old[3 * XSIZE * y + x * 3 + c];
                image_new[3 * (2 * XSIZE) * (2*y + 1) + (2*x + 1) * 3 + c] = image_old[3 * XSIZE * y + x * 3 + c];
            }
        }
    }
}


int main() {
    uchar* image;
    uchar* image_db;

    // Do the typical MPI initialization
    int comm_sz, my_rank;
    MPI_Init(NULL, NULL);
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &comm_sz);
    MPI_Comm_rank(comm, &my_rank);

    // We divide the image row-wise and distribute the rows to the different processes
    int local_YSZ = YSIZE / comm_sz;
    int local_n = YSIZE * XSIZE * 3 / comm_sz;

    // Initialize the local images
    uchar* local_im = calloc(local_n, 1);
    uchar* local_im_db = malloc(4 * local_n);

    if (my_rank == 0) {
        // Root process "stitches" the image together from other processes
        image = calloc(YSIZE * XSIZE * 3, 1);
        image_db = calloc(4 * YSIZE * XSIZE * 3, 1);
        readbmp("before.bmp", image);
    }

    // Use MPI_Scatter to distribute data to the processes
    MPI_Scatter(image, local_n, MPI_UNSIGNED_CHAR, local_im, local_n, MPI_UNSIGNED_CHAR, 0, comm);

    // Inverting the colors
    invert_colors(local_im, local_YSZ);

    // Flipping the image horizontally
    flip_image(local_im, local_YSZ);

    // Change some colors
    all_blacks_are_now_blue(local_im, local_YSZ);

    // Resize image
    double_image_size(local_im, local_im_db, local_YSZ);

    // Gather all local images and create a picture out of it
    MPI_Gather(local_im, local_n, MPI_UNSIGNED_CHAR, image, local_n, MPI_UNSIGNED_CHAR, 0, comm);
    MPI_Gather(local_im_db, 4 * local_n, MPI_UNSIGNED_CHAR, image_db, 4 * local_n, MPI_UNSIGNED_CHAR, 0, comm);

    if (my_rank == 0) {
        savebmp("after.bmp", image, XSIZE, YSIZE);
        savebmp("after_4x.bmp", image_db, 2 * XSIZE, 2 * YSIZE);

        free(image);
        free(image_db);
    }

    // Shut down MPI
    MPI_Finalize();

	return 0;
}
