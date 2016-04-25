#include "common.h"

typedef struct{
  u16 * data;
  size_t size;
}octree;

void octree_delete(octree * oct){
  dealloc(oct->data);
  oct->data = NULL;
  oct->size = 0;
}

int calc_octree_size(octree oct){
  int items = 0;
  u16 header = oct.data[0];
  ASSERT(header <= 255);
  oct.data += 1;
  for(int i = 0; i < 8; i++){
    u16 data = *oct.data;
    int st = header & (1 << i);

    if(st){
      oct.data += 1;
      items += calc_octree_size(oct);
      oct.data += data;
    }else{
      oct.data += 1;
      if(data > 0)
	items += 1;
    }
  }
  return items;
}

typedef struct{
  vec3 position;
  vec3 direction;
  int x, y;
  u16 color;
  vec3 normal;
  float size;
}ray;

typedef struct {
  vec3 position;
  float radius;
}sphere_info;

int rnd = 0;
float sphere_distance(ray * r, sphere_info * s){
  float d = vec3_len(vec3_sub(r->position, s->position)) - s->radius;
  float y = sin(r->position.y * 200) * 20;
  //if(d < 0.05){
  r->color = 480 - y;// - (rand() % 50);
    //}
  return MIN(d, 0.9 - r->position.y);;
}

typedef struct{
  float (* f)(ray * r, void * fieldinfo);
  void * fieldinfo;
  u16 * data;
  int size;
  float min_size;
}octree_distance_field;

int octree_from_distance_field_it(ray * r, octree_distance_field * oct, int offset){
  ASSERT(offset< oct->size);
  
  float d = oct->f(r, oct->fieldinfo);
  if(d <= -r->size){// && r->size >= oct->min_size && r->size * 0.5 <= oct->min_size){
    oct->data[offset] = r->color;
    return 0;
  }
  if(d >= r->size || r->size <= oct->min_size){
    oct->data[offset] = 0;
    return 0;
  }

  oct->data = realloc(oct->data, (oct->size += 9) * sizeof(u16));

  int sub_size = 1;
  int o2 = 2;
  int header = 0;
  
  ray _r = *r;
  _r.size *= 0.5;
  for(int i = 0; i < 8; i++){
    vec3 v = vec3_scale(vec3_sub(vec3_new(i % 2, (i / 2) % 2, (i / 4) % 2), vec3_half), r->size);
    
    _r.position = vec3_add(r->position, v);
    int subtree_size = octree_from_distance_field_it(&_r, oct, offset + o2);

    if(subtree_size > 1){
      header |= (1 << i);
      ASSERT(offset + o2 < oct->size);
      ASSERT(subtree_size / 9 < 65000);
      oct->data[offset + o2] = subtree_size / 9;
    }
    
    sub_size += subtree_size + 1;
    o2 += subtree_size + 1;
  }
  ASSERT(header >= 0 && header <= 255);
  ASSERT(offset + 1 < oct->size);
  //logd("sub size: %i %i\n", sub_size, sub_size %9);
  oct->data[offset] = sub_size / 9;
  oct->data[offset + 1] = header;
  return sub_size;
}

octree octree_from_distance_field(float (* f)(ray * r, void * fieldinfo), void * fieldinfo){
  octree o = {.data = alloc0(10 * sizeof(u16)), .size = 10 };
  octree_distance_field odf = {.f = f, .fieldinfo = fieldinfo, .data = o.data, .size = 10, .min_size = 0.001};
  ray r0 = {.position = vec3_half, .direction = vec3_zero, .x = 0, .y = 0, .color = 0, .normal = vec3_zero, .size = 0.5};
  octree_from_distance_field_it(&r0, &odf, 0);
  o.data = odf.data;
  memmove(o.data, o.data + 1, odf.size * sizeof(o.data[0]));
  o.size = odf.size;
  return o;
  logd("\n\n");
  logd("::Printing data::\n");
  for(int i = 0; i < odf.size; i++){
    logd(" %5i", CLAMP(odf.data[i],0,65000));
    if(i % 10 == 9)
      logd("\n");
  }
  logd("\n\n");
  return o;
}

