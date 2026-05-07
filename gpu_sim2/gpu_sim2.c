#include "GL/glew.h"
#include "GLFW/glfw3.h"
#include "gltools.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define NUM_CELLS_X 183
#define NUM_CELLS_Y 98
#define N 8192 * 2 
#define DT 0.004

#define GSSB GL_SHADER_STORAGE_BUFFER
#define GLDC GL_DYNAMIC_COPY

typedef struct{
    float x;
    float y;
}Vector2;

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

            float jitter_x = (((float)rand() / RAND_MAX) * 0.2f) - 0.1f;
            float jitter_y = (((float)rand() / RAND_MAX) * 0.2f) - 0.1f;

            positions[count].x = start_x + col * spacing + jitter_x;
            positions[count].y = start_y + row * spacing + jitter_y;
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
    float h2 = h * h;
    float cell_size = h;
    float velocity_cap = h/DT;


    float gravity = -150.0f;
    float target_density = 0.5f;
    float initial_density = target_density;
    float pressure_multiplier = 250.0f;
    float coefficient_of_viscosity = 62.5f;
    float collision_damping = 0.5f;
    float velocity_damping = 0.999f;

    float m = spacing * spacing * initial_density;

    int num_groups = N/256;
    int clear_value = -1;
    float density0 = 0.0f;

    float spiky3_constant = 10.0f/(M_PI * powf(h, 5));
    float poly6_constant = 4.0f/(M_PI * powf(h, 8));
    float viscosity_laplacian_constant = 40/(M_PI * powf(h, 5));
    float spiky3_gradient_constant = -30.0f/(M_PI * powf(h, 5));

    float dist_threshold = 64;
    float attractor_multiplier = 20.0f;
    float repeller_multiplier = 15.0f;

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

    memset(cell_starts, -1, total_cells * sizeof(int));
    memset(cell_ends, -1, total_cells * sizeof(int));

    float *densities = malloc(N * sizeof(float));
    float *pressures = malloc(N * sizeof(float));

    init_particles(positions, velocities, accelerations, N, spacing, WIDTH, HEIGHT);

    //unit quad 
    float quad_vertices[] = {
        -0.5f, -0.5f, 
        0.5f, -0.5f, 
        -0.5f,  0.5f, 
        0.5f,  0.5f
    };

    //gl stuff
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "hahah", NULL, NULL);
    glfwMakeContextCurrent(window);
    glewInit();

    char *cell_hashing_src = readFile("cell_hashing.comp");
    char *bitonic_sort_src = readFile("parallel_bitonic.comp");
    char *boundaries_src = readFile("start_end.comp");
    char *density_pressure_src = readFile("density_pressure_shader.comp");
    char *force_shader_src = readFile("force_shader.comp");
    char *external_force_src = readFile("external_force.comp");
    char *update_shader_src = readFile("update_shader.comp");

    //vertex and fragment shader source files
    char *vsrc = readFile("quad.vert");
    char *fsrc = readFile("quad.frag");

    //compute shader creation
    GLuint cell_hashing_program = createComputeShader(cell_hashing_src);
    GLuint bitonic_sort_program = createComputeShader(bitonic_sort_src);
    GLuint boundaries_program = createComputeShader(boundaries_src);
    GLuint density_pressure_program = createComputeShader(density_pressure_src);
    GLuint force_program = createComputeShader(force_shader_src);
    GLuint external_force_program = createComputeShader(external_force_src);
    GLuint update_program = createComputeShader(update_shader_src);

    //vertex and fragment shader creation
    GLuint render_program = createProgram(vsrc, fsrc);

    //ssbo declaration
    GLuint positions_ssbo, velocities_ssbo, accelerations_ssbo, 
    cell_hashes_ssbo, cell_indices_ssbo, 
    cell_starts_ssbo, cell_ends_ssbo, 
    densities_ssbo, pressures_ssbo;

    //vao and vbo declaration
    GLuint quad_vao, quad_vbo;

    //ssbo creatiion, binding
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

    //pressures
    glGenBuffers(1, &pressures_ssbo);
    glBindBuffer(GSSB, pressures_ssbo);
    glBufferData(GSSB, N * sizeof(float), pressures, GL_DYNAMIC_COPY);
    glBindBufferBase(GSSB, 8, pressures_ssbo);

    //vao and vbo creation, binding
    glGenVertexArrays(1, &quad_vao);
    glGenBuffers(1, &quad_vbo);

    glBindVertexArray(quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glBindVertexArray(0);

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
    GLint d_p_cell_size_loc = glGetUniformLocation(density_pressure_program, "cell_size");
    GLint d_p_h_loc = glGetUniformLocation(density_pressure_program, "h");
    GLint d_p_h2_loc = glGetUniformLocation(density_pressure_program, "h2");
    GLint d_p_m_loc = glGetUniformLocation(density_pressure_program, "m");
    GLint d_p_poly6_constant_loc = glGetUniformLocation(density_pressure_program, "poly6_constant");
    GLint d_p_target_density_loc = glGetUniformLocation(density_pressure_program, "target_density");
    GLint d_p_pressure_multiplier_loc = glGetUniformLocation(density_pressure_program, "pressure_multiplier");
    GLint d_p_cells_loc = glGetUniformLocation(density_pressure_program, "cells");
    GLint d_p_num_particles_loc = glGetUniformLocation(density_pressure_program, "num_particles");

    //force shader locations
    GLint force_gravity_loc = glGetUniformLocation(force_program, "gravity");
    GLint force_h_loc = glGetUniformLocation(force_program, "h");
    GLint force_h2_loc = glGetUniformLocation(force_program, "h2");
    GLint force_viscosity_laplacian_constant_loc = glGetUniformLocation(force_program, "viscosity_laplacian_constant");
    GLint force_spiky3_gradient_constant_loc = glGetUniformLocation(force_program, "spiky3_gradient_constant");
    GLint force_m_loc = glGetUniformLocation(force_program, "m");
    GLint force_coefficient_of_viscosity_loc = glGetUniformLocation(force_program, "coefficient_of_viscosity");
    GLint force_cell_size_loc = glGetUniformLocation(force_program, "cell_size");
    GLint force_cells_loc = glGetUniformLocation(force_program, "cells");
    GLint force_dimensions_loc = glGetUniformLocation(force_program, "dimensions");
    GLint force_num_particles_loc = glGetUniformLocation(force_program, "num_particles");

    //external force shader
    GLint external_mouse_pos_loc = glGetUniformLocation(external_force_program, "mouse_pos");
    GLint external_dist_threshold_loc = glGetUniformLocation(external_force_program, "dist_threshold");
    GLint external_attractor_multiplier_loc = glGetUniformLocation(external_force_program, "attractor_multiplier");
    GLint external_repeller_multiplier_loc = glGetUniformLocation(external_force_program, "repeller_multiplier");
    GLint external_click_loc = glGetUniformLocation(external_force_program, "click");
    GLint external_num_particles_loc = glGetUniformLocation(external_force_program, "num_particles");

    //update shader locations
    GLint update_dt_loc = glGetUniformLocation(update_program, "dt");
    GLint update_velocity_damping_loc = glGetUniformLocation(update_program, "velocity_damping");
    GLint update_collision_damping_loc = glGetUniformLocation(update_program, "collision_damping");
    GLint update_velocity_cap_loc = glGetUniformLocation(update_program, "velocity_cap");
    GLint update_dimensions_loc = glGetUniformLocation(update_program, "dimensions");
    GLint update_radius_loc = glGetUniformLocation(update_program, "radius");
    GLint update_num_particles = glGetUniformLocation(update_program, "num_particles");

    //vertex and fragment shader locations
    GLint draw_width_loc = glGetUniformLocation(render_program, "screen_width");
    GLint draw_height_loc = glGetUniformLocation(render_program, "screen_height");
    GLint draw_radius_loc = glGetUniformLocation(render_program, "particle_radius");
    GLint draw_target_density_loc = glGetUniformLocation(render_program, "target_density");

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
    glUseProgram(density_pressure_program);
    
    glUniform1f(d_p_cell_size_loc, cell_size);
    glUniform1f(d_p_h_loc, h);
    glUniform1f(d_p_h2_loc, h2);
    glUniform1f(d_p_m_loc, m);
    glUniform1f(d_p_poly6_constant_loc, poly6_constant);
    glUniform1f(d_p_target_density_loc, target_density);
    glUniform1f(d_p_pressure_multiplier_loc, pressure_multiplier);
    glUniform2i(d_p_cells_loc, NUM_CELLS_X, NUM_CELLS_Y);
    glUniform1i(d_p_num_particles_loc, N);

    //send constants to force shader
    glUseProgram(force_program);

    glUniform1f(force_gravity_loc, gravity);
    glUniform1f(force_h_loc, h);
    glUniform1f(force_h2_loc, h2);
    glUniform1f(force_viscosity_laplacian_constant_loc, viscosity_laplacian_constant);
    glUniform1f(force_spiky3_gradient_constant_loc, spiky3_gradient_constant);
    glUniform1f(force_m_loc, m);
    glUniform1f(force_coefficient_of_viscosity_loc, coefficient_of_viscosity);
    glUniform1f(force_cell_size_loc, cell_size);
    glUniform2i(force_cells_loc, NUM_CELLS_X, NUM_CELLS_Y);
    glUniform2i(force_dimensions_loc, WIDTH, HEIGHT);
    glUniform1i(force_num_particles_loc, N);

    //send data to external force program
    glUseProgram(external_force_program);

    glUniform1f(external_dist_threshold_loc, dist_threshold);
    glUniform1f(external_attractor_multiplier_loc, attractor_multiplier);
    glUniform1f(external_repeller_multiplier_loc, repeller_multiplier);
    glUniform1i(external_num_particles_loc, N);

    //send constants to update shader
    glUseProgram(update_program);

    glUniform1f(update_dt_loc, DT);
    glUniform1f(update_velocity_damping_loc, velocity_damping);
    glUniform1f(update_collision_damping_loc, collision_damping);
    glUniform1f(update_velocity_cap_loc, velocity_cap);
    glUniform2i(update_dimensions_loc, WIDTH, HEIGHT);
    glUniform1i(update_radius_loc, r);
    glUniform1i(update_num_particles, N);

    //send data to render program
    glUseProgram(render_program);

    glUniform1f(draw_width_loc, (float)(WIDTH));
    glUniform1f(draw_height_loc, (float)(HEIGHT));
    glUniform1f(draw_radius_loc, (float)(r));

    const double sim_step = 1.0 / 60.0;  
    const int substeps_per_step = 16;    
    double last_time = glfwGetTime();
    double accumulator = 0.0;

    while (!glfwWindowShouldClose(window))
    {
        double now = glfwGetTime();
        accumulator += now - last_time;
        last_time = now;

        if (accumulator > 0.25) accumulator = 0.25;
        while (accumulator >= sim_step)
        {
            for (int iterations = 0; iterations < substeps_per_step; iterations++)
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

                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

                //dispatch boundaries shader
                glUseProgram(boundaries_program);
                glDispatchCompute(num_groups, 1, 1);

                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

                //dispatch density and pressure shader
                glUseProgram(density_pressure_program);
                glDispatchCompute(num_groups, 1, 1);

                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

                //dispatch force shader
                glUseProgram(force_program);
                glDispatchCompute(num_groups, 1, 1);

                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

                //external force program
                int click_state = -1;

                if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) 
                {
                    click_state = 0; 
                } 
                else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) 
                {
                    click_state = 1; 
                }

                if (click_state != -1)
                {

                    double mouse_x, mouse_y;

                    glfwGetCursorPos(window, &mouse_x, &mouse_y);

                    float sim_mouse_x = (float)mouse_x;
                    float sim_mouse_y = (float)HEIGHT - (float)mouse_y;

                    glUseProgram(external_force_program);
                    glUniform2f(external_mouse_pos_loc, sim_mouse_x, sim_mouse_y);
                    glUniform1i(external_click_loc, click_state);

                    glDispatchCompute(num_groups, 1, 1);
                    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
                }

                //dispatch update and wall collision shader
                glUseProgram(update_program);
                glDispatchCompute(num_groups, 1, 1);

                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            }
            accumulator -= sim_step;
        }
        //drawing
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(render_program);

        glBindVertexArray(quad_vao);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, N);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();

    return 0;
}