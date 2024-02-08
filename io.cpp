#include <unistd.h>
#include <ncurses.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <math.h>


#include "io.h"
#include "poke327.h"
#include "pokemon.h"
#include "db_parse.h"

typedef struct io_message {
  /* Will print " --more-- " at end of line when another message follows. *
   * Leave 10 extra spaces for that.                                      */
  char msg[71];
  struct io_message *next;
} io_message_t;

static io_message_t *io_head, *io_tail;

void io_init_terminal(void)
{
  initscr();
  raw();
  noecho();
  curs_set(0);
  keypad(stdscr, TRUE);
  start_color();
  init_pair(COLOR_RED, COLOR_RED, COLOR_BLACK);
  init_pair(COLOR_GREEN, COLOR_GREEN, COLOR_BLACK);
  init_pair(COLOR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
  init_pair(COLOR_BLUE, COLOR_BLUE, COLOR_BLACK);
  init_pair(COLOR_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
  init_pair(COLOR_CYAN, COLOR_CYAN, COLOR_BLACK);
  init_pair(COLOR_WHITE, COLOR_WHITE, COLOR_BLACK);
}

void io_reset_terminal(void)
{
  endwin();

  while (io_head) {
    io_tail = io_head;
    io_head = io_head->next;
    free(io_tail);
  }
  io_tail = NULL;
}

void io_queue_message(const char *format, ...)
{
  io_message_t *tmp;
  va_list ap;

  if (!(tmp = (io_message_t *) malloc(sizeof (*tmp)))) {
    perror("malloc");
    exit(1);
  }

  tmp->next = NULL;

  va_start(ap, format);

  vsnprintf(tmp->msg, sizeof (tmp->msg), format, ap);

  va_end(ap);

  if (!io_head) {
    io_head = io_tail = tmp;
  } else {
    io_tail->next = tmp;
    io_tail = tmp;
  }
}

static void io_print_message_queue(uint32_t y, uint32_t x)
{
  while (io_head) {
    io_tail = io_head;
    attron(COLOR_PAIR(COLOR_CYAN));
    mvprintw(y, x, "%-80s", io_head->msg);
    attroff(COLOR_PAIR(COLOR_CYAN));
    io_head = io_head->next;
    if (io_head) {
      attron(COLOR_PAIR(COLOR_CYAN));
      mvprintw(y, x + 70, "%10s", " --more-- ");
      attroff(COLOR_PAIR(COLOR_CYAN));
      refresh();
      getch();
    }
    free(io_tail);
  }
  io_tail = NULL;
}

/**************************************************************************
 * Compares trainer distances from the PC according to the rival distance *
 * map.  This gives the approximate distance that the PC must travel to   *
 * get to the trainer (doesn't account for crossing buildings).  This is  *
 * not the distance from the NPC to the PC unless the NPC is a rival.     *
 *                                                                        *
 * Not a bug.                                                             *
 **************************************************************************/
static int compare_trainer_distance(const void *v1, const void *v2)
{
  const character *const *c1 = (const character * const *) v1;
  const character *const *c2 = (const character * const *) v2;

  return (world.rival_dist[(*c1)->pos[dim_y]][(*c1)->pos[dim_x]] -
          world.rival_dist[(*c2)->pos[dim_y]][(*c2)->pos[dim_x]]);
}

static character *io_nearest_visible_trainer()
{
  character **c, *n;
  uint32_t x, y, count;

  c = (character **) malloc(world.cur_map->num_trainers * sizeof (*c));

  /* Get a linear list of trainers */
  for (count = 0, y = 1; y < MAP_Y - 1; y++) {
    for (x = 1; x < MAP_X - 1; x++) {
      if (world.cur_map->cmap[y][x] && world.cur_map->cmap[y][x] !=
          &world.pc) {
        c[count++] = world.cur_map->cmap[y][x];
      }
    }
  }

  /* Sort it by distance from PC */
  qsort(c, count, sizeof (*c), compare_trainer_distance);

  n = c[0];

  free(c);

  return n;
}

void io_display()
{
  uint32_t y, x;
  character *c;

  clear();
  for (y = 0; y < MAP_Y; y++) {
    for (x = 0; x < MAP_X; x++) {
      if (world.cur_map->cmap[y][x]) {
        mvaddch(y + 1, x, world.cur_map->cmap[y][x]->symbol);
      } else {
        switch (world.cur_map->map[y][x]) {
        case ter_boulder:
        case ter_mountain:
          attron(COLOR_PAIR(COLOR_MAGENTA));
          mvaddch(y + 1, x, '%');
          attroff(COLOR_PAIR(COLOR_MAGENTA));
          break;
        case ter_tree:
        case ter_forest:
          attron(COLOR_PAIR(COLOR_GREEN));
          mvaddch(y + 1, x, '^');
          attroff(COLOR_PAIR(COLOR_GREEN));
          break;
        case ter_path:
        case ter_exit:
          attron(COLOR_PAIR(COLOR_YELLOW));
          mvaddch(y + 1, x, '#');
          attroff(COLOR_PAIR(COLOR_YELLOW));
          break;
        case ter_mart:
          attron(COLOR_PAIR(COLOR_BLUE));
          mvaddch(y + 1, x, 'M');
          attroff(COLOR_PAIR(COLOR_BLUE));
          break;
        case ter_center:
          attron(COLOR_PAIR(COLOR_RED));
          mvaddch(y + 1, x, 'C');
          attroff(COLOR_PAIR(COLOR_RED));
          break;
        case ter_grass:
          attron(COLOR_PAIR(COLOR_GREEN));
          mvaddch(y + 1, x, ':');
          attroff(COLOR_PAIR(COLOR_GREEN));
          break;
        case ter_clearing:
          attron(COLOR_PAIR(COLOR_GREEN));
          mvaddch(y + 1, x, '.');
          attroff(COLOR_PAIR(COLOR_GREEN));
          break;
        default:
 /* Use zero as an error symbol, since it stands out somewhat, and it's *
  * not otherwise used.                                                 */
          attron(COLOR_PAIR(COLOR_CYAN));
          mvaddch(y + 1, x, '0');
          attroff(COLOR_PAIR(COLOR_CYAN)); 
       }
      }
    }
  }

  mvprintw(23, 1, "PC position is (%2d,%2d) on map %d%cx%d%c.",
           world.pc.pos[dim_x],
           world.pc.pos[dim_y],
           abs(world.cur_idx[dim_x] - (WORLD_SIZE / 2)),
           world.cur_idx[dim_x] - (WORLD_SIZE / 2) >= 0 ? 'E' : 'W',
           abs(world.cur_idx[dim_y] - (WORLD_SIZE / 2)),
           world.cur_idx[dim_y] - (WORLD_SIZE / 2) <= 0 ? 'N' : 'S');
  mvprintw(23, 45, "Money:$ %d",world.pc.money);
  mvprintw(22, 1, "%d known %s.", world.cur_map->num_trainers,
           world.cur_map->num_trainers > 1 ? "trainers" : "trainer");
  mvprintw(22, 30, "Nearest visible trainer: ");
  if ((c = io_nearest_visible_trainer())) {
    attron(COLOR_PAIR(COLOR_RED));
    
    mvprintw(22, 55, "%c at %d %c by %d %c.",
             c->symbol,
             abs(c->pos[dim_y] - world.pc.pos[dim_y]),
             ((c->pos[dim_y] - world.pc.pos[dim_y]) <= 0 ?
              'N' : 'S'),
             abs(c->pos[dim_x] - world.pc.pos[dim_x]),
             ((c->pos[dim_x] - world.pc.pos[dim_x]) <= 0 ?
              'W' : 'E'));
    attroff(COLOR_PAIR(COLOR_RED));
  } else {
    attron(COLOR_PAIR(COLOR_BLUE));
    mvprintw(22, 55, "NONE.");
    attroff(COLOR_PAIR(COLOR_BLUE));
  }

  io_print_message_queue(0, 0);

  refresh();
}

uint32_t io_teleport_pc(pair_t dest)
{
  /* Just for fun. And debugging.  Mostly debugging. */

  do {
    dest[dim_x] = rand_range(1, MAP_X - 2);
    dest[dim_y] = rand_range(1, MAP_Y - 2);
  } while (world.cur_map->cmap[dest[dim_y]][dest[dim_x]]                  ||
           move_cost[char_pc][world.cur_map->map[dest[dim_y]]
                                                [dest[dim_x]]] == INT_MAX ||
           world.rival_dist[dest[dim_y]][dest[dim_x]] < 0);

  return 0;
}

static void io_scroll_trainer_list(char (*s)[40], uint32_t count)
{
  uint32_t offset;
  uint32_t i;

  offset = 0;

  while (1) {
    for (i = 0; i < 13; i++) {
      mvprintw(i + 6, 19, " %-40s ", s[i + offset]);
    }
    switch (getch()) {
    case KEY_UP:
      if (offset) {
        offset--;
      }
      break;
    case KEY_DOWN:
      if (offset < (count - 13)) {
        offset++;
      }
      break;
    case 27:
      return;
    }

  }
}

static void io_list_trainers_display(npc **c,
                                     uint32_t count)
{
  uint32_t i;
  char (*s)[40]; /* pointer to array of 40 char */

  s = (char (*)[40]) malloc(count * sizeof (*s));

  mvprintw(3, 19, " %-40s ", "");
  /* Borrow the first element of our array for this string: */
  snprintf(s[0], 40, "You know of %d trainers:", count);
  mvprintw(4, 19, " %-40s ", *s);
  mvprintw(5, 19, " %-40s ", "");

  for (i = 0; i < count; i++) {
    snprintf(s[i], 40, "%16s %c: %2d %s by %2d %s",
             char_type_name[c[i]->ctype],
             c[i]->symbol,
             abs(c[i]->pos[dim_y] - world.pc.pos[dim_y]),
             ((c[i]->pos[dim_y] - world.pc.pos[dim_y]) <= 0 ?
              "North" : "South"),
             abs(c[i]->pos[dim_x] - world.pc.pos[dim_x]),
             ((c[i]->pos[dim_x] - world.pc.pos[dim_x]) <= 0 ?
              "West" : "East"));
    if (count <= 13) {
      /* Handle the non-scrolling case right here. *
       * Scrolling in another function.            */
      mvprintw(i + 6, 19, " %-40s ", s[i]);
    }
  }

  if (count <= 13) {
    mvprintw(count + 6, 19, " %-40s ", "");
    mvprintw(count + 7, 19, " %-40s ", "Hit escape to continue.");
    while (getch() != 27 /* escape */)
      ;
  } else {
    mvprintw(19, 19, " %-40s ", "");
    mvprintw(20, 19, " %-40s ",
             "Arrows to scroll, escape to continue.");
    io_scroll_trainer_list(s, count);
  }

  free(s);
}

static void io_list_trainers()
{
  npc **c;
  uint32_t x, y, count;

  c = (npc **) malloc(world.cur_map->num_trainers * sizeof (*c));

  /* Get a linear list of trainers */
  for (count = 0, y = 1; y < MAP_Y - 1; y++) {
    for (x = 1; x < MAP_X - 1; x++) {
      if (world.cur_map->cmap[y][x] && world.cur_map->cmap[y][x] !=
          &world.pc) {
        c[count++] = (npc *) world.cur_map->cmap[y][x];
      }
    }
  }

  /* Sort it by distance from PC */
  qsort(c, count, sizeof (*c), compare_trainer_distance);

  /* Display it */
  io_list_trainers_display(c, count);
  free(c);

  /* And redraw the map */
  io_display();
}

void io_pokemart()
{
    WINDOW* building_win;
    int open = 1;

    building_win = newwin(19,78, 2, 1);
    box(building_win,0,0);
    wrefresh(building_win);  
    
  while(open){
    
    mvprintw(0, 0, "Welcome to the Pokemart. Could I interest you in some Pokeballs?");
    mvwprintw(building_win,1,2,  "Press < to exit");   
    mvwprintw(building_win,3,2,  "1. Pokeballs:      $100");  
    mvwprintw(building_win,5,2,  "2. Greatballs:     $300");  
    mvwprintw(building_win,7,2,  "3. Ultraballs:     $1000");
    mvwprintw(building_win,9,2,  "4. Quickballs:     $500");    
    mvwprintw(building_win,11,2, "5. Potions:        $100");  
    mvwprintw(building_win,13,2, "6. Super Potions:  $300");  
    mvwprintw(building_win,15,2, "7. Hyper Potions:  $500"); 
    mvwprintw(building_win,17,2, "8. Revives:        $1000");   
    
    
    //mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);
    refresh();
    wrefresh(building_win);

    char input = getch();

    if(input == '<'){
      open = 0;
    }

    switch (input)
    {
    case '1':
      if(world.pc.money >= 100){
        if(world.pc.money == 100){
          world.pc.money = 0;
          world.pc.items[item_pokeball] +=1;
          wclear(building_win);
          mvwprintw(building_win,3, 45, "You purchased a pokeball!");
          mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);

          wrefresh(building_win);
        }else{
          world.pc.money -= 100;
          world.pc.items[item_pokeball] +=1;
          wclear(building_win);
          mvwprintw(building_win,3, 45, "You purchased a pokeball!");
          mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);
          wrefresh(building_win);
        }
        refresh();
      }else{
        wclear(building_win);
        mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);
        mvwprintw(building_win,5, 45, "You cant purchase this!");
        wrefresh(building_win);
      }

      break;
    case '2':
      if(world.pc.money >= 300){
        if(world.pc.money == 300){
          world.pc.money = 0;
          world.pc.items[item_greatball] +=1;
          wclear(building_win);
          mvwprintw(building_win,3, 45, "You purchased a greatball!");
          mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);

          wrefresh(building_win);
        }else{
          world.pc.money -= 300;
          world.pc.items[item_greatball] +=1;
          wclear(building_win);
          mvwprintw(building_win,3, 45, "You purchased a greatball!");
          mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);
          wrefresh(building_win);
        }
        refresh();
      }else{
        wclear(building_win);
        mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);
        mvwprintw(building_win,5, 45, "You cant purchase this!");
        wrefresh(building_win);
      }
      break;
    case '3':
      if(world.pc.money >= 1000){
        if(world.pc.money == 1000){
          world.pc.money = 0;
          world.pc.items[item_ultraball] +=1;
          wclear(building_win);
          mvwprintw(building_win,3, 45, "You purchased an ultraball!");
          mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);

          wrefresh(building_win);
        }else{
          world.pc.money -= 1000;
          world.pc.items[item_ultraball] +=1;
          wclear(building_win);
          mvwprintw(building_win,3, 45, "You purchased an ultraball!");
          mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);
          wrefresh(building_win);
        }
        refresh();
      }else{
        wclear(building_win);
        mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);
        mvwprintw(building_win,5, 45, "You cant purchase this!");
        wrefresh(building_win);
      }  
      break;
    case '4':
    if(world.pc.money >= 500){
        if(world.pc.money == 500){
          world.pc.money = 0;
          world.pc.items[item_quickball] +=1;
          wclear(building_win);
          mvwprintw(building_win,3, 45, "You purchased a quickball!");
          mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);

          wrefresh(building_win);
        }else{
          world.pc.money -= 500;
          world.pc.items[item_quickball] +=1;
          wclear(building_win);
          mvwprintw(building_win,3, 45, "You purchased a quickball!");
          mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);
          wrefresh(building_win);
        }
        refresh();
      }else{
        wclear(building_win);
        mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);
        mvwprintw(building_win,5, 45, "You cant purchase this!");
        wrefresh(building_win);
      }
      break;
    case '5':
    if(world.pc.money >= 100){
        if(world.pc.money == 100){
          world.pc.money = 0;
          world.pc.items[item_potion] +=1;
          wclear(building_win);
          mvwprintw(building_win,3, 45, "You purchased a potion!");
          mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);

          wrefresh(building_win);
        }else{
          world.pc.money -= 100;
          world.pc.items[item_potion] +=1;
          wclear(building_win);
          mvwprintw(building_win,3, 45, "You purchased a potion!");
          mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);
          wrefresh(building_win);
        }
        refresh();
      }else{
        wclear(building_win);
        mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);
        mvwprintw(building_win,5, 45, "You cant purchase this!");
        wrefresh(building_win);
      }
      break;
    case '6':
    if(world.pc.money >= 300){
        if(world.pc.money == 300){
          world.pc.money = 0;
          world.pc.items[item_superpotion] +=1;
          wclear(building_win);
          mvwprintw(building_win,3, 45, "You purchased a superpotion!");
          mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);

          wrefresh(building_win);
        }else{
          world.pc.money -= 300;
          world.pc.items[item_superpotion] +=1;
          wclear(building_win);
          mvwprintw(building_win,3, 45, "You purchased a superpotion!");
          mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);
          wrefresh(building_win);
        }
        refresh();
      }else{
        wclear(building_win);
        mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);
        mvwprintw(building_win,5, 45, "You cant purchase this!");
        wrefresh(building_win);
      }
      break;
    case '7':
    if(world.pc.money >= 500){
        if(world.pc.money == 500){
          world.pc.money = 0;
          world.pc.items[item_hyperpotion] +=1;
          wclear(building_win);
          mvwprintw(building_win,3, 45, "You purchased a hyperpotion!");
          mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);

          wrefresh(building_win);
        }else{
          world.pc.money -= 500;
          world.pc.items[item_hyperpotion] +=1;
          wclear(building_win);
          mvwprintw(building_win,3, 45, "You purchased a hyperpotion!");
          mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);
          wrefresh(building_win);
        }
        refresh();
      }else{
        wclear(building_win);
        mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);
        mvwprintw(building_win,5, 45, "You cant purchase this!");
        wrefresh(building_win);
      }
      break;
    case '8':
    if(world.pc.money >= 1000){
        if(world.pc.money == 1000){
          world.pc.money = 0;
          world.pc.items[item_revive] +=1;
          wclear(building_win);
          mvwprintw(building_win,3, 45, "You purchased a revive!");
          mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);

          wrefresh(building_win);
        }else{
          world.pc.money -= 1000;
          world.pc.items[item_revive] +=1;
          wclear(building_win);
          mvwprintw(building_win,3, 45, "You purchased a revive!");
          mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);
          wrefresh(building_win);
        }
        refresh();
      }else{
        wclear(building_win);
        mvwprintw(building_win,1, 45, "Your Money :%d",world.pc.money);
        mvwprintw(building_win,5, 45, "You cant purchase this!");
        wrefresh(building_win);
      }
      break;
      
    default:
      mvprintw(0, 0, "Invalid Input");
      break;
    }


    wrefresh(building_win);
    refresh();

  }
  
   endwin();
  
   io_display();
   mvprintw(0, 0, "Thanks come again!");
   refresh();

}

