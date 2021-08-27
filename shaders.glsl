@ctype mat4 Mat4
@ctype vec4 Vec4
@ctype vec2 Vec2

@vs vs
uniform vs_params {
    mat4 view_proj;
    mat4 model;
};

in vec3 position;
in vec2 uv;
in vec3 normal;

out vec3 light;
out vec2 fs_uv;

void main() {
  gl_Position = view_proj * model * vec4(position, 1.0);
  fs_uv = uv;

  // vec3 frag_pos = vec3(model * vec4(position, 1));
  vec3 fs_normal = mat3(transpose(inverse(model))) * normal;

  vec3 light_dir = -normalize(vec3(0.1, -1.0, 0.3));
  vec3 light_color = vec3(1.0);

  // ambient
  float ambient_strength = 0.1;
  vec3 ambient = ambient_strength * light_color;

  // diffuse 
  vec3 norm = normalize(fs_normal);
  float diff = max(dot(norm, light_dir), 0.0);
  vec3 diffuse = diff * light_color;

  // specular
  // float specular_strength = 0.5;
  // vec3 view_dir = normalize(viewPos - frag_pos);
  // vec3 reflect_dir = reflect(-light_dir, norm);
  // float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
  // vec3 specular = specular_strength * spec * light_color;
  float specular = 0.0;

  light = ambient + diffuse + specular;
}
@end

@fs fs
uniform sampler2D tex;

in vec3 light;
in vec2 fs_uv;

out vec4 frag_color;

void main() {
  vec3 object_color = vec3(texture(tex, fs_uv));
  frag_color = vec4(object_color * light, 1);
}
@end

@program mesh vs fs

// ---------------------------------------------------- //

@vs overlay_vs
uniform overlay_vs_params {
  float istxt;
  vec2 minuv;
  vec2 sizuv;
  mat4 mvp;
  vec4 modulate;
};

in vec2 position;
in vec2 uv;
out vec2 fs_uv;
out vec4 fs_modulate;
out float fs_istxt;

void main() {
  fs_istxt = istxt;
  fs_modulate = modulate;
  fs_uv = uv*sizuv+minuv;
  gl_Position = mvp * vec4(position, -0.1, 1.0);
}
@end

@fs overlay_fs
uniform sampler2D tex;
out vec4 frag_color;
in vec2 fs_uv;
in vec4 fs_modulate;
in float fs_istxt;

void main() {
  vec4 color = texture(tex, fs_uv);
  if (fs_istxt > 0.5) {
    color = vec4(color.r, color.r, color.r, color.r);
  }
  frag_color = color*fs_modulate;
}
@end

@program overlay overlay_vs overlay_fs
