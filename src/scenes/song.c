#include "song.h"

#include "../game.h"
#include "../drawing.h"
#include "../beatmap.h"
#include "pd_api/pd_api_file.h"

#include <stdio.h>


// I'm refactoring most of this stuff out
// It'll look clean soon i swer

static const float SIGHTREAD_DISTANCE = 1.0f;

// HACK i'll get rid of these soon
static float prompt_ease;
static int prompt_show;
static int prompt;

// sin values [0, 2pi]
static const float HEALTH_SHAKE[11] = {0, 2, 4, 4, 2, 0, -3, -5, -5, -3, 0};
static int missed;
static int health_shake_index = 10;

static int failed_to_load_song = 0;

static void menu_home_callback(void* userdata) {
  GameData* game = (GameData*)userdata;
  scene_transition(game->scene_manager, game->song_list_scene);
}

// TODO: make separate particle system
//       these aren't even particles
// struct Particle {
//   uint16_t x;
//   uint16_t y;
//   uint32_t life;
// };

// int particle_start;
// int particle_end;
// struct Particle particles[10];
// int pulses[10];

// static void disk_particles_update() {
//   for (int i = particle_start; i != particle_end; i = (i + 1) % 10) {
//     particles[i].life += 1;
//     pulses[i] += 1;
//     if (particles[i].life > 15) {
//       particle_start += 1;
//       particle_start %= 10;
//     }
//   }
// }

// static void particles_draw(struct GameData* data) {
//   for (int i = particle_start; i != particle_end; i = (i + 1) % 10) {
//     // struct Particle* p = &particles[i];
//     // playdate->graphics->drawRect(p->x - p->life / 2, p->y - p->life / 2, p->life, p->life, kColorBlack);
//     // playdate->graphics->drawRect(p->x - p->life / 2 - 1, p->y - p->life / 2 - 1, p->life + 2, p->life + 2, kColorWhite);
//     playdate->graphics->drawEllipse(200 - 32 - pulses[i] / 2, 120 - 32 - pulses[i] / 2, 64 + pulses[i], 64 + pulses[i], 1, 0.0f, 0.0f, kColorBlack);
//   }
// }

// static void particles_make(uint16_t x, uint16_t y) {
//   particles[particle_end] = (struct Particle){ x, y, 0 };
//   pulses[particle_end] = 0;
//   particle_end += 1;
//   particle_end %= 10; 
// }

static float lerp(float x1, float x2, float t) {
  // return x1 + t * (x2 - x1);
  return x1 * (1 - t) + x2 * t;
}

static void get_note_position(int position, float progress, int* x, int* y) {
  switch (position) {
    case NOTE_POS_L1:
      *x = lerp(0.0f, 177.0f, progress);
      *y = lerp(0.0f, 97.0f, progress);
      break;
    case NOTE_POS_L2:
      *x = lerp(0, 170.0f, progress);
      *y = lerp(60.0f, 108.0f, progress);
      break;
    case NOTE_POS_L3:
      *x = lerp(0, 168.0f, progress);
      *y = lerp(120.0f, 120.0f, progress);
      break;
    case NOTE_POS_L4:
      *x = lerp(0, 170.0f, progress);
      *y = lerp(180.0f, 132.0f, progress);
      break;
    case NOTE_POS_L5:
      *x = lerp(0.0, 177.0f, progress);
      *y = lerp(240.0f, 143.0f, progress);
      break;
    case NOTE_POS_R1:
      *x = lerp(400.0f, 223.0f, progress);
      *y = lerp(0.0f, 97.0f, progress);
      break;
    case NOTE_POS_R2:
      *x = lerp(400.0f, 212.0f, progress);
      *y = lerp(60.0f, 108.0f, progress);
      break;
    case NOTE_POS_R3:
      *x = lerp(400.0f, 232.0f, progress);
      *y = lerp(120.0f, 120.0f, progress);
      break;
    case NOTE_POS_R4:
      *x = lerp(400.0f, 212.0f, progress);
      *y = lerp(180.0f, 132.0f, progress);
      break;
    case NOTE_POS_R5:
      *x = lerp(400.0f, 223.0f, progress);
      *y = lerp(240.0f, 143.0f, progress);
      break;
    default:
      *x = 0.0f;
      *y = 0.0f;
      break;
  }
}

