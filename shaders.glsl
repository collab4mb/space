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
uniform mesh_fs_params {
  float bloom;
  float transparency;
};

in vec3 light;
in vec2 fs_uv;

layout (location = 0) out vec4 frag_color;
layout (location = 1) out vec4 bright_color;

void main() {
  vec3 object_color = vec3(texture(tex, fs_uv));
  frag_color = vec4(object_color * light, 1) * transparency;

  float brightness = dot(frag_color.rgb, vec3(0.2126, 0.7152, 0.0722));
  bright_color = vec4(step(brightness, bloom) * frag_color.rgb, 1);
}
@end

@program mesh vs fs

// ---------------------------------------------------- //

@vs laser_vs
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

@fs laser_fs
uniform sampler2D tex;
uniform mesh_fs_params {
  float bloom;
  float transparency;
};

in vec2 fs_uv;

layout (location = 0) out vec4 frag_color;
layout (location = 1) out vec4 bright_color;

void main() {
  vec3 object_color = vec3(texture(tex, fs_uv));
  frag_color = vec4(object_color, 1);

  float brightness = dot(frag_color.rgb, vec3(0.2126, 0.7152, 0.0722));
  bright_color = vec4(step(brightness, 1.0) * frag_color.rgb, 1);
}
@end

@program laser laser_vs laser_fs

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

layout (location = 0) out vec4 frag_color;
layout (location = 1) out vec4 bright_color;

uniform force_field_fs_params {
  float transparency;
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

  vec2 muv = vec2(abs(uv.x * 2.0 - 1.0), uv.y);
  float hole = min(1, length(muv - vec2(0.75, 0.5)) / 0.7);
  hole = 1.0 - smoothstep(1.0, 0.1, hole);

  uv *= stretch;

  /* fade on last 0.2 units of vertical sides */
  float hastretchy = stretch.y / 2.0;
  float scenter = abs(uv.y - hastretchy) - hastretchy + 0.3;
  scenter = min(1.0, 1.0 - (scenter / 0.3));

  /* fade on last 0.2 units of vertical sides */
  float icenter = abs(0.2 - max(0, abs(uv.y - hastretchy) - hastretchy + 0.6));
  icenter = 1.0 - min(1.0, (icenter / 0.2));

  float ttime = time * 3.6;
  float pt = fs_uv.x + sin(fs_uv.y*80.0)*(ttime+0.1)*0.1;
  float bang = abs(abs(0.5 - pt) - ttime*3.0) / 0.085;
  bang = smoothstep(1.0, 0.0, bang);
  uv.x += bang * 0.1;
  uv.y += sin(bang + uv.x) * 0.1;

  float d = (hex((uv + vec2(time, time)) * 3.0) + bang * 0.4);
  d *= center + icenter * 0.8;
  frag_color = hole * scenter * vec4(1, 0, 1, 1) * d;

  float brightness = dot(frag_color.rgb, vec3(0.2126, 0.7152, 0.0722));
  if (brightness < 0.04) discard;
  bright_color = vec4(step(brightness, 0.04) * frag_color.rgb, 1);
}
@end

@program force_field force_field_vs force_field_fs

// ---------------------------------------------------- //

@vs healthbar_vs
in vec2 pos;
in vec2 uv;
in float hp;
in float fancy_shape;

uniform healthbar_vs_params {
  vec2 resolution;
};

out vec2 fs_uv;
out float fs_hp;
out float fs_fancy_shape;

void main() {
  vec2 screen_pos = pos*2.0/resolution;
  screen_pos = vec2(screen_pos.x - 1.0, 1.0 - screen_pos.y);
  gl_Position = vec4(screen_pos, 0.01, 1.0);
  fs_uv = uv;
  fs_hp = hp;
  fs_fancy_shape = fancy_shape;
}
@end

@fs healthbar_fs
uniform healthbar_fs_params {
  float time;
};

in vec2 fs_uv;
in float fs_hp;
in float fs_fancy_shape;

layout (location = 0) out vec4 frag_color;
layout (location = 1) out vec4 bright_color;

/* source: https://iquilezles.org/www/articles/distfunctions2d/distfunctions2d.htm */
float sdPolygon(in vec2 p, in vec2[6] v) {
  const int num = v.length();
  float d = dot(p-v[0],p-v[0]);
  float s = 1.0;
  for( int i=0, j=num-1; i<num; j=i, i++ ) {
    // distance
    vec2 e = v[j] - v[i];
    vec2 w =    p - v[i];
    vec2 b = w - e*clamp( dot(w,e)/dot(e,e), 0.0, 1.0 );
    d = min( d, dot(b,b) );

    // winding number from http://geomalgorithms.com/a03-_inclusion.html
    bvec3 cond = bvec3( p.y>=v[i].y, 
        p.y <v[j].y, 
        e.x*w.y>e.y*w.x );
    if( all(cond) || all(not(cond)) ) s=-s;  
  }

  return s*sqrt(d);
}

float hex(vec2 p, float s) {
  p.x *= 0.57735*2.0;
  p.y += mod(floor(p.x), 2.0)*0.5;
  p = abs((mod(p, 1.0) - 0.5));
  return smoothstep(1.0, 0.1, abs(max(p.x*1.5 + p.y, p.y*2.0) - 1.0) / (0.38 + s));
}

float inv_lerp(float from, float to, float value){
  return max(0.0, min(1.0, (value - from) / (to - from)));
}

