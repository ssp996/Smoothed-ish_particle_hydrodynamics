#version 430 core

layout(location = 0) in vec2 quad_vertex;

layout(std430, binding = 0) readonly buffer PositionBuffer 
{
    vec2 positions[];
};

layout(std430, binding = 7) readonly buffer DensityBuffer 
{
    float densities[];
};

uniform float screen_width;
uniform float screen_height;
uniform float particle_radius;

out vec2 local_pos;
out float particle_density;

void main()
{
    vec2 particle_center = positions[gl_InstanceID];
    vec2 scaled_vertex = quad_vertex * (particle_radius * 2.0);
    vec2 world_pos = scaled_vertex + particle_center;

    float ndc_x = (world_pos.x / screen_width) * 2.0 - 1.0;
    float ndc_y = (world_pos.y / screen_height) * 2.0 - 1.0;

    gl_Position = vec4(ndc_x, ndc_y, 0.0, 1.0);

    local_pos = quad_vertex;
    particle_density = densities[gl_InstanceID];
}