void trace_octree(octree oct, ray * r){
  u16 header = oct.data[0];
  vec3 mid = vec3_new1(0.5);
  vec3 side = vec3_zero;
  vec3 midd = vec3_sub(mid, r->position);
  vec3 sided = vec3_sub(side, r->position);
  vec3 side_o = vec3_div(sided, r->direction);
  vec3 midd_o = vec3_div(midd, r->direction);
  float ds[] = {midd_o.x, midd_o.y, midd_o.z, side_o.x, side_o.y, side_o.z};
  f32 ds3[] = {0, f32_infinity, f32_infinity, f32_infinity, f32_infinity, f32_infinity};
  for(u32 i = 0; i < array_count(ds); i++){
    f32 v = ds[i];
    if(v < 0 || v > f32_sqrt3)
      continue;
    if(v < ds3[array_count(ds3) - 1])
      ds3[array_count(ds3) - 1] = v;
    
    for(int i = array_count(ds3) - 1; i > 0; i--){
      if(ds3[i] < ds3[i - 1])
	SWAP(ds3[i], ds3[i - 1]);
    }
  }
  for(u32 i = 0; i < array_count(ds3); i++){
    float d = ds3[i];
    if(d < f32_infinity){
      vec3 p = vec3_add(r->position, vec3_scale(r->direction, d + 0.00001));
      if(p.x < 0.0 || p.y < 0.0 || p.z < 0.0 || p.x > 1.0 || p.y > 1.0 || p.z > 1.0)
	continue;
      vec3 cellv = vec3_gteq(p, vec3_half);

      int cell = (p.x > 0.5 ? 1 : 0) + (p.y > 0.5 ? 2 : 0) + (p.z > 0.5 ? 4 : 0) ;
      
      int offset = 1;
      for(int i = 0; i < cell; i++){
	int header_bit = header & (1 << i);
	auto dat = oct.data[offset];
	offset += (header_bit ? (dat * 9 + 1) : 1);
      }
      
      if((1 << cell) & header){
	octree noct = {.data = oct.data + offset + 1};
	vec3 position = vec3_sub(vec3_scale(p, 2.0f), cellv);
	vec3 ppos = r->position;
	r->position = position;
	trace_octree(noct, r);
	
	r->position = ppos;
      }else{
	r->color = oct.data[offset];
      }
      if(r->color != 0){
	return;
      }
    }else
      return;
  }
}

int main(){
  int width = 1024, height = 1024;   
  gl_window * win = gl_window_open(width, height);
  gl_window_make_current(win);
  
  for(float y = -3.5; y < 3.5; y+= 0.005){
    logd("Y: %f\n", y);
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);
    sphere_info s = {.position = vec3_new(sin(y * 20)*0.33 + 0.5 , cos(y * 5.0) * 0.3 + 0.5,cos(y * 20)*0.33 + 0.5), .radius = 0.2};
    octree oct2 = octree_from_distance_field((void *) sphere_distance, &s);


    octree oct = oct2;

    u8 img[width * height];
    memset(img, 0, sizeof(img));
    float left = -1, right = 2, bottom = -1, top = 2;
    float _w = (right - left) / (float)width, _h = (top - bottom) / (float)height;
    u64 t1 = timestamp();
    vec3 dir = vec3_normalize(vec3_new(0.5f, 0.5f + sin(y * 2) * 0.5, 1.0f));
    for(float y = bottom; y < top; y += _h){
      for(float x = left; x < right; x += _w){
	ray r = {
	  .position = vec3_new(x, y, -0.1f),
	  .direction = dir,
	  .color = 0,
	  .x = (int)((x - left) / _w),
	  .y = (int)((y - bottom) / _h)
	};
	trace_octree(oct, &r);
	int pt = (int)(r.x + r.y * width);
	img[pt] = r.color;
      }
    }
    u64 t2 = timestamp();
    /*char buf[100];
      sprintf(buf,"test_%f.png",y);*/
    logd("Rendering took %fs\n", ((float)(t2 - t1)) * 1e-6);
    /*write_png_file(buf, width, height, img, 1, 8);
    u16 lok = calc_octree_size(oct);
    logd("lok? %i\n", lok);*/
    logd("octree size: %iB\n", oct.size * sizeof(u16));
    octree_delete(&oct2);
    glDrawPixels(width, height, GL_LUMINANCE, GL_UNSIGNED_BYTE, img);
    gl_window_swap(win);
    //iron_sleep(100000);
  }
  gl_window_destroy(&win);
  return 0;
}
