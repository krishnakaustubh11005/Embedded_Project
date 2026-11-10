/* Tiny chiptune-style synthesizer and music sequencer.
 *
 * Every effect is a short list of "tones": a waveform whose pitch and volume
 * slide linearly over a few milliseconds. A punch is a burst of noise then a
 * falling sine "thud"; a menu pop is a sine sweeping upward. This costs a few
 * bytes of flash per sound, unlike recorded audio (about 22 KB per second).
 *
 * Mixing works in chunks of CHUNK samples: pitch and volume are recalculated
 * once per chunk ("control rate", about 690 Hz) and the inner loop only adds a
 * phase increment and looks up the waveform ("audio rate", 22 050 Hz).
 */
#include <math.h>
#include <stdatomic.h>
#include <string.h>
#include "config.h"
#include "audio/sound.h"
#include "audio/clips.h"

#define CHUNK 32u
#define MAX_TONES 5u
#define MUSIC_VOICES 3u
#define VOICES (AUDIO_SFX_VOICES + MUSIC_VOICES)
#define QUEUE_SIZE 16u
#define CMD_MUSIC 0x40u
#define CMD_PAUSE 0x60u
#define CMD_RESUME 0x61u
#define MIDDLE ((int32_t)(AUDIO_PWM_WRAP + 1u) / 2)
/* Phase is a 32-bit fraction of one waveform cycle: 2^32 / rate per hertz. */
#define INC_PER_HZ ((uint32_t)(4294967296.0 / AUDIO_SAMPLE_RATE))
#define STEP_SAMPLES (AUDIO_SAMPLE_RATE * 60u / (AUDIO_MUSIC_BPM * 4u))  // One 16th note.
#define RETRIGGER_SAMPLES (AUDIO_SAMPLE_RATE / 33u)                       // 30 ms.
#define END 0xFFu

typedef enum { W_SQUARE, W_PULSE, W_TRI, W_SAW, W_SINE, W_NOISE } Wave;
/* wave, start/end pitch in Hz (noise: random updates per second), ms, start/end volume. */
typedef struct { uint8_t wave; uint16_t f0, f1, ms; uint8_t v0, v1; } Tone;

typedef struct {
    uint8_t id, count, seg;
    Tone tone[MAX_TONES];
    const Clip *clip;              // Non-NULL: play recorded PCM instead of tones.
    uint32_t pos, len, phase, inc, age;
    int32_t amp;
    uint16_t lfsr;
    int8_t noise;
} Voice;

typedef struct { const uint8_t *events; unsigned int index; int32_t remaining; } Track;

