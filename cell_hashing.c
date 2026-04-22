#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WIDTH 504
#define HEIGHT 504

#define GRID_SIZE_X 48
#define GRID_SIZE_Y 48

typedef struct{
    float x;
    float y;
}Vector2;


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
        indices[i] = out_indices[i];
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

int get_cell_x(float x, float cell_size)
{
    return (int)(x / cell_size);
}

int get_cell_y(float y, float cell_size)
{
    return (int)(y / cell_size);
}

int hash_cell(int cx, int cy)
{
    return cy * GRID_SIZE_X + cx;
}

void print_array(int *arr, int n)
{
    for (int i = 0; i < n; i++)
    {
        printf("%4d ", arr[i]);
    }
    printf("\n");
}

int main()
{
    srand(time(NULL));
    int n = 10;
    Vector2 *positions = malloc(n * sizeof(Vector2));
    float h = 1.5f * 7.0f;
    float cell_size = h;

    int total_cells = GRID_SIZE_X * GRID_SIZE_Y;

    int *particle_hash = malloc(n * sizeof(int));
    int *particle_index = malloc(n * sizeof(int));

    int *cell_start = malloc(total_cells * sizeof(int));
    
    for (int i = 0; i < n; i++)
    {
        positions[i] = (Vector2){rand() % 20, rand() % 20};
    }

    for (int i = 0; i < n; i++)
    {
        int cx = get_cell_x(positions[i].x, cell_size);
        int cy = get_cell_y(positions[i].y, cell_size);

        if (cx < 0) cx = 0;
        if (cy < 0) cy = 0;
        if (cx >= GRID_SIZE_X) cx = GRID_SIZE_X - 1;
        if (cy >= GRID_SIZE_Y) cy = GRID_SIZE_Y - 1;

        particle_hash[i] = hash_cell(cx, cy);
        particle_index[i] = i;
    }

    
    printf("before sort:\n");
    print_array(particle_index, n);
    print_array(particle_hash, n);

    radix_sort(particle_hash, particle_index, n);

    printf("after sort:\n");
    print_array(particle_index, n);
    print_array(particle_hash, n);

    
    for (int i = 0; i < total_cells; i++)
    {
        cell_start[i] = -1;
    }
    

    for (int i = 0; i < n; i++)
    {
        int current_hash = particle_hash[i];
        int prev_hash = (i == 0) ? -1 : particle_hash[i - 1];

        if (current_hash != prev_hash) 
        {
            cell_start[current_hash] = i;
        }
    }
    
   
    for (int i = 0; i < total_cells; i++)
    {   
        
        if (cell_start[i] != -1)
        {
            printf("--- Cell %d ---\n", i);
            
            int index = cell_start[i];
            
          
            while (index < n && particle_hash[index] == i)
            {
                printf("  hash: %d, original particle index: %d\n", particle_hash[index], particle_index[index]);
                index++; 
            }
        }
    }

    return 0;
}