void open_pc(WINDOW *w){
  WINDOW* pc_win;
  int open = 1;
  pc_win = newwin(19,78, 2, 1);
  box(pc_win,0,0);
  mvwprintw(pc_win,1,2,  "Press < to exit");  
  

  int i;
  int num_poke;
  num_poke = world.poke_pc.size();
  if(num_poke == 0){
      mvwprintw(pc_win,1,45,"No pokemon's here!");  
  }
  wrefresh(pc_win);

  while(open){
    wrefresh(pc_win);
    num_poke = world.poke_pc.size();
    
    mvwprintw(pc_win,1,2,  "Press < to exit");   
    
    if(num_poke == 0){
      mvwprintw(pc_win,1,45,"No pokemon's here!");  
    }else{
       for(i =0; i<num_poke; i++){
         mvwprintw(pc_win,i+2,2,  "%d: %s " ,i+1, world.poke_pc[i]->get_species());   
          wrefresh(pc_win);
        }
        wrefresh(pc_win);
    }
    wrefresh(pc_win); 
    char input = getch();

    if(input == '<'){
      open = 0;
    }

    wrefresh(pc_win);
    refresh();
  }
  wrefresh(pc_win);
  wclear(w);
  endwin();
}



void io_pokemon_center()
{
  mvprintw(0, 0, "Welcome to the Pokemon Center!");
  WINDOW* building_win;
    int open = 1;

    building_win = newwin(19,78, 2, 1);
    box(building_win,0,0);
   
    mvwprintw(building_win,1,45,"                               ");  
    wrefresh(building_win);  
  while(open){
    
    box(building_win,0,0);
    mvprintw(0, 0, "Welcome to the Pokemart. Could I interest you in some Pokeballs?");
    mvwprintw(building_win,1,2,  "Press < to exit");   
    mvwprintw(building_win,3,2,  "Press h to heal your party"); 
    mvwprintw(building_win,5,2,  "Press p to open the pc");
    mvwprintw(building_win,1,45,"                               ");        
    mvwprintw(building_win,2,2,"                                 ");        
    
    
    refresh();
    wrefresh(building_win);

    char input = getch();

    if(input == '<'){
      open = 0;
    }

    if(input == 'p'){
      open_pc(building_win);
    }

    if(input == 'h'){
      int i;
      int p_count;        

      if(can_fight() == false){
        for(int i= 0; i<6; i++){
          if(world.pc.pokemon_party[i] != NULL){
            p_count++;
          }
        }

        for(i = 0; i<p_count; i++){
          world.pc.pokemon_party[i]->heal(10000); 
        }
          
        mvwprintw(building_win,7,2,  "Your party has been restored to full hp ");   
        refresh();  
      }else{
        
        mvwprintw(building_win,7,2,  "Your party is already full!               ");   
        refresh();
      }

    }




    wrefresh(building_win);
    refresh();
  }
  

   endwin();
   io_display();
   mvprintw(0, 0, "Thanks come again!");
   refresh();

}