/* ---- Effect designs (frequencies in Hz, times in ms, volumes 0..255) ---- */
static const Tone effects[SND_COUNT][MAX_TONES]={
    [SND_MENU_MOVE]  ={{W_TRI,1100,1500,28,150,0}},
    [SND_MENU_SELECT]={{W_SQUARE,660,660,45,110,90},{W_SQUARE,990,990,90,110,0}},
    [SND_MENU_BACK]  ={{W_TRI,760,330,100,160,0}},
    /* "Pop over the game": bubble pop, then a rising C-E-G sparkle. */
    [SND_GAME_POP]   ={{W_SINE,250,1500,70,230,140},{W_PULSE,1047,1047,55,110,80},
                       {W_PULSE,1319,1319,55,110,80},{W_PULSE,1568,1568,160,120,0}},
    [SND_PAUSE]      ={{W_TRI,880,440,130,160,0}},
    [SND_RESUME]     ={{W_TRI,440,880,130,160,0}},
    [SND_FANFARE]    ={{W_SQUARE,523,523,110,110,90},{W_SQUARE,659,659,110,110,90},
                       {W_SQUARE,784,784,110,110,90},{W_SQUARE,1047,1047,450,130,0}},
    [SND_GAME_OVER]  ={{W_SQUARE,392,392,180,110,90},{W_SQUARE,330,330,180,110,90},
                       {W_SQUARE,262,196,520,120,0}},
    [SND_SNAKE_TURN] ={{W_TRI,620,620,14,70,0}},
    [SND_SNAKE_EAT]  ={{W_SQUARE,700,1500,60,120,60},{W_SINE,1760,2100,60,110,0}},
    [SND_SNAKE_DIE]  ={{W_NOISE,9000,2000,90,200,90},{W_SQUARE,420,60,420,140,0}},
    [SND_PONG_PADDLE]={{W_SQUARE,490,490,40,150,0}},
    [SND_PONG_WALL]  ={{W_SQUARE,245,245,35,130,0}},
    [SND_PONG_SCORE] ={{W_SQUARE,262,262,90,120,100},{W_SQUARE,392,392,200,120,0}},
    [SND_SOCCER_SELECT]={{W_TRI,1000,1000,25,120,0}},
    [SND_SOCCER_POWER] ={{W_PULSE,500,950,70,110,0}},
    [SND_SOCCER_SHOT]  ={{W_NOISE,12000,4000,35,190,0},{W_SINE,240,120,70,180,0}},
    [SND_SOCCER_CLACK] ={{W_NOISE,20000,20000,10,150,0},{W_TRI,1800,1500,25,100,0}},
    /* Referee trill, then a crowd-like noise swell. */
    [SND_SOCCER_GOAL]  ={{W_SINE,2200,2450,80,150,150},{W_SINE,2200,2450,80,150,150},
                         {W_SINE,2300,2300,220,150,0},{W_NOISE,3500,2500,150,40,150},
                         {W_NOISE,2500,1500,650,150,0}},
    [SND_SOCCER_TURN]  ={{W_SINE,2300,2300,120,120,0}},
    [SND_DUEL_WHOOSH]  ={{W_NOISE,4000,12000,60,20,120},{W_NOISE,12000,5000,80,120,0}},
    /* Brazilian kick: a blade "shing" then a long swish. */
    [SND_DUEL_SPECIAL] ={{W_SAW,1800,3600,70,80,80},{W_NOISE,6000,16000,90,50,150},
                         {W_NOISE,16000,4000,170,150,0}},
    [SND_DUEL_JUMP]    ={{W_TRI,300,750,90,120,0}},
    [SND_DUEL_DENIED]  ={{W_SAW,150,140,90,110,110},{W_SAW,140,130,90,110,0}},
    [SND_DUEL_PUNCH_HIT]={{W_NOISE,14000,5000,25,220,110},{W_SINE,200,90,70,210,0}},
    [SND_DUEL_KICK_HIT] ={{W_NOISE,11000,3500,35,220,130},{W_SINE,160,70,110,230,0}},
    [SND_DUEL_HEAVY_HIT]={{W_NOISE,16000,2500,50,255,190},{W_SQUARE,130,50,190,190,0}},
    [SND_DUEL_BLOCK]   ={{W_SQUARE,1900,1900,20,120,60},{W_TRI,2500,2350,90,110,0}},
    /* Taiko hit, then a gong that rings out. */
    [SND_DUEL_ROUND]   ={{W_NOISE,3000,1000,25,180,120},{W_SINE,170,85,200,250,40},
                         {W_TRI,220,214,950,140,0}},
    [SND_DUEL_FIGHT]   ={{W_NOISE,9000,9000,25,160,0},{W_SAW,520,1040,90,150,140},
                         {W_SQUARE,1040,1040,170,140,0}},
    [SND_DUEL_KO]      ={{W_SINE,180,45,320,255,120},{W_TRI,262,98,800,160,0}},
};

/* ---- "Shadow of the Dojo": original 8-bar loop in the Japanese In scale (D Eb G A Bb) ----
 * Pairs of (MIDI note or 0 for rest, length in 16th notes); END loops. */
static const uint8_t lead_notes[]={
    74,4, 69,2, 70,2, 69,4, 67,4,          75,2, 74,2, 69,4, 70,2, 69,2, 67,2, 69,2,
    74,4, 75,2, 74,2, 69,2, 67,2, 63,2, 67,2, 62,6, 0,2, 69,2, 70,2, 74,4,
    79,4, 75,2, 74,2, 70,4, 69,4,          70,2, 69,2, 67,2, 63,2, 62,8,
    74,2, 74,2, 75,2, 74,2, 69,2, 70,2, 69,2, 67,2,  69,4, 63,2, 67,2, 62,8, END};
