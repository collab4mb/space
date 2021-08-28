@ctype mat4 Mat4
@ctype vec4 Vec4

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

@vs force_field_vs
uniform vs_params {
  mat4 view_proj;
  mat4 model;
};

in vec3 position;
in vec2 uv;

out vec2 fs_uv;

void main() {
  gl_Position = view_proj * model * vec4(position, 1.0);
  fs_uv = uv;
}
@end

@fs force_field_fs
in vec2 fs_uv;

out vec4 frag_color;

uniform force_field_fs_params {
  vec2 stretch;
  float time; /* set to zero when something hits force field */
};

float hex(vec2 p) {
  p.x *= 0.57735*2.0;
  p.y += mod(floor(p.x), 2.0)*0.5;
  p = abs((mod(p, 1.0) - 0.5));
  return smoothstep(1.0, 0.1, abs(max(p.x*1.5 + p.y, p.y*2.0) - 1.0) / 0.38);
}
void main() {
  vec2 uv = fs_uv;
  float center = 1.0 - 2.0*abs(uv.x - 0.5);
  center = smoothstep(0.0, 1.0, center);
  uv *= stretch;

  float ttime = time * 3.6;
  float pt = fs_uv.x + sin(fs_uv.y*80.0)*(ttime+0.1)*0.1;
  float bang = abs(abs(0.5 - pt) - ttime*3.0) / 0.085;
  bang = smoothstep(1.0, 0.0, bang);
  uv.x += bang * 0.1;
  uv.y += sin(bang + uv.x) * 0.1;

  float d = (hex(uv * 2.0) + bang * 0.4) * center;
  frag_color = vec4(1, 0, 1, 1) * d * 0.7;
}
@end

@program force_field force_field_vs force_field_fs

// ---------------------------------------------------- //

@vs overlay_vs
uniform overlay_vs_params {
    vec4 color;
    mat4 mvp;
};

in vec2 position;
out vec4 fs_color;

void main() {
  fs_color = color;
  gl_Position = mvp * vec4(position, -0.1, 1.0);
}
@end

@fs overlay_fs
out vec4 frag_color;
in vec4 fs_color;

void main() {
  frag_color = fs_color;
}
@end

@program overlay overlay_vs overlay_fs