void io_battle(character *aggressor, character *defender)
{
  npc *n = (npc *) ((aggressor == &world.pc) ? defender : aggressor);
  if(can_fight() == false){
    return;
  }

  trainer_battle(n);

  io_display();
  refresh();
  getch();

  
  

  n->defeated = 1;
  if (n->ctype == char_hiker || n->ctype == char_rival) {
    n->mtype = move_wander;
  }
}

uint32_t move_pc_dir(uint32_t input, pair_t dest)
{
  dest[dim_y] = world.pc.pos[dim_y];
  dest[dim_x] = world.pc.pos[dim_x];

  switch (input) {
  case 1:
  case 2:
  case 3:
    dest[dim_y]++;
    break;
  case 4:
  case 5:
  case 6:
    break;
  case 7:
  case 8:
  case 9:
    dest[dim_y]--;
    break;
  }
  switch (input) {
  case 1:
  case 4:
  case 7:
    dest[dim_x]--;
    break;
  case 2:
  case 5:
  case 8:
    break;
  case 3:
  case 6:
  case 9:
    dest[dim_x]++;
    break;
  case '>':
    if (world.cur_map->map[world.pc.pos[dim_y]][world.pc.pos[dim_x]] ==
        ter_mart) {
      io_pokemart();
    }
    if (world.cur_map->map[world.pc.pos[dim_y]][world.pc.pos[dim_x]] ==
        ter_center) {
      io_pokemon_center();
    }
    break;
  }

  if (world.cur_map->cmap[dest[dim_y]][dest[dim_x]]) {
    if (dynamic_cast<npc *>(world.cur_map->cmap[dest[dim_y]][dest[dim_x]]) &&
        ((npc *) world.cur_map->cmap[dest[dim_y]][dest[dim_x]])->defeated) {
      // Some kind of greeting here would be nice
      return 1;
    } else if (dynamic_cast<npc *>
               (world.cur_map->cmap[dest[dim_y]][dest[dim_x]])) {
      io_battle(&world.pc, world.cur_map->cmap[dest[dim_y]][dest[dim_x]]);
      // Not actually moving, so set dest back to PC position
      dest[dim_x] = world.pc.pos[dim_x];
      dest[dim_y] = world.pc.pos[dim_y];
    }
  }
  
  if (move_cost[char_pc][world.cur_map->map[dest[dim_y]][dest[dim_x]]] ==
      INT_MAX) {
    return 1;
  }

  return 0;
}

