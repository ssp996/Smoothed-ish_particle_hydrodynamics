#include "GL/glew.h"
#include "GLFW/glfw3.h"
#include "gltools.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define NUM_CELLS_X 60
#define NUM_CELLS_Y 60
#define N 16384

#define GSSB GL_SHADER_STORAGE_BUFFER
#define GLDC GL_DYNAMIC_COPY

typedef struct{
    float x;
    float y;
}Vector2;

typedef struct{
    int x;
    int y;
}iVector2;

void init_particles(Vector2 *positions, Vector2 *velocities, Vector2 *accelerations, int n, int spacing, int width, int height) 
{
    int side = (int)ceilf(sqrtf(n));
    float start_x = width  / 2.0f - (side - 1) * spacing / 2.0f;
    float start_y = height / 2.0f - (side - 1) * spacing / 2.0f;
    int count = 0;
    for (int row = 0; row < side && count < n; row++) 
    {
        for (int col = 0; col < side && count < n; col++) 
        {
            positions[count].x = start_x + col * spacing;
            positions[count].y = start_y + row * spacing;
            velocities[count].x = 0.0f;
            velocities[count].y = 0.0f;
            accelerations[count].x = 0.0f;
            accelerations[count].y = 0.0f;
            count++;
        }
    }
}

int main()
{
    //sim parameters
    int r = 3;
    int spacing = 7;
    float h = 1.5f * (float)spacing;
    float cell_size = h;
    
    float gravity = 9.81f;
    float target_density = 1.0f;
    float pressure_multiplier = 250.0f;
    float near_pressure_multiplier = 15.0f;
    float coefficient_of_viscosity = 2.5f;

    float spiky2_constant = 6.0f/(M_PI * powf(h, 4));
    float spiky3_constant = 10.0f/(M_PI * powf(h, 5));

    float spiky2_gradient_constant = -12.0f/(M_PI * powf(h, 4));
    float spiky3_gradient_constant = -30.0f/(M_PI * powf(h, 5));
    float poly6_constant = 4.0f/(M_PI * powf(h, 8));

    int num_groups = N/256;
    int clear_value = -1;
    float density0 = 0.0f;

    //dimensions related
    int total_cells = NUM_CELLS_X * NUM_CELLS_Y;

    int WIDTH = (int)(NUM_CELLS_X * cell_size);
    int HEIGHT = (int)(NUM_CELLS_Y * cell_size);

    //simulation data
    Vector2 *positions = malloc(N * sizeof(Vector2));
    Vector2 *velocities = malloc(N * sizeof(Vector2));
    Vector2 *accelerations = malloc(N * sizeof(Vector2));

    int *cell_hashes = malloc(N * sizeof(int));
    int *cell_indices = malloc(N * sizeof(int));

    int *cell_starts = malloc(total_cells * sizeof(int));
    int *cell_ends = malloc(total_cells * sizeof(int));

    float *densities = malloc(N * sizeof(float));
    float *near_densities = malloc(N * sizeof(float));
    float *pressures = malloc(N * sizeof(float));
    float *near_pressures = malloc(N * sizeof(float));

    init_particles(positions, velocities, accelerations, N, spacing, WIDTH, HEIGHT);

    //gl stuff
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "hahah", NULL, NULL);
    glfwMakeContextCurrent(window);
    glewInit();

    //compute shader source files
    char *cell_hashing_src = readFile("cell_hashing.comp");
    char *bitonic_sort_src = readFile("parallel_bitonic.comp");
    char *boundaries_src = readFile("start_end.comp");
    char *density_pressure_computation_src = readFile("density_pressure_computation.comp");
    char *force_computation_src = readFile("force_computation.comp");

    //compute shader creation
    GLuint cell_hashing_program = createComputeShader(cell_hashing_src);
    GLuint bitonic_sort_program = createComputeShader(bitonic_sort_src);
    GLuint boundaries_program = createComputeShader(boundaries_src);
    GLuint density_pressure_computation_program = createComputeShader(density_pressure_computation_src);
    GLuint force_computation_program = createComputeShader(force_computation_program);

    //ssbo declaration
    GLuint positions_ssbo, velocities_ssbo, accelerations_ssbo, 
    cell_hashes_ssbo, cell_indices_ssbo, 
    cell_starts_ssbo, cell_ends_ssbo, 
    densities_ssbo, near_densities_ssbo, pressures_ssbo, near_pressures_ssbo;

    //positions
    glGenBuffers(1, &positions_ssbo);
    glBindBuffer(GSSB, positions_ssbo);
    glBufferData(GSSB, N * sizeof(Vector2), positions, GL_DYNAMIC_COPY);
    glBindBufferBase(GSSB, 0, positions_ssbo);

    //velocities
    glGenBuffers(1, &velocities_ssbo);
    glBindBuffer(GSSB, velocities_ssbo);
    glBufferData(GSSB, N * sizeof(Vector2), velocities, GL_DYNAMIC_COPY);
    glBindBufferBase(GSSB, 1, velocities_ssbo);

    //accelerations
    glGenBuffers(1, &accelerations_ssbo);
    glBindBuffer(GSSB, accelerations_ssbo);
    glBufferData(GSSB, N * sizeof(Vector2), accelerations, GL_DYNAMIC_COPY);
    glBindBufferBase(GSSB, 2, accelerations_ssbo);

    //cell hashes
    glGenBuffers(1, &cell_hashes_ssbo);
    glBindBuffer(GSSB, cell_hashes_ssbo);
    glBufferData(GSSB, N * sizeof(int), cell_hashes, GL_DYNAMIC_COPY);
    glBindBufferBase(GSSB, 3, cell_hashes_ssbo);

    //cell indices
    glGenBuffers(1, &cell_indices_ssbo);
    glBindBuffer(GSSB, cell_indices_ssbo);
    glBufferData(GSSB, N * sizeof(int), cell_indices, GL_DYNAMIC_COPY);
    glBindBufferBase(GSSB, 4, cell_indices_ssbo);

    //cell starts
    glGenBuffers(1, &cell_starts_ssbo);
    glBindBuffer(GSSB, cell_starts_ssbo);
    glBufferData(GSSB, total_cells * sizeof(int), cell_starts, GL_DYNAMIC_COPY);
    glBindBufferBase(GSSB, 5, cell_starts_ssbo);

    //cell ends
    glGenBuffers(1, &cell_ends_ssbo);
    glBindBuffer(GSSB, cell_ends_ssbo);
    glBufferData(GSSB, total_cells * sizeof(int), cell_ends, GL_DYNAMIC_COPY);
    glBindBufferBase(GSSB, 6, cell_ends_ssbo);

    //densities
    glGenBuffers(1, &densities_ssbo);
    glBindBuffer(GSSB, densities_ssbo);
    glBufferData(GSSB, N * sizeof(float), densities, GL_DYNAMIC_COPY);
    glBindBufferBase(GSSB, 7, densities_ssbo);  

    //near densities
    glGenBuffers(1, &near_densities_ssbo);
    glBindBuffer(GSSB, near_densities_ssbo);
    glBufferData(GSSB, N * sizeof(float), near_densities, GL_DYNAMIC_COPY);
    glBindBufferBase(GSSB, 8, near_densities_ssbo);  

    //pressures
    glGenBuffers(1, &pressures_ssbo);
    glBindBuffer(GSSB, pressures_ssbo);
    glBufferData(GSSB, N * sizeof(float), pressures, GL_DYNAMIC_COPY);
    glBindBufferBase(GSSB, 9, pressures_ssbo);

    //near_pressures
    glGenBuffers(1, &near_pressures_ssbo);
    glBindBuffer(GSSB, near_pressures_ssbo);
    glBufferData(GSSB, N * sizeof(float), near_pressures, GL_DYNAMIC_COPY);
    glBindBufferBase(GSSB, 10, near_pressures_ssbo);

    //

    //get locations for uniforms
    //cell hashing shader locations
    GLint hashing_cell_size_loc = glGetUniformLocation(cell_hashing_program, "cell_size");
    GLint hashing_cells_loc = glGetUniformLocation(cell_hashing_program, "cells");
    GLint hashing_num_loc = glGetUniformLocation(cell_hashing_program, "num_particles");

    //sorting shader locations
    GLint sort_k_loc = glGetUniformLocation(bitonic_sort_program, "k");
    GLint sort_j_loc = glGetUniformLocation(bitonic_sort_program, "j");

    //boundary shader locations
    GLint boundaries_num_loc = glGetUniformLocation(boundaries_program, "num_particles");

    //density and pressure shader locations
    GLint d_p_cell_size_loc = glGetUniformLocation(density_pressure_computation_program, "cell_size");
    GLint d_p_h_loc = glGetUniformLocation(density_pressure_computation_program, "h");
    GLint d_p_spiky2_constant_loc = glGetUniformLocation(density_pressure_computation_program, "spiky2_constant");
    GLint d_p_spiky3_constant_loc = glGetUniformLocation(density_pressure_computation_program, "spiky3_constant");
    GLint d_p_target_density_loc = glGetUniformLocation(density_pressure_computation_program, "target_density");
    GLint d_p_pressure_multiplier_loc = glGetUniformLocation(density_pressure_computation_program, "pressure_multiplier");
    GLint d_p_near_pressure_multiplier_loc = glGetUniformLocation(density_pressure_computation_program, "near_pressure_multiplier");
    GLint d_p_cells_loc = glGetUniformLocation(density_pressure_computation_program, "cells");
    GLint d_p_num_particles_loc = glGetUniformLocation(density_pressure_computation_program, "num_particles");

    //force shader locations
    GLint force_gravity_loc = glGetUniformLocation(force_computation_program, "gravity");
    GLint force_h_loc = glGetUniformLocation(force_computation_program, "h");
    GLint force_spiky2_gradient_constant_loc = glGetUniformLocation(force_computation_program, "spiky2_gradient_constant");
    GLint force_spiky3_gradient_constant_loc = glGetUniformLocation(force_computation_program, "spiky3_gradient_constant");
    GLint force_poly6_constant_loc = glGetUniformLocation(force_computation_program, "poly6_constant");
    GLint force_coefficient_of_viscosity_loc = glGetUniformLocation(force_computation_program, "coefficient_of_viscosity");
    GLint force_cell_size_loc = glGetUniformLocation(force_computation_program, "cell_size");
    GLint force_cells_loc = glGetUniformLocation(force_computation_program, "cells");
    GLint force_num_particles_loc = glGetUniformLocation(force_computation_program, "num_particles");

    //send whatever constant data is there to the GPU
    //send cell size, (num_cells_x, num_cells_y), num_particles to hashing shader
    glUseProgram(cell_hashing_program);

    glUniform1f(hashing_cell_size_loc, cell_size);
    glUniform2i(hashing_cells_loc, NUM_CELLS_X, NUM_CELLS_Y);
    glUniform1i(hashing_num_loc, N);

    //send num_particles to boundaries shader
    glUseProgram(boundaries_program);

    glUniform1i(boundaries_num_loc, N);

    //send constants and stuff to density and pressure shader
    glUseProgram(density_pressure_computation_program);
    
    glUniform1f(d_p_cell_size_loc, cell_size);
    glUniform1f(d_p_h_loc, h);
    glUniform1f(d_p_spiky2_constant_loc, spiky2_constant);
    glUniform1f(d_p_spiky3_constant_loc, spiky3_constant);
    glUniform1f(d_p_target_density_loc, target_density);
    glUniform1f(d_p_pressure_multiplier_loc, pressure_multiplier);
    glUniform1f(d_p_near_pressure_multiplier_loc, near_pressure_multiplier);
    glUniform2i(d_p_cells_loc, NUM_CELLS_X, NUM_CELLS_Y);
    glUniform1i(d_p_num_particles_loc, N);

    //send constants to force shader
    glUseProgram(force_computation_program);

    glUniform1f(force_gravity_loc, gravity);
    glUniform1f(force_h_loc, h);
    glUniform1f(force_spiky2_gradient_constant_loc, spiky2_gradient_constant);
    glUniform1f(force_spiky3_gradient_constant_loc, spiky3_gradient_constant);
    glUniform1f(force_poly6_constant_loc, poly6_constant);
    glUniform1f(force_poly6_constant_loc, coefficient_of_viscosity);
    glUniform2i(force_cells_loc, NUM_CELLS_X, NUM_CELLS_Y);
    glUniform1i(force_num_particles_loc, N);

    while (!glfwWindowShouldClose(window))
    {
        //dispatch cell hashing shader
        glUseProgram(cell_hashing_program);
        glDispatchCompute(num_groups, 1, 1);
        
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        //bitonic sort
        glUseProgram(bitonic_sort_program);
        for (int k = 2; k <= N; k *= 2)
        {
            for (int j = k/2; j > 0; j /= 2)
            {
                glUniform1i(sort_k_loc, k);
                glUniform1i(sort_j_loc, j);

                glDispatchCompute(num_groups, 1, 1);

                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            }
        }

        //clear cell starts and ends to -1
        glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

        glClearNamedBufferData(cell_starts_ssbo, GL_R32I, GL_RED_INTEGER, GL_INT, &clear_value);
        glClearNamedBufferData(cell_ends_ssbo, GL_R32I, GL_RED_INTEGER, GL_INT, &clear_value);

        //dispatch boundaries shader
        glUseProgram(boundaries_program);
        glDispatchCompute(num_groups, 1, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        //dispatch density and pressure shader
        glUseProgram(density_pressure_computation_program);
        glDispatchCompute(num_groups, 1, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        //dispatch force shader
        glUseProgram(force_computation_program);
        glDispatchCompute(num_groups, 1, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }
    
    glfwTerminate();

    return 0;
}
