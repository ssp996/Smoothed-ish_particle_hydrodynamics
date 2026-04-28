#include "GL/glew.h"
#include "GLFW/glfw3.h"
#include "gltools.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

void read_ssbo(GLuint ssbo, unsigned int *arr, int n)
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    unsigned int *ptr = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
    for (int i = 0; i < n; i++)
        arr[i] = ptr[i];
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);   
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
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(640, 480, "Compute", NULL, NULL);
    glfwMakeContextCurrent(window);
    glewInit();

    char *csrc = readFile("testcs1.comp");
    GLuint compute_shader = createComputeShader(csrc);

    GLuint threads_per_workgroup = 32;
    GLuint workgroups = 4;

    GLuint num_elements = threads_per_workgroup * workgroups;

    unsigned int *input = malloc(num_elements * sizeof(unsigned int));
    unsigned int *output = malloc(num_elements * sizeof(unsigned int));

    for (int i = 0; i < num_elements; i++)
        input[i] = i;

    GLuint ssbo_in, ssbo_out, ssbo_lid, ssbo_gid, ssbo_wid;

    glGenBuffers(1, &ssbo_in);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_in);
    glBufferData(GL_SHADER_STORAGE_BUFFER, num_elements * sizeof(GLuint), input, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_in);

    glGenBuffers(1, &ssbo_out);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_out);
    glBufferData(GL_SHADER_STORAGE_BUFFER, num_elements * sizeof(GLuint), NULL, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, ssbo_out);

    glGenBuffers(1, &ssbo_lid);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_lid);
    glBufferData(GL_SHADER_STORAGE_BUFFER, num_elements * sizeof(GLuint), NULL, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo_lid);

    glGenBuffers(1, &ssbo_gid);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_gid);
    glBufferData(GL_SHADER_STORAGE_BUFFER, num_elements * sizeof(GLuint), NULL, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssbo_gid);

    glGenBuffers(1, &ssbo_wid);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_wid);
    glBufferData(GL_SHADER_STORAGE_BUFFER, num_elements * sizeof(GLuint), NULL, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssbo_wid);

    glUseProgram(compute_shader);

    glUniform1i(glGetUniformLocation(compute_shader, "factor"), 2);
    glUniform1ui(glGetUniformLocation(compute_shader, "num_elements"), num_elements);

    glDispatchCompute(workgroups, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    unsigned int *out_data = malloc(num_elements * sizeof(unsigned int));
    read_ssbo(ssbo_out, out_data, num_elements);
    print_array(out_data, num_elements);
    printf("\n");

    read_ssbo(ssbo_lid, out_data, num_elements);
    print_array(out_data, num_elements);
    printf("\n");

    read_ssbo(ssbo_gid, out_data, num_elements);
    print_array(out_data, num_elements);
    printf("\n");

    read_ssbo(ssbo_wid, out_data, num_elements);
    print_array(out_data, num_elements);
    printf("\n");
    
    return 0;
}