static float note_position_to_angle(int position) {  
  float note_angle;
  switch (position) {
    // left
    case NOTE_POS_L1:
      note_angle = 225.0f + (4.0f * 32.5f);
      break;
    case NOTE_POS_L2:
      note_angle = 225.0f + (3.0f * 22.5f);
      break;
    case NOTE_POS_L3:
      note_angle = 225.0f + (2.0f * 22.5f);
      break;
    case NOTE_POS_L4:
      note_angle = 225.0f + 22.5f;
      break;
    case NOTE_POS_L5:
      note_angle = 225.0f;
      break;

    // right
    case NOTE_POS_R5:
      note_angle = 135.0f;
      break;
    case NOTE_POS_R4:
      note_angle = 112.5f;
      break;
    case NOTE_POS_R3:
      note_angle = 90.0f;
      break;
    case NOTE_POS_R2:
      note_angle = 67.5f;
      break;
    case NOTE_POS_R1:
      note_angle = 45.0f;
      break;
    default:
      note_angle = 0.0f;
  }
  
  return note_angle;
}

static int angle_to_color(float crank_angle, float note_angle) {    
  // since disk is rotationally symmetrical, we can clamp it to 120 degrees 
  // get note angle relative to the local rotation of the disk
  int local_rot = ((int)note_angle - (int)crank_angle + 360) % 120;
  if (local_rot < 60) {
    return NOTE_WHITE;
  } else {
    return NOTE_BLACK;
  }
}

void song_on_start(void* game_data, void* song_data) {
  GameData* game = (GameData*)game_data;
  SongData* song = (SongData*)song_data;
  
  song->quit_menu_item = playdate->system->addMenuItem("Quit Song", menu_home_callback, game_data);
  
  song->note_particles = particles_newSystem((ParticleConfig){
    .max_emitters = 15,
    .particle_start_velocity = 0.5f,
    .particle_end_velocity = 0.1f,
    .particle_start_size = 3.0f,
    .particle_end_size = 1.0f,
  });
  
  song->hit_particles = particles_newSystem((ParticleConfig){
    .max_particles = 20,
    .max_emitters = 2,
    .emit_rate = 50.0f,
    .emit_lifetime = 0.2f,
    .particle_start_velocity = 0.7f,
    .particle_end_velocity = 0.1f,
    .particle_start_size = 10.0f,
    .particle_end_size = 1.0f,
  });

  song->health = MAX_HEALTH;
  song->display_score = 0;

  int load_song_result = beatmap_load(&song->beatmap, &game->header);
  if (!load_song_result) {
    song->health = 0;
    failed_to_load_song = 1;
    return;
  }
  
  for (int i = 0; i < song->beatmap.notes_length; ++i) {
    song->beatmap.notes[i].emitter = -1;
  }

  song->rhythmplayer = rhythm_newPlayer();
  rhythm_load(song->rhythmplayer, song->beatmap.audio_path, song->beatmap.bpm, song->beatmap.offset);
  rhythm_playOffset(song->rhythmplayer, -3.0f, 0);

  playdate->graphics->clear(kColorWhite);
}

