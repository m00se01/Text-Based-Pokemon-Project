#ifndef IO_H
# define IO_H

#include "pokemon.h"
#include "poke327.h"

typedef struct character character_t;
typedef int16_t pair_t[2];

void io_init_terminal(void);
void io_reset_terminal(void);
void io_display(void);
void io_handle_input(pair_t dest);
void io_queue_message(const char *format, ...);
void trainer_battle(npc *npc);
void wild_poke_battle(pokemon *p);
void io_battle(character_t *aggressor, character_t *defender);
void io_encounter_pokemon(void);
void io_choose_starter(void);
void io_open_bag_overworld(void);
void io_open_bag_in_wild_battle(pokemon *p1, pokemon *p2);
void debug(int lineAt);
void io_pokemon_party(void);
bool is_party_full(void);

void use_potion(pokemon *p,int heal_amt);
void use_hyperpotion(pokemon *p,int heal_amt);
void use_superpotion(pokemon *p,int heal_amt);
bool can_fight(void);
void io_open_bag_tb(pokemon *p1);
void use_revive(pokemon *p);
// void open_pc(WINDOW *w);

#endif
