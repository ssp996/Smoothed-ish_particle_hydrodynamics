#include "GL/glew.h"
#include "GLFW/glfw3.h"
#include "gltools.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>


#define WIDTH 800
#define HEIGHT 800
#define N 12000

typedef struct{
    float px, py;
    float vx, vy;
}Particle;

void initParticles(Particle *particles, int n, int spacing) {
    int side = (int)ceilf(sqrtf(n));
    float start_x = WIDTH  / 2.0f - (side - 1) * spacing / 2.0f;
    float start_y = HEIGHT / 2.0f - (side - 1) * spacing / 2.0f;
    int count = 0;
    for (int row = 0; row < side && count < n; row++) {
        for (int col = 0; col < side && count < n; col++) {
            particles[count].px = start_x + col * spacing;
            particles[count].py = start_y + row * spacing;
            particles[count].vx = 0.0f;
            particles[count].vy = 0.0f;
            count++;
        }
    }
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "hahah", NULL, NULL);
    glfwMakeContextCurrent(window);
    glewInit();

    Particle *particles= malloc (N * sizeof(Particle));
    initParticles(particles, N, 7);

    GLuint ssbo;
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, N * sizeof(Particle), particles, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
    free(particles);

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    char *csrc = readFile("sph_computeshader.comp");
    char *vsrc = readFile("particle.vert");
    char *fsrc = readFile("particle.frag");

    GLuint compute_program = createComputeShader(csrc);
    GLuint render_program = createProgram(vsrc, fsrc);

    free(csrc); free(vsrc); free(fsrc);

    GLint dt_loc = glGetUniformLocation(compute_program, "dt");

    glEnable(GL_PROGRAM_POINT_SIZE);

    while (!glfwWindowShouldClose(window))
    {
        glUseProgram(compute_program);
        glUniform1f(dt_loc, 0.001f);
        glDispatchCompute((N + 255)/256, 1, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(render_program);
        glDrawArrays(GL_POINTS, 0, N);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glDeleteBuffers(1, &ssbo);
    glDeleteVertexArrays(1, &vao);
    glfwTerminate();

    return 0;
}


