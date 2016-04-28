#include "common.h"

#define yield ccyield
int offset = 5;

void ccwait(u64 us){
  u64 start = timestamp();
  while((timestamp() - start) < us)
    yield();
}

int cc_elem_get_type_id(const char * name){
  static char ** names;
  static int name_cnt = 0;
  for(int i = 0; i < name_cnt; i++){
    if(strcmp(name,names[i]) == 0){
      return i;
    }
  }
  char * name2 = fmtstr("%s", name);
  list_push2(names, name_cnt, name2);
  return name_cnt - 1;
}

#define CC_DEF(item, name) (item.header.type = cc_elem_get_type_id( name ));

typedef struct{
  int type;
}cc_elem_header;

typedef struct{
  cc_elem_header header;
  int width, height;
  char * title;
  int closed;
}ccwindow;

ccwindow ccwindow_new(int width, int height){
  ccwindow out = {0};
  CC_DEF(out, "window");
  out.width = width;
  out.height = height;
  return out;
}

typedef enum{
  ALIGN_CENTER,
  ALIGN_LEFT,
  ALIGN_RIGHT,
  ALIGN_STRETCH
}ccalignment_enum;

typedef struct{
  cc_elem_header header;
  ccalignment_enum alignment;
}ccalignment;

ccalignment ccalignment_new(ccalignment_enum v){
  ccalignment out = {.alignment = v};
  CC_DEF(out, "alignment");
  return out;
}

typedef struct{
  cc_elem_header header;
  int width, height;
  int clicked;
}ccbutton;

ccbutton ccbutton_new(int width, int height){
  ccbutton out = {.width = width, .height = height, .clicked = 0};
  CC_DEF(out, "button");
  return out;
}

typedef struct{
  cc_elem_header header;
  char * text;
}cctext;

cctext cctext_new(char * text){
  cctext out = {.text = text};
  CC_DEF(out, "text");
  return out;
}

typedef struct {
  u64 uid1;
  u64 uid2;
  
  int * items;
  int * item_super;
  int ** sub_items;
  int * sub_item_cnt;
  int item_cnt;
  
  int current_item;
}dynscope;

// guid: 
const u64 dynscope_uid1 = 0xce382dc7b01e4695;
const u64 dynscope_uid2 = 0x95a40d436af62135;

void load_gui(void (* gui_fcn)()){
  dynscope dyn = {
    .uid1 = dynscope_uid1,
    .uid2 = dynscope_uid2,
    .current_item = -1
    
  };
  UNUSED(dyn);
  //_inside_gui = true;
  //cc_logical_tree
  gui_fcn();
  //_inside_gui = false;
}

void cc_push_element(void * item){
  dynscope * scope = item - sizeof(u64);
  while(scope->uid1 != dynscope_uid1 && scope->uid2 != dynscope_uid2){
    void ** scope2 = (void **) &scope;
    (*scope2) += sizeof(u64);
  }
  int distance = cc_get_stack_base() - item;
  
  list_push(scope->items, scope->item_cnt, distance);
  int super = scope->current_item;
  list_push(scope->sub_items, scope->item_cnt, NULL);
  list_push(scope->sub_item_cnt, scope->item_cnt, 0);  
  list_push2(scope->item_super, scope->item_cnt, super);  
  scope->current_item = scope->item_cnt - 1;
  if(super != -1){
    list_push2(scope->sub_items[super], scope->sub_item_cnt[super], scope->current_item);
  }
  
  cc_elem_header * head = item;
  logd("header: %i\n", head->type);
}


void cc_pop_element(){
  dynscope * scope = alloc(0);
  dynscope * _scope = scope;
  scope = (void *) &scope;
  while(scope->uid1 != dynscope_uid1 && scope->uid2 != dynscope_uid2){
    void ** scope2 = (void **) &scope;
    (*scope2) += sizeof(u64);
  }
  scope->current_item = scope->item_super[scope->current_item];
  dealloc(_scope);
}

dynscope * find_scope(void * stack_top){
  while(true){
    dynscope * ds = stack_top;
    if(ds->uid1 == dynscope_uid1 && ds->uid2 == dynscope_uid2){
      return ds;
    }
    stack_top += sizeof(u8);
  }
  return NULL;
}

void cc_redraw(void * item){
  UNUSED(item);
}

