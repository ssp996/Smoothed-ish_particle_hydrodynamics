#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int get_max(int *arr, int n)
{
    int max = arr[0];
    for (int i = 1; i < n; i++)
    {
        if (arr[i] > max)
        {
            max = arr[i];
        }
    }
    return max;
}

void count_sort(int *arr, int *indices, int n, int shift, int *output, int *out_indices)
{
    int count[256] = {0};

    for (int i = 0; i < n; i++)
    {
        int digit = (arr[i] >> shift) & 0xFF;
        count[digit]++;
    }

    for (int i = 1; i < 256; i++)
    {
        count[i] += count[i - 1];
    }

    for (int i = n - 1; i >= 0; i--)
    {
        int digit = (arr[i] >> shift) & 0xFF;
        output[count[digit] - 1] = arr[i];
        out_indices[count[digit] - 1] = indices[i];

        count[digit]--;
    }

    for (int i = 0; i < n; i++)
    {
        arr[i] = output[i];
    }
}

void radix_sort(int *arr, int *indices, int n)
{
    int max = get_max(arr, n);

    int *output = malloc (n * sizeof(int));
    int *out_indices = malloc (n * sizeof(int));

    for (int shift = 0; (max >> shift) > 0; shift += 8)
    {
        count_sort(arr, indices, n, shift, output, out_indices);
    }

    free(output);
    free(out_indices);
}


int main()
{
    struct timespec start, end;
    srand(time(NULL));
    int n = 1000000;
    int *arr = malloc(n * sizeof(int));
    for (int i = 0; i < n; i++)
    {
        arr[i] = rand() % 2500;
    }

    printf("starting\n");
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    printf("done\n");
    double time_taken = (end.tv_sec - start.tv_sec) + 
                        (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Time: %f seconds\n", time_taken);
    
    return 0;
}