void song_on_update(void* game_data, void* song_data) {
  GameData* game = (GameData*)game_data;
  SongData* song = (SongData*)song_data;
    
  PDButtons pressed;
  playdate->system->getButtonState(NULL, &pressed, NULL);
  
  // float beat_time = rhythm_getBeatTime(song->rhythmplayer);
  float time = rhythm_getTime(song->rhythmplayer);
  float progress = rhythm_getProgress(song->rhythmplayer);

  // TODO: it's not clear that this is only called once
  if (progress > 1.0f) {
    song->finished = 1;
    
    if (song->score > game->header.highscore) {
      SDFile* file = playdate->file->open(song->beatmap.name, kFileWrite);
      char buffer[100];
      int written = snprintf(buffer, 100, "%d", song->score);
      playdate->system->logToConsole("%d", written);
      playdate->file->write(file, buffer, written);
      playdate->file->close(file);
    }
  }
      
  if (song->health < 1 || song->finished) {
    // sp_stop(game, &game->song_player);
    rhythm_stop(song->rhythmplayer);
    if (pressed > 0) {
      scene_transition(game->scene_manager, game->song_list_scene);
    }
  }
  
  // disk_particles_update();

  struct Note* note;
  for (int i = song->index; i < song->beatmap.notes_length; ++i) {
    note = &song->beatmap.notes[i];
    if ((note->beat_time * 60.0f / song->beatmap.bpm) - SIGHTREAD_DISTANCE  > time) {
      break;
    }
    
    if (note->emitter == -1) {
      note->emitter = particles_createEmitter(song->note_particles);
      particles_startEmitter(song->note_particles, note->emitter);
    }
        
    if (note->type == NOTE_CLICK) {
      // too late
      if (note->time + 0.11f < time) {
        particles_destroyEmitter(song->note_particles, note->emitter);
        note->emitter = -1;
        song->miss_count += 1;
        song->index += 1;
        song->health -= 15;
        prompt_show = 1;
        missed = 1;
        prompt_ease = 1.0f;
        prompt = 0;
        song->combo = 0;
      }
  
      // playdate->graphics->setStencilImage(clear_bitmap, 1);
      if (pressed > 0 && song->index == i) {
        float diff = note->time - time;
        float diff_2 = diff * diff;
        
        // inside clickable range
        if (diff_2 < 0.11f * 0.11f && note->color == angle_to_color(playdate->system->getCrankAngle(), note_position_to_angle(note->position))) {
          particles_destroyEmitter(song->note_particles, note->emitter);
          note->emitter = -1;
          song->index += 1;
          song->health += 5;
          prompt_show = 1;
          prompt_ease = 1.0f;
          song->combo += 1;
          float progress = 1.0f - ((note->beat_time * 60.0f / song->beatmap.bpm) - time) / song->beatmap.bpm;
          int x, y;
          get_note_position(note->position, progress, &x, &y);
          emitter_id id = particles_createEmitter(song->hit_particles);
          particles_moveEmitter(song->hit_particles, id, x, y);
          particles_startEmitter(song->hit_particles, id);
          // particles_make(x, y);
          if (diff_2 < 0.045f * 0.045f) {
            song->perfect_count += 1;
            song->score += 100;
            prompt = 3;
          } else if (diff_2 < 0.07f * 0.07f) {
            song->good_count += 1;
            song->score += 30;
            prompt = 2;
          } else {
            song->ok_count += 1;
            song->score += 10;
            prompt = 1;
          }
        }
      }
    } else if (note->type == NOTE_NORMAL) {
      if (note->time < time) {
        song->index += 1;
        particles_destroyEmitter(song->note_particles, note->emitter);
        note->emitter = -1;
        float note_angle = note_position_to_angle(note->position);
        if (note->color == angle_to_color(playdate->system->getCrankAngle(), note_angle)) {
          float progress = 1.0f - ((note->beat_time * 60.0f / song->beatmap.bpm) - time) / SIGHTREAD_DISTANCE;
          int x, y;
          get_note_position(note->position, progress, &x, &y);
          // particles_make(x, y);
          song->score += 50;
          song->health += 3;
          song->combo += 1;
          song->normal_hit += 1;
        } else {
          song->combo = 0;
          missed = 1;
          song->health -= 5;
          song->normal_miss += 1;
        }
      }
    } else if (note->type == NOTE_DANGER) {
      if (note->time < time) {
        song->index += 1;
        particles_destroyEmitter(song->note_particles, note->emitter);
        note->emitter = -1;
        float note_angle = note_position_to_angle(note->position);
        if (note->color == angle_to_color(playdate->system->getCrankAngle(), note_angle)) {
          song->health -= 20;
          missed = 1;
          song->combo = 0;
          song->danger_miss += 1;
        } else {
          song->score += 25;
          song->health += 10;
          song->combo += 1;
          song->danger_hit += 1;
        }
      }
    }
  }
  
  if (song->health > MAX_HEALTH) {
    song->health = MAX_HEALTH;
  }
  
  if (song->ok_count + song->good_count + song->perfect_count + song->miss_count > 0) {
    song->accuracy = (float)song->ok_count * 10.0f + (float)song->good_count * 30.0f + (float)song->perfect_count * 100.0f + (float)song->danger_hit * 25.0f + (float)song->normal_hit * 50.0f;
    song->accuracy /= (float)(song->ok_count + song->good_count + song->perfect_count + song->miss_count) * 100.0f + (float)(song->danger_hit + song->danger_miss) * 25.0f + (float)(song->normal_hit + song->normal_miss) * 50.0f;
  } else {
    song->accuracy = 0.0f;
  }

  // Draw
  playdate->graphics->clear(kColorWhite);
  if (song->health < 1 || song->finished) {
    playdate->graphics->drawText("Game Over", 9, kASCIIEncoding, 200, 120);
    char buffer[100];
    snprintf(buffer, 100, "Score: %d", song->score);
    playdate->graphics->drawText(buffer, 99, kASCIIEncoding, 200, 150);
    if (failed_to_load_song) {
      playdate->graphics->drawText("Failed to open song", 99, kASCIIEncoding, 0, 10);
    }
    
    return;
  }
  
  particles_update(song->note_particles);
  particles_update(song->hit_particles);

  int x, y;
  float note_progress;
  for (int i = song->index; i < song->beatmap.notes_length; ++i) {
    note = &song->beatmap.notes[i];
    // not yet
    if ((note->beat_time * 60.0f / song->beatmap.bpm) - SIGHTREAD_DISTANCE > time) {
      break;
    }
    
    // The lerped values are precalculated
    note_progress = 1.0f - ((note->beat_time / song->beatmap.bpm * 60.0f) - time) / SIGHTREAD_DISTANCE;
    get_note_position(note->position, note_progress, &x, &y);
    
    particles_moveEmitter(song->note_particles, note->emitter, x, y);
    
    draw_note(game, x, y, note->type, note->color);
  }  

  playdate->system->getButtonState(&pressed, NULL, NULL);
  playdate->graphics->setStencilImage(game->grey_bitmap, 1);
  static int size = 0;
  if (song->index > 0) { 
    if (rhythm_isOnBeat(song->rhythmplayer, 0.2f)) {
      size += 15;
      if (size > 100) {
        size = 100;
      }
    } else {
      size -= 1;
      if (size < 75) {
        size = 75;
      }
    }
  } else {
    size = 70;
  }
  playdate->graphics->fillEllipse(200 - (size >> 1), 120 - (size >> 1), size, size, 0.0f, 0.0f, kColorBlack);
  playdate->graphics->setStencilImage(game->clear_bitmap, 1);
  draw_disk(game, 200, 120, playdate->system->getCrankAngle(), pressed > 0);
  
  // old particles
  // particles_draw(game);
  
  // UI
  // --
  // score
  char display_buffer[50];
  if (song->display_score < song->score) {
    int diff = song->score - song->display_score;
    song->display_score += (diff + 1) >> 1;
  }
  snprintf(display_buffer, 50, "%d", song->display_score);
  playdate->graphics->drawText(display_buffer, 50, kASCIIEncoding, 200 - playdate->graphics->getTextWidth(game->font, display_buffer, 26, kASCIIEncoding, 0) / 2.0f, 200);
  
  // name
  playdate->graphics->drawText(song->beatmap.name, 26, kASCIIEncoding, 200 - playdate->graphics->getTextWidth(game->font, song->beatmap.name, 26, kASCIIEncoding, 0) / 2.0f, 5);
  
  // health
  if (missed) {
    missed = 0;
    health_shake_index = 0;
  }
  if (health_shake_index > 10) {
    health_shake_index = 10;
  }
  int offset = HEALTH_SHAKE[health_shake_index];
  health_shake_index += 2;
  
  playdate->graphics->drawRect(200 - 50 + offset, 240 - 18 - 3, 100, 18, kColorBlack);
  playdate->graphics->fillRect(200 - 50 + 2 + offset, 240 - 18 - 3 + 2, 96 * song->health / 100, 14, kColorBlack);
  
  // timer
  playdate->graphics->drawEllipse(200 - 50 - 8 - 18 + offset, 240 - 3 - 18, 18, 18, 1, 0.0f, 0.0f, kColorBlack);
  playdate->graphics->fillEllipse(200 - 50 - 8 - 18 + offset, 240 - 3 - 18, 18, 18, 0.0f, 360.0f * progress, kColorBlack);
  
  // accuracy
  float accuracy = song->accuracy * 100.0f;
  snprintf(display_buffer, 50, "%d.%d%%", (int)accuracy, (int)((accuracy - (int)(accuracy)) * 100.0f + 0.5f));
  // playdate->graphics->setFont(game->accuracy_font);
  playdate->graphics->drawText(display_buffer, 50, kASCIIEncoding, 200 + 50 + 5 + offset, 240 - playdate->graphics->getFontHeight(game->font));
  // playdate->graphics->setFont(game->font);
    
  // prompt
  int prompt_y = (int)(prompt_ease * 10.0f);
  prompt_ease *= 0.9f;
  int height = 50;
  if (prompt_ease < 0.1f) {
    prompt_ease = 0.0f;
  } else {
    switch (prompt) {
      case 0:
    playdate->graphics->drawText("Miss", 4, kASCIIEncoding, 200 - playdate->graphics->getTextWidth(game->font, "Miss", 4, kASCIIEncoding, 0) / 2, height - prompt_y);
    break;
      case 1:
    playdate->graphics->drawText("OK", 2, kASCIIEncoding, 200 - playdate->graphics->getTextWidth(game->font, "OK", 2, kASCIIEncoding, 0) / 2, height - prompt_y);
    break;
      case 2:
    playdate->graphics->drawText("Good", 4, kASCIIEncoding, 200 - playdate->graphics->getTextWidth(game->font, "Good", 4, kASCIIEncoding, 0) / 2, height - prompt_y);
    break;
      case 3:
    playdate->graphics->drawText("Perfect", 7, kASCIIEncoding, 200 - playdate->graphics->getTextWidth(game->font, "Perfect", 7, kASCIIEncoding, 0) / 2, height - prompt_y);
    break;
      default:
    break;
    }
  }
  
  // combo
  if (song->combo > 0) {
    snprintf(display_buffer, 50, "Combo %d", song->combo);
    playdate->graphics->drawText(display_buffer, 50, kASCIIEncoding, 200 - playdate->graphics->getTextWidth(game->font, display_buffer, 50, kASCIIEncoding, 0) / 2, height - 20 - (prompt_y >> 1));
  }
}

void song_on_end(void* game_data, void* song_data) {
  SongData* song = (SongData*)song_data;

  playdate->system->removeMenuItem(song->quit_menu_item);

  rhythm_stop(song->rhythmplayer);
  rhythm_freePlayer(song->rhythmplayer);
}

