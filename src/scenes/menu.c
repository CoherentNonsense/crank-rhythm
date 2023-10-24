#include "menu.h"

#include "../game.h"
#include "../drawing.h"
#include "../utils.h"
#include <stdint.h>


// precomputed values to move the background notes
#define RNG_Y_LENGTH    12
#define RNG_TYPE_LENGTH 5
#define SIN_VALS_LENGTH 11

static const uint16_t RNG_Y[RNG_Y_LENGTH] = {50, 70, 200, 210, 140, 190, 100, 8, 120, 30, 180, 120};
static const uint8_t RNG_TYPE[RNG_TYPE_LENGTH] = {0, 1, 0, 2, 0};
static const int8_t SIN_VALS[SIN_VALS_LENGTH] = {0, 2, 4, 4, 2, 0, -3, -5, -5, -3, 0};

// #define WAVEFORMS_LENGTH 4

// static const float WAVEFORMS[WAVEFORMS_LENGTH * 10] = {
//   0.0f, 0.3f, 0.0f, 0.2f, 0.3f, 0.5f, 0.4f, 0.2f, 0.5f,
//   1.0f, 1.0f, 0.4f, 0.3f, 0.5f, 0.1f, 0.5f, 0.7f, 0.8f,
//   0.0f, 0.3f, 0.5f, 0.6f, 0.8f, 0.4f, 0.1f, 0.0f, 0.2f,
//   1.0f, 0.4f, 0.0f, 0.2f, 0.5f, 0.6f, 0.9f, 0.7f, 0.5f
// };

static const float CRANK_DURATION = 1.5f;

void menu_on_start(void* game_data, void* menu_data) {
  GameData* game = (GameData*)game_data;
  MenuData* menu = (MenuData*)menu_data;
  
  rhythm_load(game->rhythmplayer, "audio/menu", 175.0f, 0.0f);
  rhythm_play(game->rhythmplayer, 0);
    
  menu->progress = 0.0f;
  menu->progress_synth = playdate->sound->synth->newSynth();
  playdate->sound->synth->setWaveform(menu->progress_synth, kWaveformSawtooth);
  
  menu->particles = particles_newSystem((ParticleConfig){
    .max_particles = MENU_NOTES_LENGTH * 4,
    .max_emitters = MENU_NOTES_LENGTH,
    .emit_rate = 4,
    .particle_start_velocity = 0.2f,
    .particle_end_velocity = 0.1f,
    .particle_start_size = 3.0f,
    .particle_end_size = 1.0f,
  });

  // const char* err;
  // menu->waveform_bitmap = playdate->graphics->loadBitmap("images/waveform", &err);

  // for (int i = 0; i < 10; ++i) {
  //   menu->waveform[i] = 0.0f;
  // }
  
  for (int i = 0; i < 15; ++i) {
    MenuNote* note = &menu->bg_notes[i];
    note->y = (i * 2) % 12;
    note->type = (i * 3) % 5;
    note->color = (i * 7) % 2;
    note->x = i * 50;
    note->sin_offset = i * 3;
    note->emitter = particles_createEmitter(menu->particles);
    particles_startEmitter(menu->particles, note->emitter);
  }
}

void menu_on_update(void* game_data, void* menu_data) {
  GameData* game = (GameData*)game_data;
  MenuData* menu = (MenuData*)menu_data;
  
  PDButtons buttons;
  playdate->system->getButtonState(NULL, &buttons, NULL);
  
  // Crank to start
  float crank = playdate->system->getCrankChange();
  if (crank > 0.5f) {
    menu->progress += game->delta_time / CRANK_DURATION;
  } else {
    menu->progress -= game->delta_time * 3.0f;

    if (menu->progress < 0.0f) {
      menu->progress = 0.0f;
    }
  }
  
  if (menu->progress > 0) {
    playdate->sound->synth->playNote(menu->progress_synth, 466.16f + menu->progress * 261.0f, 0.25f, 0.1f, 0);
  }
    
  
  if (menu->progress > 1.0f) {
    playdate->sound->sampleplayer->play(game->sound_effect, 1, 1.0);
    scene_transition(game->scene_manager, game->song_list_scene);
  }

  // drawing
  playdate->graphics->clear(kColorWhite);
  
  for (int i = 0; i < MENU_NOTES_LENGTH; ++i) {
    MenuNote* note = &menu->bg_notes[i];
    note->x = (note->x + ((float)RNG_Y[note->y] / 80.0f) + 1.5f);
    if (note->x > 500) {
      note->x = 0;
      note->y = (note->y + 1) % 12;
      note->type = (note->type + 1) % 5;
      note->color = (note->color + 1) % 2;
    }
    int y = RNG_Y[note->y] + SIN_VALS[((game->frame + note->sin_offset) / 10) % 11];
    particles_moveEmitter(menu->particles, note->emitter, 420 - note->x, y);
  }
  
  particles_update(menu->particles);
  for (int i = 0; i < MENU_NOTES_LENGTH; ++i) {    
    MenuNote* note = &menu->bg_notes[i];
    int y = RNG_Y[note->y] + SIN_VALS[((game->frame + note->sin_offset) / 10) % 11];
    draw_note(game, 420 - (int)note->x, y, RNG_TYPE[note->type], note->color);
  }

  
  playdate->graphics->setDrawMode(kDrawModeNXOR);
  int on_beat = rhythm_isOnBeat(game->rhythmplayer, 0.4f);
  playdate->graphics->drawBitmap(game->title_bitmaps[on_beat], 200 - (196 / 2), 50, kBitmapUnflipped);
  playdate->graphics->setDrawMode(kDrawModeCopy);
  
  // Crank To Start
  int width = playdate->graphics->getTextWidth(game->basic_font, "CRANK TO START", 16, kASCIIEncoding, 0);
  int height = playdate->graphics->getFontHeight(game->basic_font);
  int left = 200 - (width >> 1);
  int top = 230 - height - ((game->frame / 20) % 2) * 2;
  playdate->graphics->setFont(game->basic_font);
  playdate->graphics->drawText("CRANK TO START", 16, kASCIIEncoding, left, top);
  
  playdate->graphics->setFont(game->font_cuberick);
  playdate->graphics->drawText("v0.0.5", 7, kASCIIEncoding, 400 - 1 - playdate->graphics->getTextWidth(game->font_cuberick, "v0.0.4", 7, kASCIIEncoding, 0), 1);
  
  playdate->graphics->setFont(game->font);

  // Loading bar
  int size = (int)(menu->progress * (width + 6));
  playdate->graphics->fillRect(200 - (width >> 1) - 3, top - 2, size, height + 4, kColorXOR);
}

void menu_on_end(void* game_data, void* menu_data) {
  MenuData* menu = (MenuData*)menu_data;
  
  playdate->sound->synth->freeSynth(menu->progress_synth);
  
  particles_freeSystem(menu->particles);
  // playdate->graphics->freeBitmap(menu->waveform_bitmap);  
}
