#ifndef POKEMON_H
# define POKEMON_H

enum pokemon_stat {
  stat_hp,
  stat_atk,
  stat_def,
  stat_spatk,
  stat_spdef,
  stat_speed
};

enum pokemon_gender {
  gender_female,
  gender_male
};

class pokemon {
 private:
  int level;
  int pokemon_index;
  int move_index[4];
  int pokemon_species_index;
  int IV[6];
  int effective_stat[6];
  bool shiny;
  int hp;
  pokemon_gender gender;
 public:
  pokemon(int level);
  const char *get_species() const;
  int base_hp;
  int get_hp() const;
  void heal(int num);
  int get_atk() const;
  int get_def() const;
  int get_spatk() const;
  int get_spdef() const;
  int get_speed() const;
  int get_level();
  void set_hp(int new_hp);
  const char *get_gender_string() const;
  bool fainted;
  bool is_shiny() const;
  const char *get_move(int i) const;
  int get_move_power(int i);
  int get_move_accuracy(int i);
  int get_move_damage(int move);
  bool is_fainted();

};

#endif
