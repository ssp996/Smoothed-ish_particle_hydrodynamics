#include <stdio.h>
#include <raylib.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

#define WIDTH 80
#define HEIGHT 80
#define DT 0.001f

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

void draw_particles(Vector2 *positions, int n, int r, Color color)
{
    for (int i = 0; i < n; i++)
    {
        DrawCircle(positions[i].x, positions[i].y, r, color);
    }
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

int main()
{
    int n = 100;
    int r = 3;
    int spacing = 7;

    float rest_density = 1;

    float m = spacing * spacing * rest_density;
    float inv_m = 1/m;

    float h = 1.5f * (float)spacing;
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
    float stiffness_constant = 950.0f;

    float viscosity_coefficicent = 9.5f;

    InitWindow(WIDTH, HEIGHT, "sph");
    SetTargetFPS(100);

    while (!WindowShouldClose())
    {
        for (int iterations = 0; iterations < 10; iterations++)
        {
            #pragma omp parallel for
            for (int i = 0; i < n; i++)
            {
                accelerations[i] = (Vector2){0, 150.1f};
            }

            #pragma omp parallel for
            for (int i = 0; i < n; i++)
            {
                densities[i] = 0.0f;

                for (int j = 0; j < n; j++)
                {
                    Vector2 rel_pos = vec_sub(positions[j], positions[i]);
                    float r2 = sqr_mag(rel_pos);

                    densities[i] += m * poly6(r2, h2, h8);
                }
                pressures[i] = stiffness_constant * (powf(densities[i] / rest_density, gamma) - 1.0f);
                if (pressures[i] < 0.0f) pressures[i] = 0.0f;
            }

            for (int i = 0; i < n; i++)
            {
                
                for (int j = i + 1; j < n; j++)
                {
                    Vector2 rel_pos = vec_sub(positions[j], positions[i]);
                    Vector2 rel_vel = vec_sub(velocities[j], velocities[i]);

                    float r2 = sqr_mag(rel_pos);
                    if (r2 >= h2 || r2 < 1e-6f) continue;

                    float r = sqrtf(r2);

                    float constant_p = m * (pressures[i]/(densities[i] * densities[i]) + pressures[j]/(densities[j] * densities[j]));

                    float laplacian = viscosity_laplacian(rel_pos, r2, r, h, h2, h5);
                    float avg_rho = (densities[i] + densities[j])/2.0f;
                    float constant_v = m * viscosity_coefficicent * laplacian/avg_rho;

                    Vector2 force_p = scalar_mul(spiky_gradient(rel_pos, r, r2, h, h2, h5), constant_p);
                    Vector2 force_v = scalar_mul(rel_vel, constant_v );
                    Vector2 net_force = vec_add(force_p, force_v);

                    accelerations[i] = vec_add(accelerations[i], net_force);
                    accelerations[j] = vec_sub(accelerations[j], net_force);
                }
            }

            #pragma omp parallel for
            for (int i = 0; i < n; i++)
            {
                velocities[i] = vec_add(velocities[i], scalar_mul(accelerations[i], DT));

                positions[i] = vec_add(positions[i], scalar_mul(velocities[i], DT));

                handle_wall_collision(&positions[i], &velocities[i], r);
            }
        }

        ClearBackground(BLACK);
        BeginDrawing();

        draw_particles(positions, n, r, BLUE);

        EndDrawing();
    }
    return 0;
}