void io_teleport_world(pair_t dest)
{
  /* mvscanw documentation is unclear about return values.  I believe *
   * that the return value works the same way as scanf, but instead   *
   * of counting on that, we'll initialize x and y to out of bounds   *
   * values and accept their updates only if in range.                */
  int x = INT_MAX, y = INT_MAX;
  
  world.cur_map->cmap[world.pc.pos[dim_y]][world.pc.pos[dim_x]] = NULL;

  echo();
  curs_set(1);
  do {
    mvprintw(0, 0, "Enter x [-200, 200]:           ");
    refresh();
    mvscanw(0, 21, "%d", &x);
  } while (x < -200 || x > 200);
  do {
    mvprintw(0, 0, "Enter y [-200, 200]:          ");
    refresh();
    mvscanw(0, 21, "%d", &y);
  } while (y < -200 || y > 200);

  refresh();
  noecho();
  curs_set(0);

  x += 200;
  y += 200;

  world.cur_idx[dim_x] = x;
  world.cur_idx[dim_y] = y;

  new_map(1);
  io_teleport_pc(dest);
}

//First thing you see when starting the game!
void io_choose_starter()
{
    WINDOW* start_screen; 

    pokemon *s1 = new pokemon(1);
    pokemon *s2 = new pokemon(1);
    pokemon *s3 = new pokemon(1); 

    start_screen = newwin( 10, 40, 0, 0);
    refresh();
    box(start_screen,0,0);

    mvprintw(10,0, " Choose Your Starting Pokemon: ");

    //Click & Keyboard Input
    mvwprintw(start_screen, 1,1, "Starter 1: %s ", s1->get_species() );
    mvwprintw(start_screen, 3,1, "Starter 2: %s ", s2->get_species() );
    mvwprintw(start_screen, 5,1, "Starter 3: %s ", s3->get_species() );
    
    wrefresh(start_screen);

    char input =  getch();

    //Initialize the PC's pokemon party
    int i;
    for(i = 0; i<6; i++){
      world.pc.pokemon_party[i] = NULL;
    }


    //If the input is not 1, 2 or 3 then It won't start the game!
    int valid = 0;

    while(valid == 0){
      if(input != '1' && input != '2' && input  != '3'){
        mvprintw(13,0," Invalid Input!");
        refresh();
        input = getch();
      }else{

        switch (input)
        {
        case '1':
          world.pc.pokemon_party[0] = s1;
          delete s2;
          delete s3;
          refresh();
          break;
        case '2':
          world.pc.pokemon_party[0] = s2;
          delete s1;
          delete s3;
          refresh();
          break;
        case '3':
          world.pc.pokemon_party[0] = s3;
          delete s1;
          delete s2;
          refresh();
          break;   
        default:
          break; 
        } 
        
         clear();
         refresh();
         mvprintw(0,0," Congrats you chose: %s!", world.pc.pokemon_party[0]->get_species());
         refresh();
         mvprintw(2,0," Press any button to continue... ");
         getch(); 
         break; 
      }
    }

    endwin();
}

void debug(int lineAt){
  mvprintw(15,0,"Line: %d", lineAt);
  refresh();
}


void catch_pokemon(WINDOW *w, pokemon *p){
    int i;
    bool spot_available;

    if(world.pc.pokemon_party [0] != NULL
     && world.pc.pokemon_party[1] != NULL 
     && world.pc.pokemon_party[2] != NULL 
     && world.pc.pokemon_party[3] != NULL 
     && world.pc.pokemon_party[4] != NULL
     && world.pc.pokemon_party[5] != NULL){
      spot_available = false;
     }else{
      spot_available = true;
     }

    if(spot_available == false){
      mvprintw(0,0, "Sent to PC");
      // mvwprintw(w,0,0, "Party is Full! Cannot Catch Pokemon!" );  
      world.poke_pc.push_back(p);
      world.pc.items[item_pokeball] = world.pc.items[item_pokeball]-1;
      wrefresh(w);
      world.pc.in_battle =0;
      return;
    } 

    if(spot_available == true){

      i = 0;  
      while(i <6){
          if(world.pc.pokemon_party[i] == NULL){
            world.pc.pokemon_party[i] = p;
            world.pc.items[item_pokeball] = world.pc.items[item_pokeball]-1;
            world.pc.in_battle = 0;
            p->fainted = true;
            p->set_hp(0);
            refresh();
            i = 100;
            return;
            break;
          }
          i++;
      }
      
      world.pc.in_battle = 0;
    }
    refresh();
}


static int calcNumPokemon(){
  float frandom = (float) rand() / RAND_MAX;
  if (frandom <= 0.08){
      return 6;
  }
  else if (frandom <= 0.13){
      return 5;
  }
  else if (frandom <= 0.22)
  {
      return 4;
  }
  else if (frandom <= 0.36)
  {
      return 3;
  }
  else if (frandom <= 0.6)
  {
      return 2;
  }
  else{
    return 1;
  }
}