#define BASS_D 50,4, 50,2, 62,2, 57,4, 58,2, 57,2
#define BASS_G 55,4, 55,2, 67,2, 62,4, 63,2, 62,2
static const uint8_t bass_notes[]={BASS_D,BASS_D,BASS_D,BASS_D,BASS_G,BASS_G,BASS_D,BASS_D,END};
/* Drums: 1 big taiko "don", 2 small drum, 3 wooden "ka" click, 4 cymbal swish. */
#define GROOVE 1,3, 3,1, 1,2, 3,2, 1,2, 2,2, 3,1, 3,1, 1,2
#define FILL 1,2, 2,2, 1,2, 2,2, 1,1, 1,1, 2,1, 2,1, 1,1, 2,1, 1,1, 4,1
static const uint8_t drum_hits[]={GROOVE,GROOVE,GROOVE,GROOVE,GROOVE,GROOVE,GROOVE,FILL,END};
static const Tone drum_tones[5][2]={
    [1]={{W_NOISE,2500,800,15,130,90},{W_SINE,190,90,190,175,0}},
    [2]={{W_SINE,300,180,90,130,0}},
    [3]={{W_NOISE,18000,18000,22,95,0}},
    [4]={{W_NOISE,22000,20000,260,55,0}}};

/* ---- Shared state ---- */
static volatile uint8_t queue[QUEUE_SIZE];
static volatile uint8_t queue_head, queue_tail;      // Main loop writes head; IRQ writes tail.
static volatile uint8_t duel_theme;
static volatile bool muted;
static MusicId requested_music;                      // Main-loop copy for menus and tests.
static uint32_t play_counts[SND_COUNT];

/* Interrupt-side state: only sound_render touches these after sound_init. */
static Voice voices[VOICES];
static Track tracks[MUSIC_VOICES];
static bool music_on, music_paused;
static int8_t sine_table[64];
static uint16_t note_hz[128];

/** Fill the sine and MIDI-note tables using floating point once at start-up. */
void sound_init(void) {
    memset(voices,0,sizeof(voices)); memset(tracks,0,sizeof(tracks));
    memset(play_counts,0,sizeof(play_counts));
    queue_head=queue_tail=0; music_on=music_paused=false; requested_music=MUSIC_NONE;
    for (unsigned int i=0;i<64;i++) sine_table[i]=(int8_t)lrintf(120.0f*sinf((float)i*6.2831853f/64.0f));
    for (unsigned int n=0;n<128;n++) note_hz[n]=(uint16_t)lrintf(440.0f*powf(2.0f,((float)n-69.0f)/12.0f));
    for (unsigned int v=0;v<VOICES;v++) voices[v].lfsr=(uint16_t)(0xACE1u+v*977u);
}

/** Push one command byte for the interrupt; drop it if the ring is full. */
static void push(uint8_t command) {
    uint8_t head=queue_head, next=(uint8_t)((head+1u)%QUEUE_SIZE);
    if (next==queue_tail) return;
    queue[head]=command;
    atomic_signal_fence(memory_order_release);  // Store the byte before publishing it.
    queue_head=next;
}

/** Queue a one-shot effect and count it for debugging. */
void sound_play(SoundId id) {
    if (id<=SND_NONE || id>=SND_COUNT) return;
    play_counts[id]++;
    if (!muted) push((uint8_t)id);
}

/** Queue a music change; the interrupt restarts the loop from bar one. */
void sound_music(MusicId id) {
    if (id>=MUSIC_COUNT) return;
    requested_music=id;
    push((uint8_t)(CMD_MUSIC+id));
}

/** Queue a pause/continue for the music loop. */
void sound_music_pause(bool paused) { push(paused?CMD_PAUSE:CMD_RESUME); }

/** Store the theme; the interrupt reads it when a hit sound starts. */
void sound_set_duel_theme(DuelTheme theme) { if (theme<DUEL_THEME_COUNT) duel_theme=(uint8_t)theme; }

/** Return the selected Shade Duel theme. */
DuelTheme sound_duel_theme(void) { return (DuelTheme)duel_theme; }

/** Return a constant theme label for menus. */
const char *sound_theme_name(DuelTheme theme) { return theme==DUEL_THEME_CLIPS?"CLIPS":"NINJA"; }

/** Mute or unmute the speaker output. */
void sound_set_muted(bool value) { muted=value; }

/** Return whether output is muted. */
bool sound_muted(void) { return muted; }

/** Return the music most recently requested. */
MusicId sound_current_music(void) { return requested_music; }

