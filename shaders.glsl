@ctype mat4 Mat4

@vs vs
uniform vs_params {
    mat4 mvp;
};

in vec3 position;
in vec2 uv;
in vec3 normal;

out vec4 color;

void main() {
  gl_Position = mvp * vec4(position, 1.0);
  color = vec4(normal, 1.0);
  color.r += uv.x*0.001;
}
@end

@fs fs
in vec4 color;
out vec4 frag_color;

void main() {
    frag_color = color;
}
@end

@program cube vs fs