void trainer_battle(npc *npc){

    if(can_fight() == false){
      return;
    }

    int in_battle = 1;

    int i;
    
    for (i = 0; i< 6; i++){
      npc->pokemon_party[i] = NULL;
    }

    int md = (abs(world.cur_idx[dim_x] - (WORLD_SIZE / 2)) +
              abs(world.cur_idx[dim_y] - (WORLD_SIZE / 2)));
    int minl, maxl;

    if (md <= 200) {
      minl = 1;
      maxl = md / 2;
    } else {
      minl = (md - 200) / 2;
      maxl = 100;
    }
    if (minl < 1) {
      minl = 1;
    }
    if (minl > 100) {
      minl = 100;
    }
    if (maxl < 1) {
      maxl = 1;
    }
    if (maxl > 100) {
      maxl = 100;
    }

  int npc_party_size = calcNumPokemon();
  int p;  
    for(p = 0; p< npc_party_size; p++){
      npc->pokemon_party[p] = new pokemon(rand() % (maxl - minl + 1) + minl) ;
    }

    // npc->pokemon_party[0] = new pokemon(rand() % (maxl - minl + 1) + minl) ;
    // npc->pokemon_party[1] = new pokemon(rand() % (maxl - minl + 1) + minl) ;
    // npc->pokemon_party[2] = new pokemon(rand() % (maxl - minl + 1) + minl) ;

    int next_pokemon = 0;

     pokemon *my_pokemon = world.pc.pokemon_party[0];


      if(world.pc.pokemon_party[0] != NULL && world.pc.pokemon_party[0]->get_hp() != 0 && can_fight() == true){
        my_pokemon = world.pc.pokemon_party[0];
      }else if(world.pc.pokemon_party[1] != NULL && world.pc.pokemon_party[1]->get_hp() != 0 && can_fight() == true){
        my_pokemon = world.pc.pokemon_party[1];
      }else if(world.pc.pokemon_party[2] != NULL && world.pc.pokemon_party[2]->get_hp() != 0 && can_fight() == true){
        my_pokemon = world.pc.pokemon_party[2];
      }else if(world.pc.pokemon_party[3] != NULL && world.pc.pokemon_party[3]->get_hp() != 0 && can_fight() == true){
        my_pokemon = world.pc.pokemon_party[3];
      }else if(world.pc.pokemon_party[4] != NULL && world.pc.pokemon_party[4]->get_hp() != 0 && can_fight() == true){
        my_pokemon = world.pc.pokemon_party[4];
      }else if(world.pc.pokemon_party[5] != NULL && world.pc.pokemon_party[5]->get_hp() != 0 && can_fight() == true){
        my_pokemon = world.pc.pokemon_party[5];
      }


    pokemon *npc_pokemon = npc->pokemon_party[0];


    do{
      clear();  
      
      refresh();
      mvprintw(0,0,"Your opponent sent out: %s ", npc_pokemon->get_species());
      mvprintw(1,0,"%s HP: %d ", npc_pokemon->get_species(), npc_pokemon->get_hp());


      mvprintw(5,0,"You sent out: %s", my_pokemon->get_species());
      mvprintw(6,0,"%s HP: %d ", my_pokemon->get_species(), my_pokemon->get_hp());

      mvprintw(10,0,"Press '1' to attack");
      mvprintw(12,0,"Press 'b' to open your bag ");
      mvprintw(0,55,"NPC's party size: %d ", npc_party_size );
      refresh();

      //Swaps pokemon when fainted 
      if(npc_pokemon->get_hp() == 0){
        next_pokemon++;

        if(next_pokemon > npc_party_size || npc->pokemon_party[next_pokemon] == NULL){
          in_battle = 0;
          world.pc.money +=  npc->money_given;
        }else{
          npc_pokemon =  npc->pokemon_party[next_pokemon];
          refresh();
        }

      }else if(my_pokemon->get_hp() == 0){
        in_battle = 0;
      }

      refresh();

      char input = getch();
      int my_turn = 1;
      int damage; 

    while(my_turn){
      if(input == '1'){
        int i;
        for(i = 0; i<4; i++){
          mvprintw(13 + i ,0, "%d. %s", i+1, my_pokemon->get_move(i));
          refresh(); 
        }

        char select_move = getch();
        switch (select_move)
        {
          case '1':
            if(strcmp(my_pokemon->get_move(0), "" ) == 0){
              mvprintw(3,0,"This move doesn't exist!");
              refresh();
              getch();  
            }else{

            damage = my_pokemon->get_move_damage(0);
            
            mvprintw(1,0,"%s HP: %d ", npc_pokemon->get_species(), npc_pokemon->get_hp());
            refresh();
            
            if(my_pokemon->get_move_accuracy(0) > rand() %100 ){    
                mvprintw(3,0,"You Dealt %d damage", damage);
                npc_pokemon->set_hp(damage * -1);
                mvprintw(1,0,"%s HP: %d ", npc_pokemon->get_species(), npc_pokemon->get_hp());
                refresh();
                my_turn = 0;
              }else{
                //miss
                damage = -1;
                mvprintw(3,0,"Missed");
                refresh();
                my_turn = 0;
              }
            }
            break;

          case '2':
          if(strcmp(my_pokemon->get_move(1), "" ) == 0){
              mvprintw(3,0,"This move doesn't exist!");
              refresh();
              getch();  
            }else{
              damage = my_pokemon->get_move_damage(1);
              if(my_pokemon->get_move_accuracy(1) > rand() %100 ){
                mvprintw(3,0,"You Dealt %d damage", damage);
                npc_pokemon->set_hp(damage * -1);
                mvprintw(1,0,"%s HP: %d ", npc_pokemon->get_species(), npc_pokemon->get_hp());
                refresh();
                my_turn = 0;
              }else{
                //miss
                damage = -1;
                refresh();
                my_turn = 0;
              }
            }
            break;
          case '3':
            if(strcmp(my_pokemon->get_move(2), "" ) == 0){
              mvprintw(3,0,"This move doesn't exist!");
              refresh();
              getch();  
            }else{
              damage = my_pokemon->get_move_damage(2);
              if(my_pokemon->get_move_accuracy(2) > rand() %100 ){
                mvprintw(3,0,"You Dealt %d damage", damage);
                npc_pokemon->set_hp(damage * -1);
                mvprintw(1,0,"%s HP: %d ", npc_pokemon->get_species(), npc_pokemon->get_hp());
                refresh();
                my_turn = 0;
              }else{
                //miss
                damage = -1;
                refresh();
                my_turn = 0;
              }
            
            }
            break;
          case '4':
            if( strcmp(my_pokemon->get_move(3), "" ) == 0){
              mvprintw(3,0,"This move doesn't exist!");
              refresh();
              getch();  
            }else{
              damage = my_pokemon->get_move_damage(3);
              if(my_pokemon->get_move_accuracy(3) > rand() %100 ){
                mvprintw(3,0,"You Dealt %d damage", damage);
                npc_pokemon->set_hp(damage * -1);
                mvprintw(1,0,"%s HP: %d ", npc_pokemon->get_species(), npc_pokemon->get_hp());
                refresh();
                my_turn = 0;
              }else{
                //miss
                damage = -1;
                refresh();
                my_turn = 0;
              }
            }
            break;  
          default:
              mvprintw(3,0, "ERROR");
              refresh();
            break;
        }
         
        refresh();
        getch();
      }
 
      if(input == 'b'){
       io_open_bag_tb(my_pokemon);
       break;
      
      }

      input = getch();
      refresh();
    }
    
      int npc_damage = npc_pokemon->get_move_damage(rand() % 1 + 1);
      if(npc_pokemon->get_move_accuracy(rand() % 1 +1) > rand() % 100){   
          my_pokemon->set_hp(npc_damage * -1);
          mvprintw(4,0,"%s HP: %d ", my_pokemon->get_species(), my_pokemon->get_hp());
          refresh();
          my_turn = 1 ;
      }else{
        //miss
          npc_damage = -1;
          my_turn = 1;
      }

      refresh();
      if(my_pokemon->get_hp() == 0){
        in_battle = 0;
      }  
    }while(in_battle);
   

}

