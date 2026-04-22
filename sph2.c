#include <stdio.h>
#include <raylib.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

#define WIDTH 504
#define HEIGHT 504
#define DT 0.004f

#define GRID_SIZE_X 48
#define GRID_SIZE_Y 48

float mag(Vector2 v) {
    return sqrtf(v.x * v.x + v.y * v.y); 
}

float sqr_mag(Vector2 v) { 
    return v.x * v.x + v.y * v.y;
}

Vector2 scalar_mul(Vector2 v1, float s) {
    return (Vector2){v1.x * s, v1.y * s};
}

Vector2 vec_sub(Vector2 v1, Vector2 v2) { 
    return (Vector2){v1.x - v2.x, v1.y - v2.y};
}

Vector2 vec_add(Vector2 v1, Vector2 v2)
{
    return (Vector2){v1.x + v2.x, v1.y + v2.y};
}

float poly6(float r2, float h2, float h8) {
    if (r2 >= h2) return 0.0f; 

    float h2_r2 = h2 - r2;

    float normalisation_constant = 4.0f / (M_PI * h8);
    
    return normalisation_constant * h2_r2 * h2_r2 * h2_r2;
}

Vector2 poly6_gradient(Vector2 rel_pos, float r2, float h2, float h8) {
    if (r2 >= h2) return (Vector2){0.0f, 0.0f};

    float h2_r2 = h2 - r2;
    float normalisation_constant = -24.0f / (M_PI * h8);
    float k = normalisation_constant * h2_r2 * h2_r2;
    
    return scalar_mul(rel_pos, k);
}

float spiky(float r, float r2, float h, float h2, float h5) {
    if (r2 >= h2 || r2 == 0.0f) return 0.0f;

    float h_r = h - r;
    
    float normalisation_constant = 10.0f / (M_PI * h5);
    
    return normalisation_constant * h_r * h_r * h_r;
} 


Vector2 spiky_gradient(Vector2 rel_pos, float r, float r2, float h, float h2, float h5) {
    
    if (r2 >= h2) return (Vector2){0.0f, 0.0f};
    
    if (r2 < 0.000001f) return (Vector2){0.0f, 0.0f}; 
    float h_r = h - r;

    float normalisation_constant = -30.0f / (M_PI * h5); 
    float k = normalisation_constant * h_r * h_r / (r + 0.001f);
    
    return scalar_mul(rel_pos, k);
}

float viscosity_laplacian(Vector2 rel_pos, float r2, float r, float h, float h2, float h5) {
    
    if (r2 >= h2) return 0.0f;

    float h_r = h - r;

    float normalisation_constant = 40.0f / (M_PI * h5);
    
    return normalisation_constant * h_r;
}



void init_particles(int n, int spacing, int width, int height, Vector2 *positions)
{
    int side_length = 0;
    while (side_length * side_length < n)
    {
        side_length++;
    }
    float start_x = (width / 2.0f) - ((side_length - 1) * spacing / 2.0f);
    float start_y = (height / 2.0f) - ((side_length - 1) * spacing / 2.0f);

    int count = 0;

    for (int row = 0; row < side_length; row++)
    {
        for (int col = 0; col < side_length; col++)
        {
            if (count < n) 
            {
                float x = start_x + (col * spacing);
                float y = start_y + (row * spacing);
                
                positions[count] = (Vector2){x, y};
                count++;
            }
        }
    }
}

