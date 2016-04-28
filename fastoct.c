#include "common.h"

void octree16_delete(octree16 * oct){
  dealloc(oct->data);
  oct->data = NULL;
  oct->size = 0;
}

int calc_octree16_size(octree16 oct){
  int items = 0;
  u16 header = oct.data[0];
  ASSERT(header <= 255);
  oct.data += 1;
  for(int i = 0; i < 8; i++){
    u16 data = *oct.data;
    int st = header & (1 << i);

    if(st){
      oct.data += 1;
      items += calc_octree16_size(oct);
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
  float (* f)(ray * r, void * fieldinfo);
  void * fieldinfo;
  u16 * data;
  int size;
  float min_size;
}octree16_distance_field;

int octree16_from_distance_field_it(ray * r, octree16_distance_field * oct, int offset){
  ASSERT(offset< oct->size);
  
  float d = oct->f(r, oct->fieldinfo);
  if(d <= -r->size || r->size * 0.501 <= oct->min_size){
    oct->data[offset] = r->color;
    return 0;
  }
  if(d >= r->size ){
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
    int subtree_size = octree16_from_distance_field_it(&_r, oct, offset + o2);

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
  oct->data[offset] = sub_size / 9;
  oct->data[offset + 1] = header;
  return sub_size;
}

octree16 octree16_from_distance_field(float (* f)(ray * r, void * fieldinfo), void * fieldinfo){
  octree16_distance_field odf = {
    .f = f,
    .fieldinfo = fieldinfo,
    .data = alloc0(10 * sizeof(u16)),
    .size = 10,
    .min_size = 0.005
  };

  ray r0 = {
    .position = vec3_half, .direction = vec3_zero,
    .x = 0, .y = 0, .color = 0, .normal = vec3_zero, .size = 0.5};
  
  octree16_from_distance_field_it(&r0, &odf, 0);
  
  memmove(odf.data, odf.data + 1, odf.size * sizeof(odf.data[0]));
  
  return (octree16){.data = odf.data, .size = odf.size};
}

void trace_octree16(octree16 oct, ray * r){
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
	octree16 noct = {.data = oct.data + offset + 1};
	vec3 position = vec3_sub(vec3_scale(p, 2.0f), cellv);
	vec3 ppos = r->position;
	r->position = position;
	trace_octree16(noct, r);
	
	r->position = ppos;
      }else{
	r->color = oct.data[offset];
	r->color_ptr = &oct.data[offset];
      }
      if(r->color != 0){
	return;
      }
    }else
      return;
  }
}

void octree_delete(octree_ptr * oct){
  for(int i = 0; i < 8; i++){
    int mask = 1 << i;
    if(oct->subs & mask){
      octree_delete(oct->nodes[i].sub_node);
      dealloc(oct->nodes[i].sub_node);
      oct->nodes[i].sub_node = NULL;
    }else if(oct->compressed & mask){
      octree16_delete(&oct->nodes[i].compressed_tree);
    }else{
      oct->nodes[i].color = 0;
    }
  }  
}

int calc_octree_size(octree_ptr oct){
  int subsize = 0;
  for(int i = 0; i < 8; i++){
    int mask = 1 << i;
    if(mask & oct.subs)
      subsize += calc_octree_size(*oct.nodes[i].sub_node);
    else if(mask & oct.compressed)
      subsize += calc_octree16_size(oct.nodes[i].compressed_tree);
    else if(oct.nodes[i].color)
      subsize += 1;
  }
  return subsize;
}

void trace_octree(octree_ptr oct, ray * r){
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
      int mask = 1 << cell;
      if(false == ((oct.compressed | oct.subs) & mask)){
	u16 color = oct.nodes[cell].color;
	r->color = color;
	r->color_ptr = &oct.nodes[cell].color;
	
      }else{
	vec3 position = vec3_sub(vec3_scale(p, 2.0f), cellv);
	vec3 ppos = r->position;
	r->position = position;
      
	if(oct.compressed & mask){
	  trace_octree16(oct.nodes[cell].compressed_tree, r);
	}else{
	  trace_octree(*oct.nodes[cell].sub_node, r);
	}
	r->position = ppos;
      }
      if(r->color)
	return;
    }
    else return;
  }
}


// Testing
typedef struct {
  vec3 position;
  float radius;
}sphere_info;

float sphere_distance(ray * r, sphere_info * s){
  float d = vec3_len(vec3_sub(r->position, s->position)) - s->radius;
  float y = sin(r->position.y * 200) * 20;
  r->color = 480 - y;
  return d;//MIN(d, 0.9 - r->position.y);;
}