#define await(cond) while(!cond){yield();}
void test_gui(){
  ccwindow win = ccwindow_new(300, 300);
  cc_push_element(&win);
  ccalignment align = ccalignment_new(ALIGN_CENTER);
  cc_push_element(&align);
  ccbutton btn = ccbutton_new(100, 20);
  cc_push_element(&btn);
  cctext txt = cctext_new((char *)"Hello");
  cc_push_element(&txt);
  cc_pop_element();
  cctext txt2 = cctext_new((char *)"Hello");
  cc_push_element(&txt2);
  while(true){
    await(btn.clicked || win.closed);
    if(win.closed)
      return;
    if(btn.clicked){
      txt.text = (char *) "OK!";
      cc_redraw(&txt);
    }
  }
}

void render_gui(){

}

void test_dispatch2(float * phase){
  //UNUSED(arg);
    size_t wut = 5, wut2 = 10 + offset, wut3 = 15;
    logd("X %i\n", wut);
    yield();
    while(true){
      *phase += 0.01;
      wut += 3;
      logd("X2 %i %i %i %p\n", wut2, wut, wut3, &wut2);
      yield();
      
      //test_dispatch2();
    }
  }

void test_dispatch3(float * phase){
  int height = 512, width = 512;
  gl_window * win = gl_window_open(width, height);
  size_t a = 1, b = 2;
  //float phase = 0.0f;
  size_t wut = 5, wut2 = 10 + offset, wut3 = 15;
  logd("X2 %i %i %i %p\n", wut2, wut, wut3, &wut2);
  //logd("P%p\n", phase + phase2);
  logd("%f\n", 1.0f);
  yield();

  while(true){
    a += 2; b += 4;
    gl_window_make_current(win);
    glClearColor(sin(*phase) * 0.5 + 0.5,cos(*phase * 0.9) * 0.5 + 0.5,cos(*phase) * 0.5 + 0.5,0);
    glClear(GL_COLOR_BUFFER_BIT);
    gl_window_swap(win);
    ccwait(500000);
    logd("Phase: %f %i %i\n", *phase, a, b);
  }
}

void * get_stk_item(dynscope * scope, int index){
  UNUSED(scope);
  UNUSED(index);
  return NULL;
}

void render_gui2(void * ctx){
  ccwindow * ccwin = get_stk_item(ctx, 0);
  if(ccwin == NULL)
    return;
  if(ccwin->header.type != cc_elem_get_type_id("window"))
    ERROR("First element must be a window");
  gl_window * win = gl_window_open(ccwin->width, ccwin->height);
  float phase = 0.0;
  while(true){

    glClearColor(sin(phase) * 0.5 + 0.5, cos(phase * 0.9) * 0.5 + 0.5, cos(phase) * 0.5 + 0.5, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    /*int cnt = get_sub_cnt(ctx);
    for(int i = 0; i < cnt; i++){
      
    }*/
    gl_window_swap(win);
    yield();
    phase += 0.1;
  }
}

void ccgui_test(){

  void _test_dispatch4(){
    load_gui(test_gui);
  }
  coroutine * c4 = ccstart(_test_dispatch4);
  void * stk;
  size_t stk_size;
  cc_get_stack(c4, &stk, &stk_size);
  logd("stack size: %i\n", stk_size);
  dynscope * scope = find_scope(stk);
  logd("scope items: %i\n", scope->item_cnt);
  for(int i = 0; i < scope->item_cnt; i++){
    logd("offset: %i %i %i\n", scope->items[i], scope->item_super[i], scope->sub_item_cnt[i]);
    cc_elem_header * head = stk + (stk_size - scope->items[i]);
    logd("item type: %i\n", head->type);
    if(head->type == cc_elem_get_type_id("text")){
      cctext * txt = (void *) head;
      logd("Text: %s\n", txt->text);
    }
  }
  ccstep(c4);
  
  return;
  float * phase = alloc0(sizeof(float));
  void _test_dispatch2(){
    test_dispatch2(phase);
  }
  void _test_dispatch3(){
    test_dispatch3(phase);
  }
  //char sp[1000];

  
  //coroutine * c = ccstart(test_dispatch2);
  offset += 3;
  coroutine * c2 = ccstart(_test_dispatch2);
  coroutine * c3 = ccstart(_test_dispatch3);
  //UNUSED(c2);
  UNUSED(c3);
  //while(true)
  for(int i = 0; i < 50000; i++){
    logd("i: %i\n", i);
    //ccstep(c);
    ccstep(c2);
    ccstep(c3);
    iron_sleep(0.01);
  }
  
}