/** Return the request counter for id. */
uint32_t sound_play_count(SoundId id) { return id<SND_COUNT?play_counts[id]:0; }

/** Begin segment seg of a voice; frees the voice after its last segment. */
static void begin_segment(Voice *v, unsigned int seg) {
    v->seg=(uint8_t)seg; v->pos=0;
    if (seg>=v->count) { v->id=SND_NONE; v->count=0; return; }
    v->len=v->tone[seg].ms*AUDIO_SAMPLE_RATE/1000u;
    if (v->len==0) v->len=1;
}

/** Start tones (or a clip) on a voice, resetting its phase and age. */
static void start_voice(Voice *v, uint8_t id, const Tone *tones, unsigned int count, const Clip *clip) {
    v->id=id; v->clip=clip; v->age=0; v->phase=0;
    if (clip) { v->count=1; v->seg=0; v->pos=0; v->len=clip->length; v->amp=255; return; }
    unsigned int n=0;
    while (n<count && n<MAX_TONES && tones[n].ms>0) { v->tone[n]=tones[n]; n++; }
    v->count=(uint8_t)n;
    begin_segment(v,0);
}

/** Recorded clip that replaces a synthesized hit in the CLIPS theme, else NULL. */
static const Clip *theme_clip(unsigned int id) {
    if (duel_theme!=DUEL_THEME_CLIPS) return NULL;
    if (id==SND_DUEL_PUNCH_HIT) return &clip_punch1;
    if (id==SND_DUEL_KICK_HIT) return &clip_kick;
    if (id==SND_DUEL_HEAVY_HIT) return &clip_punch2;
    return NULL;
}

/** Choose a voice for effect id: retrigger the same effect, else free, else oldest. */
static void trigger_effect(unsigned int id) {
    Voice *chosen=NULL;
    for (unsigned int i=0;i<AUDIO_SFX_VOICES;i++) {
        if (voices[i].id==id) {
            if (voices[i].age<RETRIGGER_SAMPLES) return;  // Rapid repeats would only buzz.
            chosen=&voices[i]; break;
        }
    }
    for (unsigned int i=0;!chosen && i<AUDIO_SFX_VOICES;i++) if (voices[i].id==SND_NONE) chosen=&voices[i];
    if (!chosen) {
        chosen=&voices[0];
        for (unsigned int i=1;i<AUDIO_SFX_VOICES;i++) if (voices[i].age>chosen->age) chosen=&voices[i];
    }
    start_voice(chosen,(uint8_t)id,effects[id],MAX_TONES,theme_clip(id));
}

/** Apply every queued command; runs at the start of each rendered chunk. */
static void drain_queue(void) {
    while (queue_tail!=queue_head) {
        atomic_signal_fence(memory_order_acquire);
        uint8_t command=queue[queue_tail];
        queue_tail=(uint8_t)((queue_tail+1u)%QUEUE_SIZE);
        if (command<SND_COUNT) { if (!muted) trigger_effect(command); }
        else if (command==CMD_PAUSE) music_paused=true;
        else if (command==CMD_RESUME) music_paused=false;
        else if (command>=CMD_MUSIC && command<CMD_MUSIC+MUSIC_COUNT) {
            music_on=command!=CMD_MUSIC+MUSIC_NONE; music_paused=false;
            static const uint8_t *const sheets[MUSIC_VOICES]={lead_notes,bass_notes,drum_hits};
            for (unsigned int t=0;t<MUSIC_VOICES;t++) {
                tracks[t]=(Track){sheets[t],0,0};
                voices[AUDIO_SFX_VOICES+t].id=SND_NONE;
            }
        }
    }
}

/** Start the next note of track t on its dedicated music voice. */
static void play_event(unsigned int t, uint8_t value, uint32_t length_steps) {
    Voice *v=&voices[AUDIO_SFX_VOICES+t];
    if (value==0) return;  // Rest: let the previous note ring out.
    uint16_t ms=(uint16_t)(length_steps*STEP_SAMPLES*1000u/AUDIO_SAMPLE_RATE);
    if (t==2) { if (value<5) start_voice(v,SND_COUNT,drum_tones[value],2,NULL); return; }
    uint16_t hz=note_hz[value&0x7Fu];
    if (t==0) {  // Koto-like pluck: bright saw attack, mellow triangle decay.
        uint16_t decay=(uint16_t)(ms+120u>700u?700u:ms+120u);
        Tone pluck[2]={{W_SAW,hz,hz,20,75,55},{W_TRI,hz,hz,decay,115,0}};
        start_voice(v,SND_COUNT,pluck,2,NULL);
    } else {     // Soft square bass under the melody.
        Tone bass[1]={{W_SQUARE,hz,hz,(uint16_t)(ms>12u?ms-10u:ms),36,18}};
        start_voice(v,SND_COUNT,bass,1,NULL);
    }
}

