@ctype mat4 Mat4
@ctype vec4 Vec4

@vs vs
uniform vs_params {
    mat4 mvp;
    mat4 model;
};

in vec3 position;
in vec2 uv;
in vec3 normal;

out vec2 fsuv;
out vec4 color;

void main() {
  vec3 lightdir = normalize(vec3(0.1, 1.0, 0.2));
  gl_Position = mvp * vec4(position, 1.0);
  vec3 dir = vec3(model * vec4(position + normal, 1.0)) - vec3(model * vec4(position, 1.0));
  float intensity = dot(dir, lightdir)*0.7;
  intensity = clamp(intensity, 0.05, 1.0);
  color = vec4(intensity, intensity, intensity, 1.0);
  fsuv = uv;
}
@end

@fs fs
uniform sampler2D tex;

in vec2 fsuv;
in vec4 color;
out vec4 frag_color;

void main() {
  frag_color = texture(tex, fsuv) * color;
}
@end

@program mesh vs fs

// ---------------------------------------------------- //

@vs overlay_vs
uniform overlay_vs_params {
    mat4 mvp;
};

in vec2 position;
in vec2 uv;
out vec2 fs_uv;

void main() {
  fs_uv = uv;
  gl_Position = mvp * vec4(position, -0.1, 1.0);
}
@end

@fs overlay_fs
uniform sampler2D tex;
out vec4 frag_color;
in vec2 fs_uv;

void main() {
  frag_color = texture(tex, fs_uv);
}
@end

@program overlay overlay_vs overlay_fs
