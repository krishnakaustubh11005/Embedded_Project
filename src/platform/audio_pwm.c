/* PWM audio with DMA double buffering.
 *
 * PWM as a DAC: the pin switches at 122 kHz and the fraction of time it is
 * high (duty cycle) is the sample value. An RC low-pass filter (or the
 * amplifier and speaker themselves) averages the pulses into a voltage.
 *
 * Timing: a DMA pacing timer fires AUDIO_SAMPLE_RATE times per second. Each
 * tick, DMA copies one level from RAM into the PWM compare register with no
 * CPU involvement. Two buffers form a ping-pong: while channel A plays buffer
 * 0, channel B is armed with buffer 1. When A finishes it triggers B ("chain")
 * and raises an interrupt in which the CPU refills buffer 0. The display's
 * blocking SPI transfer can therefore never cause an audio gap, as long as a
 * refill (well under 1 ms) finishes inside one 11.6 ms block.
 */
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "config.h"
#include "audio/sound.h"
#include "platform/audio.h"

/* 32-bit words: the same level goes in both halves of the CC register (A and B). */
static uint32_t buffers[2][AUDIO_BLOCK_SAMPLES];
static uint16_t levels[AUDIO_BLOCK_SAMPLES];
static int channels[2];
static volatile unsigned int blocks;

/** Synthesize one block and widen it into DMA buffer index. */
static void fill(unsigned int index) {
    sound_render(levels,AUDIO_BLOCK_SAMPLES);
    for (unsigned int i=0;i<AUDIO_BLOCK_SAMPLES;i++) buffers[index][i]=levels[i]|((uint32_t)levels[i]<<16);
    blocks++;
}

/** DMA_IRQ_1 handler: re-arm the finished channel and refill its buffer. */
static void audio_dma_irq(void) {
    for (unsigned int i=0;i<2;i++) {
        if (dma_channel_get_irq1_status((unsigned int)channels[i])) {
            dma_channel_acknowledge_irq1((unsigned int)channels[i]);
            /* Point back at the start of the buffer without starting; the other
             * channel's chain trigger restarts this one in 11.6 ms. */
            dma_channel_set_read_addr((unsigned int)channels[i],buffers[i],false);
            fill(i);
        }
    }
}

/** Configure PWM, pacing timer and the two chained DMA channels; see audio.h. */
void audio_init(void) {
    gpio_set_function(AUDIO_PIN,GPIO_FUNC_PWM);
    unsigned int slice=pwm_gpio_to_slice_num(AUDIO_PIN);
    pwm_config config=pwm_get_default_config();
    pwm_config_set_clkdiv(&config,1.0f);
    pwm_config_set_wrap(&config,AUDIO_PWM_WRAP);
    pwm_init(slice,&config,true);
    pwm_set_gpio_level(AUDIO_PIN,(AUDIO_PWM_WRAP+1u)/2u);  // Silence = 50% duty.

    /* Pacing timer rate = clk_sys * X / Y; with X=1, Y=5668 at 125 MHz = 22 054 Hz. */
    int timer=dma_claim_unused_timer(true);
    dma_timer_set_fraction((unsigned int)timer,1,(uint16_t)(clock_get_hz(clk_sys)/AUDIO_SAMPLE_RATE));
    channels[0]=dma_claim_unused_channel(true);
    channels[1]=dma_claim_unused_channel(true);
    fill(0); fill(1);
    for (unsigned int i=0;i<2;i++) {
        dma_channel_config c=dma_channel_get_default_config((unsigned int)channels[i]);
        channel_config_set_transfer_data_size(&c,DMA_SIZE_32);
        channel_config_set_read_increment(&c,true);
        channel_config_set_write_increment(&c,false);
        channel_config_set_dreq(&c,dma_get_timer_dreq((unsigned int)timer));
        channel_config_set_chain_to(&c,(unsigned int)channels[i^1u]);
        dma_channel_configure((unsigned int)channels[i],&c,&pwm_hw->slice[slice].cc,
                              buffers[i],AUDIO_BLOCK_SAMPLES,false);
        dma_channel_set_irq1_enabled((unsigned int)channels[i],true);
    }
    irq_set_exclusive_handler(DMA_IRQ_1,audio_dma_irq);
    irq_set_enabled(DMA_IRQ_1,true);
    dma_channel_start((unsigned int)channels[0]);
}

/** Return the number of rendered blocks. */
unsigned int audio_blocks(void) { return blocks; }