/** Advance the three music tracks by n samples, starting notes that fall due. */
static void sequence(unsigned int n) {
    if (!music_on || music_paused) return;
    for (unsigned int t=0;t<MUSIC_VOICES;t++) {
        Track *track=&tracks[t];
        track->remaining-=(int32_t)n;
        for (unsigned int guard=0;track->remaining<=0 && guard<4;guard++) {
            if (track->events[track->index]==END) track->index=0;
            uint8_t value=track->events[track->index], steps=track->events[track->index+1];
            track->index+=2;
            play_event(t,value,steps);
            track->remaining+=(int32_t)(steps*STEP_SAMPLES);
        }
    }
}

/** One waveform sample in -127..127 for a voice whose phase has just advanced. */
static int32_t wave_sample(Voice *v, uint8_t wave, uint32_t old_phase) {
    uint32_t p=v->phase;
    switch (wave) {
        case W_SQUARE: return p<0x80000000u?90:-90;
        case W_PULSE:  return p<0x40000000u?90:-90;
        case W_TRI: { int32_t t=(int32_t)(p>>23); return (t<256?t:511-t)-128; }
        case W_SAW:    return (int32_t)(p>>24)-128;
        case W_SINE:   return sine_table[p>>26];
        default:       // Noise: pick a new random level each time the phase wraps.
            if (p<old_phase) {
                v->lfsr=(uint16_t)((v->lfsr>>1)^(-(v->lfsr&1u)&0xB400u));
                v->noise=(int8_t)((v->lfsr&0xFFu)-128);
            }
            return v->noise;
    }
}

/** Add one voice's next n samples into the mix accumulators. */
static void mix_voice(Voice *v, int32_t *mix, unsigned int n) {
    if (v->clip) {
        for (unsigned int i=0;i<n && v->pos<v->len;i++,v->pos++) mix[i]+=v->clip->data[v->pos]*255;
        v->age+=n;
        if (v->pos>=v->len) v->id=SND_NONE;
        return;
    }
    const Tone *t=&v->tone[v->seg];
    int32_t span=(int32_t)v->len, at=(int32_t)v->pos;
    int32_t hz=t->f0+((int32_t)t->f1-(int32_t)t->f0)*at/span;
    v->amp=t->v0+((int32_t)t->v1-(int32_t)t->v0)*at/span;
    v->inc=(uint32_t)hz*INC_PER_HZ;
    for (unsigned int i=0;i<n;i++) {
        uint32_t old=v->phase;
        v->phase+=v->inc;
        mix[i]+=wave_sample(v,t->wave,old)*v->amp;
    }
    v->age+=n;
    v->pos+=n;
    if (v->pos>=v->len) begin_segment(v,v->seg+1u);
}

/** Render count PWM levels; see sound.h. */
void sound_render(uint16_t *levels, unsigned int count) {
    while (count>0) {
        unsigned int n=count<CHUNK?count:CHUNK;
        int32_t mix[CHUNK]={0};
        drain_queue();
        sequence(n);
        if (!muted) {
            for (unsigned int v=0;v<VOICES;v++) {
                if (voices[v].id==SND_NONE) continue;
                if (v>=AUDIO_SFX_VOICES && music_paused) continue;
                mix_voice(&voices[v],mix,n);
            }
        }
        for (unsigned int i=0;i<n;i++) {
            int32_t value=(mix[i]/128)*(int32_t)AUDIO_VOLUME/256;   // One loud voice is about half scale.
            if (value>MIDDLE-1) value=MIDDLE-1;
            if (value< -MIDDLE) value=-MIDDLE;
            levels[i]=(uint16_t)(MIDDLE+value);
        }
        levels+=n; count-=n;
    }
}