void wild_poke_battle(pokemon *p){
    
      world.pc.in_battle = 1;
      int attempts = 1;
      
      pokemon *my_pokemon = world.pc.pokemon_party[0];


      if(world.pc.pokemon_party[0] != NULL && world.pc.pokemon_party[0]->get_hp() != 0 && can_fight() == true){
        my_pokemon = world.pc.pokemon_party[0];
      }else if(world.pc.pokemon_party[1] != NULL && world.pc.pokemon_party[1]->get_hp() != 0 && can_fight() == true){
        my_pokemon = world.pc.pokemon_party[1];
      }else if(world.pc.pokemon_party[2] != NULL && world.pc.pokemon_party[2]->get_hp() != 0 && can_fight() == true){
        my_pokemon = world.pc.pokemon_party[2];
      }else if(world.pc.pokemon_party[3] != NULL && world.pc.pokemon_party[3]->get_hp() != 0 && can_fight() == true){
        my_pokemon = world.pc.pokemon_party[3];
      }else if(world.pc.pokemon_party[4] != NULL && world.pc.pokemon_party[4]->get_hp() != 0 && can_fight() == true){
        my_pokemon = world.pc.pokemon_party[4];
      }else if(world.pc.pokemon_party[5] != NULL && world.pc.pokemon_party[5]->get_hp() != 0 && can_fight() == true){
        my_pokemon = world.pc.pokemon_party[5];
      }
    


    do{
      
      clear();
      mvprintw(0,0,"A wild %s appeared! ", p->get_species());
      mvprintw(1,0,"%s HP: %d ", p->get_species(), p->get_hp());

      mvprintw(3,0,"You sent out: %s", my_pokemon->get_species());
      mvprintw(4,0,"%s HP: %d ", my_pokemon->get_species(), my_pokemon->get_hp());

      mvprintw(6,0,"Press '1' to attack");
      mvprintw(8,0,"Press 'b' to open your bag ");
      mvprintw(10,0,"Press 'r' to run ");

     
      refresh();

      char input = getch();
      int damage; 

      if(input == '1'){

        int i;
        for(i = 0; i<4; i++){
          mvprintw(13 + i ,0, "%d. %s", i+1, my_pokemon->get_move(i));
          refresh(); 
        }

        char select_move = getch();
        switch (select_move)
        {
          case '1':
            if(strcmp(my_pokemon->get_move(0), "" ) == 0){
              mvprintw(15,0,"This move doesn't exist!");
              refresh();
              getch();  
            }else{

            damage = my_pokemon->get_move_damage(0);
            
            
            mvprintw(1,0,"%s HP: %d ", p->get_species(), p->get_hp());
            refresh();
            
            if(my_pokemon->get_move_accuracy(0) > rand() %100 ){    
                mvprintw(12,0,"You Dealt %d damage", damage);
                p->set_hp(damage * -1);
                mvprintw(1,0,"%s HP: %d ", p->get_species(), p->get_hp());
                refresh();
              }else{
                //miss
                damage = -1;
                mvprintw(12,0,"Missed");
                refresh();
              }

            }
            break;

          case '2':
          if(strcmp(my_pokemon->get_move(1), "" ) == 0){
              mvprintw(12,0,"This move doesn't exist!");
              refresh();
              getch();  
            }else{
              damage = my_pokemon->get_move_damage(1);
              if(my_pokemon->get_move_accuracy(1) > rand() %100 ){
                mvprintw(12,0,"You Dealt %d damage", damage);
                p->set_hp(damage * -1);
                mvprintw(1,0,"%s HP: %d ", p->get_species(), p->get_hp());
                refresh();
              }else{
                //miss
                damage = -1;
              }
            }
            break;
          case '3':
            if(strcmp(my_pokemon->get_move(2), "" ) == 0){
              mvprintw(12,0,"This move doesn't exist!");
              refresh();
              getch();  
            }else{
              damage = my_pokemon->get_move_damage(2);
              if(my_pokemon->get_move_accuracy(2) > rand() %100 ){
                mvprintw(12,0,"You Dealt %d damage", damage);
                p->set_hp(damage * -1);
                mvprintw(1,0,"%s HP: %d ", p->get_species(), p->get_hp());
                refresh();
              }else{
                //miss
                damage = -1;
              }
            
            }
            break;
          case '4':
            if( strcmp(my_pokemon->get_move(3), "" ) == 0){
              mvprintw(12,0,"This move doesn't exist!");
              refresh();
              getch();  
            }else{
              damage = my_pokemon->get_move_damage(3);

              if(my_pokemon->get_move_accuracy(3) > rand() %100 ){
                mvprintw(12,0,"You Dealt %d damage", damage);
                p->set_hp(damage * -1);
                mvprintw(1,0,"%s HP: %d ", p->get_species(), p->get_hp());
                refresh();
              }else{
                //miss
                damage = -1;
              }
            }
            break;  
          default:
              mvprintw(12,0, "ERROR");
              refresh();
            break;
        }
         
        refresh();
        getch();

      }
      
      //Open Bag
      if(input == 'b'){
        io_open_bag_in_wild_battle(my_pokemon,p); 
      }

      //Run
      if(input == 'r'){
        int odds = ((my_pokemon->get_speed() * 32) / ((p->get_speed() /4) % 256)) + 30 * attempts;
        attempts++;

        if(odds > rand() % 256){
          world.pc.in_battle = 0;
        }
        
      }
        
      // Wild pokemon has fainted
      if(p->get_hp() == 0){
         mvprintw(11,0,"%s has fainted! ", p->get_species());
         refresh();
        world.pc.in_battle = 0;
      }  


      //Wild pokemons turn
      int p_damage = p->get_move_damage(rand() % 1 + 1);
      if(p->get_move_accuracy(rand() % 1 + 1) > rand() %100 ){  
                my_pokemon->set_hp(p_damage * -1);
                 mvprintw(4,0,"%s HP: %d ", my_pokemon->get_species(), my_pokemon->get_hp());
                 refresh();
              }else{
                //miss
                p_damage = -1;
              }
      refresh();

      //PC's Pokemon has fainted
      if(my_pokemon->get_hp() == 0){
        my_pokemon->fainted = true;
        world.pc.in_battle = 0;
      }  

    }while(world.pc.in_battle);
    
}

void use_revive(pokemon *p){
          
          if(world.pc.items[item_revive] == 0){
            clear();
            mvprintw(15,0,"You've run out of this item!");
            refresh();
          }else if(p->fainted == true){
            clear();
            world.pc.items[item_revive] = world.pc.items[item_revive]-1;
            p->heal(25);
            mvprintw(15,0,"Your pokemon was revived!"); 
            refresh();
          }else{
            clear();
            mvprintw(15,0,"Your pokemon is not fainted!"); 
            refresh();
          }
          refresh();
}

void use_potion(pokemon *p,int heal_amt){
          
          if(p->get_hp() == p->base_hp){
             clear();
             mvprintw(17,0,"You're Pokemon is already at full HP");
             refresh();
          }
          
            
          if(world.pc.items[item_potion] == 0){  
            clear();
            mvprintw(17,0,"You've run out of this item!");
            refresh();
          }else{
            clear();
            world.pc.items[item_potion] = world.pc.items[item_potion]-1;
            p->heal(heal_amt);
            mvprintw(17,0,"You healed 20 HP"); 
            refresh();
          }
          refresh();
}

