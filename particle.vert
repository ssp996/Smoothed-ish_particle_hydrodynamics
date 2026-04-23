#version 430 core

struct Particle{
    vec2 position;
    vec2 velocity;
};

layout(std430, binding = 0) buffer ParticleBuffer {
    Particle particles[];
};

void main()
{
    vec2 pos = particles[gl_VertexID].position;

    vec2 ndc = (pos/vec2(800.0, 800.0)) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    gl_Position = vec4(ndc, 0.0, 1.0);
    gl_PointSize = 6.0;
}