void handle_wall_collision(Vector2 *pos, Vector2 *vel, float radius)
{
    float damping = -0.5f; 
    if (pos->x < radius)
    {
        pos->x = radius;
        vel->x *= damping;
    }


    if (pos->x > WIDTH - radius)
    {
        pos->x = WIDTH - radius;
        vel->x *= damping;
    }


    if (pos->y < radius)
    {
        pos->y = radius;
        vel->y *= damping;
    }


    if (pos->y > HEIGHT - radius)
    {
        pos->y = HEIGHT - radius;
        vel->y *= damping;
    }
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

Color getcolor(float density)
{
    float t = density / 4.5f;
    if (t > 1.0f) t = 1.0f;

    return ColorLerp(BLUE, RED, t);
}

void draw_particles(Vector2 *positions, float *densities, int n, int r)
{
    for (int i = 0; i < n; i++)
    {
        Color col = getcolor(densities[i]);
        DrawCircle(positions[i].x, positions[i].y, r, col);
    }
}


int main()
{
    int n = 3000;
    int r = 3;
    int spacing = 7;

    float rest_density = 1.0f;

    float m = spacing * spacing * rest_density;
    float inv_m = 1/m;

    float h = 1.5f * (float)spacing;


    float vmax = h/DT;

    float cell_size = h;

    float h2 = h * h;
    float h5 = powf(h, 5);
    float h8 = powf(h, 8);

    Vector2 *positions = malloc(n * sizeof(Vector2));
    Vector2 *velocities = calloc(n, sizeof(Vector2));
    Vector2 *accelerations = calloc(n, sizeof(Vector2));
    
    float *densities = malloc(n * sizeof(float));
    float *pressures = malloc(n * sizeof(float));
    float *viscosity = malloc(n * sizeof(float));

    init_particles(n, spacing, WIDTH, HEIGHT, positions);

    float gamma = 7.0f;
    float stiffness_constant = 350.0f;

    float viscosity_coefficicent = 2.5f;

    int *particle_hash = malloc(n * sizeof(int));
    int *particle_index = malloc(n * sizeof(int));

    int *cell_start = malloc(GRID_SIZE_X * GRID_SIZE_Y * sizeof(int));
    int *cell_end = malloc(GRID_SIZE_X * GRID_SIZE_Y * sizeof(int));
    
    int total_cells = GRID_SIZE_X * GRID_SIZE_Y;
    
    InitWindow(WIDTH, HEIGHT, "sph");
    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        Vector2 mousepos = GetMousePosition();
        for (int iterations = 0; iterations < 4; iterations++)
        {
            #pragma omp parallel for
            for (int i = 0; i < n; i++)
            {
                accelerations[i] = (Vector2){0.0f, 0.0f};
            }

            #pragma omp parallel for
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

            #pragma omp parallel for
            for (int i = 0; i < total_cells; i++)
            {
                cell_start[i] = -1;
                cell_end[i] = -1;
            }

            radix_sort(particle_hash, particle_index, n);
            
            for (int i = 0; i < n; i++)
            {
                int current_hash = particle_hash[i];
                int prev_hash = (i == 0) ? -1 : particle_hash[i - 1];

                if (current_hash != prev_hash) 
                {
                    cell_start[current_hash] = i;
                    if (i > 0)
                    {
                        cell_end[prev_hash] = i;
                    }
                }
                if (i == n - 1)
                {
                    cell_end[current_hash] = n;
                }
            }

            #pragma omp parallel for
            for (int i = 0; i < n; i++)
            {
                densities[i] = 0.0f;

                int cx = get_cell_x(positions[i].x, cell_size);
                int cy = get_cell_y(positions[i].y, cell_size);

                for (int dx = -1; dx <= 1; dx++)
                {
                    for (int dy = -1; dy <= 1; dy++)
                    {
                        int nx = cx + dx;
                        int ny = cy + dy;

                        if (nx < 0 || ny < 0 || nx >= GRID_SIZE_X || ny >= GRID_SIZE_Y) continue;

                        int cell = hash_cell(nx, ny);

                        int start = cell_start[cell];
                        if (start == -1) continue;

                        int end = cell_end[cell];

                        for (int k = start; k < end; k++)
                        {
                            int j = particle_index[k];

                            Vector2 rel_pos = vec_sub(positions[j], positions[i]);
                            float r2 = sqr_mag(rel_pos);

                            densities[i] += m * poly6(r2, h2, h8);
                        }
                    }
                }
                pressures[i] = stiffness_constant * (powf(densities[i] / rest_density, gamma) - 1.0f);
                if (pressures[i] < 0.0f) pressures[i] = 0.0f;
            }

            #pragma omp parallel for
            for (int i = 0; i < n; i++)
            {
                int cx = get_cell_x(positions[i].x, cell_size);
                int cy = get_cell_y(positions[i].y, cell_size);

                for (int dx = -1; dx <= 1; dx++)
                {
                    for (int dy = -1; dy <= 1; dy++)
                    {
                        int nx = cx + dx;
                        int ny = cy + dy;

                        if (nx < 0 || ny < 0 || nx >= GRID_SIZE_X || ny >= GRID_SIZE_Y) continue;

                        int cell = hash_cell(nx, ny);

                        int start = cell_start[cell];
                        if (start == -1) continue;

                        int end = cell_end[cell];

                        for (int k = start; k < end; k++)
                        {
                            int j = particle_index[k];

                            if (i == j) continue;

                            Vector2 rel_pos = vec_sub(positions[j], positions[i]);
                            Vector2 rel_vel = vec_sub(velocities[j], velocities[i]);

                            float r2 = sqr_mag(rel_pos);
                            if (r2 >= h2 || r2 < 1e-6f) continue;

                            float r = sqrtf(r2);

                            float constant_p = m * (pressures[i]/(densities[i] * densities[i]) + pressures[j]/(densities[j] * densities[j]));

                            float laplacian = viscosity_laplacian(rel_pos, r2, r, h, h2, h5);
                            float avg_rho = (densities[i] + densities[j]) * 0.5f;
                            float constant_v = m * viscosity_coefficicent * laplacian/avg_rho;

                            Vector2 force_p = scalar_mul(spiky_gradient(rel_pos, r, r2, h, h2, h5), constant_p);
                            Vector2 force_v = scalar_mul(rel_vel, constant_v );
                            Vector2 net_force = vec_add(force_p, force_v);

                            accelerations[i] = vec_add(accelerations[i], net_force);
                        }
                    }
                }
            }

            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
            {
                #pragma omp parallel for
                for (int i = 0; i < n; i++)
                {
                    Vector2 rel = vec_sub(mousepos, positions[i]);
                    float dist = rel.x * rel.x + rel.y * rel.y;
                    if (dist > 64.0f * h * h) continue;
                    Vector2 acc = scalar_mul(rel, 5.0f);
                    accelerations[i] = vec_add(accelerations[i], acc);
                }
            }

            #pragma omp parallel for
            for (int i = 0; i < n; i++)
            {
                velocities[i] = vec_add(velocities[i], scalar_mul(accelerations[i], DT));

                velocities[i] = scalar_mul(velocities[i], 0.997f);

                float v = mag(velocities[i]);
                if (v > vmax)
                {
                    velocities[i] = scalar_mul(velocities[i], vmax/v);
                }

                positions[i] = vec_add(positions[i], scalar_mul(velocities[i], DT));

                handle_wall_collision(&positions[i], &velocities[i], r);
            }
        }

        ClearBackground(BLACK);
        BeginDrawing();

        DrawFPS(10, 10);
        draw_particles(positions, densities, n, r);

        EndDrawing();
    }
    return 0;
}