void use_superpotion(pokemon *p,int heal_amt){

          if(p->get_hp() == p->base_hp){
             clear();
             mvprintw(17,0,"You're Pokemon is already at full HP");
             refresh();
          }
          
            
          if(world.pc.items[item_superpotion] == 0){
            clear();
            
            mvprintw(17,0,"You've run out of this item!");
            refresh();
          }else{
            clear();
            world.pc.items[item_superpotion] = world.pc.items[item_superpotion]-1;
            p->heal(heal_amt);
            mvprintw(17,0,"You healed %d HP", heal_amt); 
            refresh();
          }
          refresh();
}

void use_hyperpotion(pokemon *p,int heal_amt){

          if(p->get_hp() == p->base_hp){
             clear();
             mvprintw(17,0,"You're Pokemon is already at full HP");
             refresh();
          }
          
            
          if(world.pc.items[item_hyperpotion] == 0){
            clear();
            mvprintw(17,0,"You've run out of this item!");
            refresh();
          }else{
            clear();
            world.pc.items[item_hyperpotion] = world.pc.items[item_hyperpotion]-1;
            p->heal(heal_amt);
            mvprintw(17,0,"You healed %d HP",heal_amt); 
            refresh();
          }
          refresh();
}


void io_pokemon_party(){
    WINDOW* party_win;
    int open = 1;
    int i;

    party_win = newwin(19,40, 2, 1);
    box(party_win,0,0);
    wrefresh(party_win);  
    wrefresh(party_win);
    
    while(open){
    
     mvwprintw(party_win,1,2, "Press < to exit"); 
     for(i=0; i<6; i++){
        if(world.pc.pokemon_party[i] != NULL ){
           mvwprintw(party_win,3+i,2,"%d: %s HP: %d ",i+1,world.pc.pokemon_party[i]->get_species(),world.pc.pokemon_party[i]->get_hp());
        }
    }

    wrefresh(party_win);
    char input = getch();

   

    if(input == '<'){
        open = 0;
    }

    wrefresh(party_win);
    refresh();

  }
    
    endwin();
    io_display();


}

bool is_party_full(){
    int i;
     for(i = 0; i<6; i++){
      if(world.pc.pokemon_party[i] == NULL){  
        return false;
        }
      }

    return true;

}

bool can_fight(){
    int i;
    for(i =0; i<6; i++){
       if(world.pc.pokemon_party[i] != NULL && world.pc.pokemon_party[i]->get_hp() != 0 ){
        return true; 
       } 
    }

    return false;
}


void io_open_bag_in_wild_battle(pokemon *my_pokemon, pokemon *enemy_pokemon){
    WINDOW* w_battle_screen;
    clear();
    int open = 1;
    
    //make a copy of the enemy pokemon and store it in the PC's Party

    while(open){
    
    refresh();  
    mvprintw(0,0, "Backpack: ");
    w_battle_screen = newwin( 14, 50, 1, 0); 
    refresh();

    box(w_battle_screen, 0,0);
    
    mvprintw(20,0, "Press '<' to exit ");
    mvprintw(16,0, "Choose Item: ");

    mvwprintw(w_battle_screen, 1,1, "1. Revives: %d ",world.pc.items[item_revive]);
    mvwprintw(w_battle_screen, 3,1, "2. Potions: %d ",world.pc.items[item_potion]);
    mvwprintw(w_battle_screen, 5,1, "3. Pokeball: %d ",world.pc.items[item_pokeball]);
    mvwprintw(w_battle_screen, 7,1, "4. Superpotion: %d ",world.pc.items[item_superpotion]);
    mvwprintw(w_battle_screen, 1,21, "5. Hyperpotion: %d ",world.pc.items[item_hyperpotion]);
    mvwprintw(w_battle_screen, 3,21, "6. Greatball: %d ",world.pc.items[item_greatball]);
    mvwprintw(w_battle_screen, 5,21, "7. Ultraball: %d ",world.pc.items[item_ultraball]);
    mvwprintw(w_battle_screen, 7,21, "8. Quickball: %d ",world.pc.items[item_quickball]);  
    wrefresh(w_battle_screen);

    char input = getch();

    if(input == '<'){
      break;
    }

    if(input !='1' && input !='2' && input != '3' && input != '4' && input != '5' && input != '6' && input != '7' && input != '8' ){
      mvprintw(17,0,"Invalid Input!");
      refresh();
      getch();
    }

    if(input == '1'){
      use_revive(my_pokemon);
    }

    if(input == '2'){
       use_potion(my_pokemon,20);
       refresh();
       getch();
    }

    if(input == '3'){
      // if(is_party_full() == true){
      //   clear();
      //   mvprintw(0,0, "Party is Full! Cannot Catch Pokemon");
      //   refresh();
      //   getch();
      // }else{
        catch_pokemon(w_battle_screen, enemy_pokemon);
        break;  
      // }

      refresh();
      getch();
    }

    if(input == '4'){
      use_superpotion(world.pc.pokemon_party[0],50);
    } 

    if(input == '5'){
      use_hyperpotion(world.pc.pokemon_party[0],100);
    } 

    wrefresh(w_battle_screen);

  }

  endwin();
  refresh();

}

void io_open_bag_overworld(){
    WINDOW* bag_screen;
    clear();

    int open = 1;
    while(open){
    

    mvprintw(0,0, "Items: ");
    bag_screen = newwin( 14, 50, 1, 0);
    refresh();
    box(bag_screen,0,0);

    mvprintw(20,0, "Press '<' to exit ");
    mvprintw(16,0, "Choose Item: ");

    mvwprintw(bag_screen, 1,1, "1. Revives: %d ",world.pc.items[item_revive]);
    mvwprintw(bag_screen, 3,1, "2. Potions: %d ",world.pc.items[item_potion]);
    mvwprintw(bag_screen, 5,1, "3. Pokeball: %d ",world.pc.items[item_pokeball]);
    mvwprintw(bag_screen, 7,1, "4. Superpotion: %d ",world.pc.items[item_superpotion]);
    mvwprintw(bag_screen, 1,21, "5. Hyperpotion: %d ",world.pc.items[item_hyperpotion]);
    mvwprintw(bag_screen, 3,21, "6. Greatball: %d ",world.pc.items[item_greatball]);
    mvwprintw(bag_screen, 5,21, "7. Ultraball: %d ",world.pc.items[item_ultraball]);
    mvwprintw(bag_screen, 7,21, "8. Quickball: %d ",world.pc.items[item_quickball]); 
    

    wrefresh(bag_screen);


    char input = getch();

    if(input == '<'){
      break;
    }

  if(input !='1' && input !='2' && input != '3' && input != '4' && input != '5' && input != '6' && input != '7' && input != '8' ){
      mvprintw(17,0,"Invalid Input!");
      refresh();
      getch();
    }
    
    if(input == '1'){
      use_revive(world.pc.pokemon_party[0]);
    }

    //heals the first pokemon in your party
    if(input == '2'){
      use_potion(world.pc.pokemon_party[0],20);
      refresh();
    }


  if(input == '3' || input == '6' || input == '7' || input == '8'){
     clear();
     mvprintw(18,0,"You cannot use that!");
     refresh();
    }

  if(input == '4'){
    use_superpotion(world.pc.pokemon_party[0],50);
    refresh();
  } 

    if(input == '5'){
    use_hyperpotion(world.pc.pokemon_party[0],100);
    refresh();
  } 
  
    refresh();
    //clear();
    }
    

    io_display();

}


