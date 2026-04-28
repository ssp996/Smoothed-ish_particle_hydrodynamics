#include "GL/glew.h"
#include "GLFW/glfw3.h"
#include "gltools.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h> 
#include <math.h>

#define GSSB GL_SHADER_STORAGE_BUFFER
#define GLDC GL_DYNAMIC_COPY



void read_ssbo(GLuint ssbo, int *arr, int n)
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
  
    int *ptr = (int*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
  
    if (ptr != NULL) 
    {
        memcpy(arr, ptr, n * sizeof(int));
        
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);   
    } 
    else 
    {
        printf("ERROR: Failed to map SSBO!\n");
    }
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
    struct timespec start, end;
    glfwInit();

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); 
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); 
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    

    GLFWwindow* window = glfwCreateWindow(640, 480, "Compute Shader", NULL, NULL);
    glfwMakeContextCurrent(window);

    glewInit();

    char *csrc = readFile("parallel_bitonic.comp");

    GLuint bitonic_sort = createComputeShader(csrc);

    int n = pow(2, 18);
    int *indices = malloc(n * sizeof(int));
    int *cell_hashes = malloc(n * sizeof(int));

    for (int i = 0; i < n; i++)
    {
        indices[i] = i;
        cell_hashes[i] = rand() % 64;
    }

    GLuint ssbo, index_ssbo;

    glGenBuffers(1, &ssbo);
    glBindBuffer(GSSB, ssbo);
    glBufferData(GSSB, n * sizeof(int), cell_hashes, GLDC);
    glBindBufferBase(GSSB, 0, ssbo);

    glGenBuffers(1, &index_ssbo);
    glBindBuffer(GSSB, index_ssbo);
    glBufferData(GSSB, n * sizeof(int), indices, GLDC);
    glBindBufferBase(GSSB, 1, index_ssbo);

    glUseProgram(bitonic_sort);

    GLint k_loc = glGetUniformLocation(bitonic_sort, "k");
    GLint j_loc = glGetUniformLocation(bitonic_sort, "j");

    GLuint num_work_groups = n/256;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int k = 2; k <= n; k *= 2)
    {
        for (int j = k / 2; j > 0; j /= 2)
        {
            glUniform1i(k_loc, k);
            glUniform1i(j_loc, j);

            glDispatchCompute(num_work_groups, 1, 1);

            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    double time_taken = (end.tv_sec - start.tv_sec) + 
                        (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Time: %f seconds\n", time_taken);
    printf("done\n");
    glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);

    int *out_indices = malloc(n * sizeof(int));
    int *out_hashes = malloc(n * sizeof(int));

    read_ssbo(ssbo, out_hashes, n);
    read_ssbo(index_ssbo, out_indices, n);

    //print_array(out_hashes, n);
    //print_array(out_indices, n);

    return 0;
}