void main() {
  vec2 uv = fs_uv;
  float hp = fs_hp;

  vec2[6] polygon;
  if (fs_fancy_shape > 0.0f) {
    polygon[0] = vec2(0.97, 0.776);
    polygon[1] = vec2(0.13, 0.776);
    polygon[2] = vec2(0.03, 0.256);
    polygon[3] = vec2(0.68, 0.256);
    polygon[4] = vec2(0.72, 0.024);
    polygon[5] = vec2(0.97, 0.024);
  } else {
    polygon[0] = vec2(0.87, 0.776);
    polygon[1] = vec2(0.13, 0.776);
    polygon[2] = vec2(0.03, 0.400);
    polygon[3] = vec2(0.13, 0.024);
    polygon[4] = vec2(0.87, 0.024);
    polygon[5] = vec2(0.97, 0.400);
  }

  for (int i = 0; i < polygon.length(); i++)
    polygon[i] = (polygon[i] + vec2(0, 0.1)) / vec2(1, 5);

  float d = sdPolygon(uv, polygon);

  float cd = 0.7*(1.0-smoothstep(0.01,0.02, abs(d))) + (1.0-smoothstep(-0.03,0.03, abs(0.02 - d)));
  // cd = step(0.4, cd)*cd;
  vec3 c = vec3(cd) * vec3(0.575, 0.5, 0.7);
  float lowhp = inv_lerp(0.3, 0.27, hp);
  float h = hex(uv * 14.0, lowhp * 0.2 * abs(sin(7.0*time)));
  float hpx = (1.15 * (1.0 - uv.x)) - 0.08;
  vec3 cbase = mix(vec3(1.0, 0.0, 1.0-0.8*lowhp), vec3(0.4, 0.2, 0.4), inv_lerp(hp-0.03, hp, hpx));
  c += float(!(c.x > 0.0)) * max(0.0, -d)/0.06 * cbase * h;

  frag_color = vec4(vec3(c*0.7), 1.0) * inv_lerp(0.025, 0.015, d);
  bright_color = vec4(0.0, 0.0, 0.0, 1.0);
}
@end

@program healthbar healthbar_vs healthbar_fs

// ---------------------------------------------------- //

@vs overlay_vs
uniform overlay_vs_params {
  float istxt;
  vec2 minuv;
  vec2 sizuv;
  vec2 pos;
  vec2 size;
  vec2 resolution;
  vec4 modulate;
};

in vec2 vert_pos;
in vec2 uv;
out vec2 fs_uv;
out vec4 fs_modulate;
out float fs_istxt;

void main() {
  fs_istxt = istxt;
  fs_modulate = modulate;
  fs_uv = minuv + uv * sizuv;
  vec2 screen_pos = pos + vert_pos * size;
  screen_pos = screen_pos*2.0/resolution;
  screen_pos = vec2(screen_pos.x - 1.0, 1.0 - screen_pos.y);
  gl_Position = vec4(screen_pos, -0.1, 1);
}
@end

@fs overlay_fs
uniform sampler2D tex;
layout (location = 0) out vec4 frag_color;
layout (location = 1) out vec4 bright_color;
in vec2 fs_uv;
in vec4 fs_modulate;
in float fs_istxt;

void main() {
  vec4 color = texture(tex, fs_uv);
  if (fs_istxt > 0.5) {
    color = vec4(color.r, color.r, color.r, color.r);
  }
  frag_color = color*fs_modulate;

  float brightness = dot(frag_color.rgb, vec3(0.2126, 0.7152, 0.0722));
  bright_color = vec4(step(brightness, 0.98) * brightness * frag_color.rgb * 0.55, 1);
}
@end

@program overlay overlay_vs overlay_fs

@vs fsq_vs

in vec2 pos;

out vec2 uv;

void main() {
    gl_Position = vec4(pos*2.0-1.0, 0.5, 1.0);
    uv = pos;
}
@end

@fs fsq_fs
uniform sampler2D tex;
uniform sampler2D blur0;
uniform sampler2D blur1;
uniform sampler2D blur2;
uniform sampler2D blur3;

in vec2 uv;

out vec4 frag_color;

void main() {
  vec3 t = texture(  tex, uv).rgb +
           texture(blur0, uv).rgb +
           texture(blur1, uv).rgb +
           texture(blur2, uv).rgb +
           texture(blur3, uv).rgb ;
  frag_color = vec4(t, 1);
}
@end
@program fsq fsq_vs fsq_fs

@vs blur_vs
@glsl_options flip_vert_y

in vec2 pos;

out vec2 uv;

void main() {
    gl_Position = vec4(pos*2.0-1.0, 0.5, 1.0);
    uv = pos;
}
@end

@fs blur_fs
in vec2 uv;
out vec4 frag_color;

uniform sampler2D tex;
uniform blur_fs_params {
  vec2 hori;
};
const float weight[] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };

void main() {
  vec2 tex_offset = 1.0 / textureSize(tex, 0);
  vec3 result = texture(tex, uv).rgb * weight[0]; // current fragment's contribution
  for (int i = 1; i < 5; ++i) {
    result += texture(tex, uv + hori * tex_offset * i).rgb * weight[i];
    result += texture(tex, uv - hori * tex_offset * i).rgb * weight[i];
  }
  frag_color = vec4(result, 1.0);
}
@end
@program blur blur_vs blur_fs
