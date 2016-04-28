
typedef struct{
  // current position of ray
  vec3 position;

  // direction of ray
  vec3 direction;

  // x and y in image.
  int x, y;

  // color of ray hit.
  u16 color;

  // normal of ray hit.
  vec3 normal;

  // current ray size
  float size;
  
  // pointer to color hit.
  u16 * color_ptr;
}ray;

// octree16: Compressed octree data structure.
  
typedef struct{
  u16 * data;
  size_t size;
}octree16;

void octree16_delete(octree16 * oct);
int calc_octree16_size(octree16 oct);
void trace_octree16(octree16 oct, ray * r);
octree16 octree16_from_distance_field(float (* f)(ray * r, void * fieldinfo), void * fieldinfo);

// ## Uncompressed / large octree data structure.
struct _octree_ptr;
typedef struct _octree_ptr octree_ptr;
struct _octree_ptr{
  u8 subs;
  u8 compressed;
  union{
    octree_ptr * sub_node;
    u16 color;
    octree16 compressed_tree;
  }nodes[8];
};

void trace_octree(octree_ptr oct, ray * r);
int calc_octree_size(octree_ptr oct);
void octree_delete(octree_ptr * oct);

// testing
bool fastoct_test();