void io_open_bag_tb(pokemon *p){
    WINDOW* bag_screen;
    clear();
    int open = 1;
    
    while(open){
    
   

    mvprintw(0,0, "Items: ");
    bag_screen = newwin( 14, 50, 1, 0);
    refresh();
    box(bag_screen,0,0);

    mvprintw(20,0, "Press '<' to exit ");
    mvprintw(16,0, "Choose Item: ");

    mvwprintw(bag_screen, 1,1, "1. Revives: %d ",world.pc.items[item_revive]);
    mvwprintw(bag_screen, 3,1, "2. Potions: %d ",world.pc.items[item_potion]);
    mvwprintw(bag_screen, 5,1, "3. Pokeball: %d ",world.pc.items[item_pokeball]);
    mvwprintw(bag_screen, 7,1, "4. Superpotion: %d ",world.pc.items[item_superpotion]);
    mvwprintw(bag_screen, 1,21, "5. Hyperpotion: %d ",world.pc.items[item_hyperpotion]);
    mvwprintw(bag_screen, 3,21, "6. Greatball: %d ",world.pc.items[item_greatball]);
    mvwprintw(bag_screen, 5,21, "7. Ultraball: %d ",world.pc.items[item_ultraball]);
    mvwprintw(bag_screen, 7,21, "8. Quickball: %d ",world.pc.items[item_quickball]); 
 

    wrefresh(bag_screen);


    char input = getch();

    if(input == '<'){
      break;
    }

    if(input !='1' && input !='2' && input != '3' && input != '4' && input != '5' && input != '6' && input != '7' && input != '8' ){
      mvprintw(17,0,"Invalid Input!");
      refresh();
      getch();
    }

    if(input =='1'){
      use_revive(p);
    }
    
    //heals the first pokemon in your party
    if(input == '2'){
      use_potion(p,20);
      refresh();
    }

    if(input == '3' || input == '6' || input == '7' || input == '8'){
     clear();
     mvprintw(18,0,"You cannot use that!");
     refresh();
    }

  
    // endwin();
    refresh();
    }
    
    
    io_display();
}



void io_handle_input(pair_t dest)
{
  uint32_t turn_not_consumed;
  int key;

  do {
    switch (key = getch()) {
    case '7':
    case 'y':
    case KEY_HOME:
      turn_not_consumed = move_pc_dir(7, dest);
      break;
    case '8':
    case 'k':
    case KEY_UP:
      turn_not_consumed = move_pc_dir(8, dest);
      break;
    case '9':
    case 'u':
    case KEY_PPAGE:
      turn_not_consumed = move_pc_dir(9, dest);
      break;
    case '6':
    case 'l':
    case KEY_RIGHT:
      turn_not_consumed = move_pc_dir(6, dest);
      break;
    case '3':
    case 'n':
    case KEY_NPAGE:
      turn_not_consumed = move_pc_dir(3, dest);
      break;
    case '2':
    case 'j':
    case KEY_DOWN:
      turn_not_consumed = move_pc_dir(2, dest);
      break;
    case '1':
    case 'b':
    case KEY_END:
      turn_not_consumed = move_pc_dir(1, dest);
      break;
    case '4':
    case 'h':
    case KEY_LEFT:
      turn_not_consumed = move_pc_dir(4, dest);
      break;
    case '5':
    case ' ':
    case '.':
    case KEY_B2:
      dest[dim_y] = world.pc.pos[dim_y];
      dest[dim_x] = world.pc.pos[dim_x];
      turn_not_consumed = 0;
      break;
    case '>':
      turn_not_consumed = move_pc_dir('>', dest);
      break;
    case 'p':
      io_pokemon_party();
      break; 
    case 'Q':
      dest[dim_y] = world.pc.pos[dim_y];
      dest[dim_x] = world.pc.pos[dim_x];
      world.quit = 1;
      turn_not_consumed = 0;
      break;
      break;
    case 't':
      io_list_trainers();
      turn_not_consumed = 1;
      break;
    case 'Y':
      /* Teleport the PC to a random place in the map.               */
      io_teleport_pc(dest);
      turn_not_consumed = 0;
      break;
    case 'm':
      io_list_trainers();
      turn_not_consumed = 1;
      break;
    case 'B':
      io_open_bag_overworld();
      break;
    case 'f':
      /* Fly to any map in the world.                                */
      io_teleport_world(dest);
      turn_not_consumed = 0;
      break;
    case 'q':
      /* Demonstrate use of the message queue.  You can use this for *
       * printf()-style debugging (though gdb is probably a better   *
       * option.  Not that it matters, but using this command will   *
       * waste a turn.  Set turn_not_consumed to 1 and you should be *
       * able to figure out why I did it that way.                   */
      io_queue_message("This is the first message.");
      io_queue_message("Since there are multiple messages, "
                       "you will see \"more\" prompts.");
      io_queue_message("You can use any key to advance through messages.");
      io_queue_message("Normal gameplay will not resume until the queue "
                       "is empty.");
      io_queue_message("Long lines will be truncated, not wrapped.");
      io_queue_message("io_queue_message() is variadic and handles "
                       "all printf() conversion specifiers.");
      io_queue_message("Did you see %s?", "what I did there");
      io_queue_message("When the last message is displayed, there will "
                       "be no \"more\" prompt.");
      io_queue_message("Have fun!  And happy printing!");
      io_queue_message("Oh!  And use 'Q' to quit!");

      dest[dim_y] = world.pc.pos[dim_y];
      dest[dim_x] = world.pc.pos[dim_x];
      turn_not_consumed = 0;
      break;
    default:
      /* Also not in the spec.  It's not always easy to figure out what *
       * key code corresponds with a given keystroke.  Print out any    *
       * unhandled key here.  Not only does it give a visual error      *
       * indicator, but it also gives an integer value that can be used *
       * for that key in this (or other) switch statements.  Printed in *
       * octal, with the leading zero, because ncurses.h lists codes in *
       * octal, thus allowing us to do reverse lookups.  If a key has a *
       * name defined in the header, you can use the name here, else    *
       * you can directly use the octal value.                          */
      mvprintw(0, 0, "Unbound key: %#o ", key);
      turn_not_consumed = 1;
    }
    refresh();
  } while (turn_not_consumed);
}



void io_encounter_pokemon()
{

  if(can_fight() == false){
     return;
  }

  pokemon *p;
  int md = (abs(world.cur_idx[dim_x] - (WORLD_SIZE / 2)) +
            abs(world.cur_idx[dim_y] - (WORLD_SIZE / 2)));
  int minl, maxl;

  if (md <= 200) {
    minl = 1;
    maxl = md / 2;
  } else {
    minl = (md - 200) / 2;
    maxl = 100;
  }
  if (minl < 1) {
    minl = 1;
  }
  if (minl > 100) {
    minl = 100;
  }
  if (maxl < 1) {
    maxl = 1;
  }
  if (maxl > 100) {
    maxl = 100;
  }

  p = new pokemon(rand() % (maxl - minl + 1) + minl);

  // io_queue_message("%s%s%s: HP:%d ATK:%d DEF:%d SPATK:%d SPDEF:%d SPEED:%d %s",
  //                  p->is_shiny() ? "*" : "", p->get_species(),
  //                  p->is_shiny() ? "*" : "", p->get_hp(), p->get_atk(),
  //                  p->get_def(), p->get_spatk(), p->get_spdef(),
  //                  p->get_speed(), p->get_gender_string());
  // io_queue_message("%s's moves: %s %s", p->get_species(),
  //                  p->get_move(0), p->get_move(1));

  // Later on, don't delete if captured
  wild_poke_battle(p);

  // delete p;
}