bool fastoct_test(){
  int width = 256, height = 256;   
  u8 img[width * height];
  char buffer[1000];
  memset(buffer, 0, sizeof(buffer));
  memset(img, 0, sizeof(img));
  gl_window * win = gl_window_open(width, height);
  gl_window_make_current(win);
  float y = 0.0;
  sphere_info s =
    {
      .position = vec3_new(sin(y * 20) * 0.33 + 0.5,
			   cos(y * 5.0) * 0.3 + 0.5,
			   cos(y * 20)*0.33 + 0.5),
      .radius = 0.2
    };
  octree_ptr oct = {.compressed = 1};
  oct.nodes[0].compressed_tree = octree16_from_distance_field((void *) sphere_distance, &s);
  vec3 speed_v = vec3_new(0, 0, 0);
  vec3 cam_position = vec3_new(0,0,2);
  vec3 spin = vec3_new(0, 0, 0);
  quat q = quat_identity();
  u16 color = 0;
  
  while(true){
    q = quat_mul(quat_from_axis(vec3_new(0,1,0), spin.x * 0.1), q);
    q = quat_mul(quat_from_axis(vec3_new(1,0,0), spin.y * 0.1), q);
    vec3 s = quat_mul_vec3(q, vec3_scale(speed_v, 0.1));
    cam_position = vec3_add(s, cam_position);
    mat4 m = mat4_perspective(1.0, 1.0, 1, 100.0);
    mat4 w = mat4_from_quat(q);
    
    float left = -1, right = 1, bottom = -1, top = 1;
    float _w = (right - left) / (float)width, _h = (top - bottom) / (float)height;
    u64 t1 = timestamp();
    mat4 m2 = mat4_mul(w, m);
    for(float y = MIN(top, bottom); y < MAX(top, bottom); y += ABS(_h)){
      for(float x = left; x < right; x += _w){
	vec3 screen_pt = vec3_new(x, y, 1.0);
	vec3 v = mat4_mul_vec3(m2, screen_pt);
	
	ray r = {
	  .position = cam_position,
	  .direction = vec3_normalize(v),
	  .color = 0,
	  .x = (int)((x - left) / _w),
	  .y = (int)((y - bottom) / _h)
	};
	trace_octree(oct, &r);
	int pt = (int)(r.x + (r.y - 1) * width);
	img[pt] = r.color;
      }
    }
    u64 t2 = timestamp();
    if(false)
      logd("Rendering took %fs\n", ((float)(t2 - t1)) * 1e-6);
    
    glDrawPixels(width, height, GL_LUMINANCE, GL_UNSIGNED_BYTE, img);
    gl_window_swap(win);
    gl_window_event evts[10];
    size_t evt_cnt = gl_get_events(evts, array_count(evts));
    if(evt_cnt > 0){
      //logd("Event count: %i\n", evt_cnt, buffer);
      for(size_t i = 0; i< evt_cnt; i++){
	
	gl_window_event * evt = evts + i;
	if(evt->type == EVT_WINDOW_CLOSE){
	  goto end;
	}
	if(evt->type == EVT_KEY_DOWN || evt->type == EVT_KEY_UP){
	  float d = evt->type == EVT_KEY_DOWN ? -1.0f : 1.0f;
	  evt_key * key = (void *) evt;
	  if(!key->ischar){
	    if(key->key == KEY_RIGHT){
	      spin.x += d;
	    }
	    if(key->key == KEY_LEFT){
	      spin.x -= d;
	    }
	    if(key->key == KEY_UP){
	      spin.y += d;
	    }
	    if(key->key == KEY_DOWN){
	      spin.y -= d;
	    }
	    if(key->key == 'W'){
	      speed_v.z += d;
	    }
	    if(key->key == 'S'){
	      speed_v.z -= d;
	    }
	    if(key->key == 'A'){
	      speed_v.x += d;
	    }
	    if(key->key == 'D'){
	      speed_v.x -= d;
	    }
	  }
	}
	if(evt->type == EVT_MOUSE_BTN_DOWN){
	  evt_mouse_btn * btnevt = (void *)evt;
	  logd("button: %i\n", btnevt->button);
	  int _x, _y;
	  float x, y;
	  {
	    get_mouse_position(win, &_x, &_y);
	    _y = height - _y;
	    x = (float)_x;
	    y = (float)_y;
	    x = x *_w + left;
	    y = y *_h + bottom;
	  }
	  logd("Pushed: %f %f\n", x, y);
	  vec3 screen_pt = vec3_new(x, y, 1);
	  vec3 v = mat4_mul_vec3(m2, screen_pt);
	  ray r = {
	    .position = cam_position,
	    .direction = vec3_normalize(v),
	    .color = 0,
	    .x = _x,
	    .y = _y
	  };
	  trace_octree(oct, &r);
	  logd("Color: %i\n", color);
	  if(btnevt->button == 1){
	    color = r.color;
	  }else if(r.color_ptr != NULL){
	    *r.color_ptr = color;
	  }
	  
	}
      }
    } 
  }
 end:
  octree_delete(&oct);
  gl_window_destroy(&win);
  return TEST_SUCCESS;
}
