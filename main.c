/*! =========================================================================
 *
 *  Copyright(c) 2025 JV Gomes, EmbarcaTech - All rights reserved. 
 *  
 *  Sintetizador de Audio
 *  @brief    O programa realiza à captura, armazenamento, processamento e 
 *            reprodução de áudio digital.
 *
 *  @file	    main.c
 *  @author   Joao Vitor G. de Oliveira
 *  @date	    2 Jun 2025
 *  @version  1.0
 * 
============================================================================ */

/* ============================   LIBRARIES  =============================== */
#include <stdio.h> // Biblioteca padrão
#include <stdlib.h>
#include <math.h> // Biblioteca de matemática (função "round" foi utilizada)

#include "pico/stdlib.h" // Biblioteca padrão pico
#include "hardware/gpio.h" // Biblioteca de GPIOs
#include "hardware/adc.h" // Biblioteca do ADC
#include "hardware/pwm.h" // Biblioteca do PWM
#include "hardware/timer.h" // Biblioteca de Timmer

/* =============================   MACROS   ================================ */

#define LED_GREEN_PIN 11 //Pinagem LED Verde onboard Bitdog
#define LED_RED_PIN 13 //Pinagem LED Vermelho onboard Bitdog

#define BUZZER_LEFT 10 //Pinagem Buzzer esquerdo onboard Bitdog
#define BUZZER_RIGHT 21 //Pinagem Buzzer direito onboard Bitdog

#define BTN_A_PIN 5 //Pinagem botão A onboard Bitdog
#define BTN_B_PIN 6 //Pinagem botão B onboard Bitdog

#define MIC_PIN 28 //Pinagem microfone onboard Bitdog

#define AUDIO_DURATION_SEC 5
#define SAMPLE_RATE 16000 // ou 22050
#define MAX_SAMPLES (AUDIO_DURATION_SEC * SAMPLE_RATE)

/* =========================   GLOBAL VARIABLES   ========================== */

uint16_t audio_buffer[MAX_SAMPLES];
volatile bool btn_a_pressed = false;
volatile bool btn_b_pressed = false;


/* ========================   FUNCTION PROTOTYPE   ========================= */

void gpio_start(void); // Mapeamento de hardware
void adc_setup(void); // Inicializacao ADC
void pwm_setup(uint buzzer_pin); 
void play_sample(uint buzzer_pin, uint16_t value);
void grava_audio(void);
void reproduz_audio(void);
uint16_t low_pass_filter(uint16_t current_sample, uint16_t prev_sample, float alpha);
void normalize_audio(uint16_t *buffer, int len);
uint16_t moving_average_filter(uint16_t *buffer, int index, int window);

/* ===========================   MAIN FUNCTION   =========================== */
int main(void)
{
  stdio_init_all();
  gpio_start();
  adc_setup();
  pwm_setup(BUZZER_LEFT);
  pwm_setup(BUZZER_RIGHT);

  while (true) 
  {
    if (!gpio_get(BTN_A_PIN)) 
    {
      sleep_ms(200); // debounce
      grava_audio();
    }

    if (!gpio_get(BTN_B_PIN)) 
    {
      sleep_ms(200); // debounce
      reproduz_audio();
    }
  }

}/* end main */
/* =======================  DEVELOPMENT OF FUNCTIONS ======================= */

/*! ---------------------------------------------------------------------------
 *  @brief  Configuracao e inicializacao das portas de entrada e saida de uso
 *          geral (GPIOs) do microcontrolador
 *
 *  @param none
 *  @return void
 * 
 ----------------------------------------------------------------------------*/
 void gpio_start(void)
 {
    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);

    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);

    gpio_init(BTN_A_PIN);
    gpio_set_dir(BTN_A_PIN, GPIO_IN);
    gpio_pull_up(BTN_A_PIN);

    gpio_init(BTN_B_PIN);
    gpio_set_dir(BTN_B_PIN, GPIO_IN);
    gpio_pull_up(BTN_B_PIN);

    gpio_init(BUZZER_LEFT);
    gpio_set_function(BUZZER_LEFT, GPIO_FUNC_PWM);

    gpio_init(BUZZER_RIGHT);
    gpio_set_function(BUZZER_RIGHT, GPIO_FUNC_PWM);
 }

 /*! ---------------------------------------------------------------------------
 *  @brief  Configuracao do periferico do adc
 *
 *  @param none
 *  @return void
 * 
 ----------------------------------------------------------------------------*/
 void adc_setup(void) 
 {
    adc_init();
    adc_gpio_init(MIC_PIN);
    adc_select_input(2); // GPIO 28 é canal 2
}

 /*! ---------------------------------------------------------------------------
 *  @brief  Configuracao do pwm
 *
 *  @param buzzer_pin   pino de saida de sinal pwm
 *  @return void
 * 
 ----------------------------------------------------------------------------*/
void pwm_setup(uint buzzer_pin) 
{
  uint slice_num = pwm_gpio_to_slice_num(buzzer_pin);
  pwm_config config = pwm_get_default_config();

  pwm_config_set_clkdiv(&config, 1.0f); // PWM rápido
  pwm_init(slice_num, &config, true);
  pwm_set_wrap(slice_num, 1023); // 10 bits: equilíbrio entre resolução e velocidade
  pwm_set_chan_level(slice_num, pwm_gpio_to_channel(buzzer_pin), 0);

}

 /*! ---------------------------------------------------------------------------
 *  @brief  Funcao de auxilio para reproducao da sample
 *
 *  @param none
 *  @return void
 * 
 ----------------------------------------------------------------------------*/
void play_sample(uint buzzer_pin, uint16_t value)
{
  uint slice = pwm_gpio_to_slice_num(buzzer_pin);
  pwm_set_chan_level(slice, pwm_gpio_to_channel(buzzer_pin), value);
}

 /*! ---------------------------------------------------------------------------
 *  @brief  Processo de gravacao de audio
 *
 *  @param none
 *  @return void
 * 
 ----------------------------------------------------------------------------*/
void grava_audio(void) 
{
  gpio_put(LED_RED_PIN, 1);

  uint16_t prev = 0;
  float alpha = 0.1f; // Constante do filtro passa-baixa

  for (int i = 0; i < MAX_SAMPLES; i++) 
  {
    uint16_t sample = adc_read();
    prev = low_pass_filter(sample, prev, alpha); // Aplica filtro
    audio_buffer[i] = prev;
    sleep_us(1000000 / SAMPLE_RATE);
  }

  normalize_audio(audio_buffer, MAX_SAMPLES); // Ajusta o volume

  gpio_put(LED_RED_PIN, 0);
}

 /*! ---------------------------------------------------------------------------
 *  @brief  Processo de reproducao de audio
 *
 *  @param none
 *  @return void
 * 
 ----------------------------------------------------------------------------*/
void reproduz_audio(void) 
{
  gpio_put(LED_GREEN_PIN, 1);

  for (int i = 0; i < MAX_SAMPLES; i++) 
  {
    play_sample(BUZZER_LEFT, audio_buffer[i]);
    play_sample(BUZZER_RIGHT, audio_buffer[i]);
    sleep_us(1000000 / SAMPLE_RATE);
  }

  // Desliga os buzzers ao final
  play_sample(BUZZER_LEFT, 0);
  play_sample(BUZZER_RIGHT, 0);

  gpio_put(LED_GREEN_PIN, 0);
}

// Filtro passa-baixa simples
uint16_t low_pass_filter(uint16_t current_sample, uint16_t prev_sample, float alpha)
{
    return (uint16_t)(alpha * current_sample + (1.0f - alpha) * prev_sample);
}

// Normalização de volume
void normalize_audio(uint16_t *buffer, int len)
{
    uint16_t max_val = 0;
    for (int i = 0; i < len; i++) 
    {
        if (buffer[i] > max_val) max_val = buffer[i];
    }

    if (max_val == 0) return; // Evita divisão por zero

    for (int i = 0; i < len; i++) 
    {
        buffer[i] = (buffer[i] * 1023) / max_val; // Escala para 12 bits
    }
}

uint16_t moving_average_filter(uint16_t *buffer, int index, int window) 
{
  int sum = 0;
  int count = 0;
  for (int i = index - window; i <= index + window; i++) 
  {
    if (i >= 0 && i < MAX_SAMPLES) 
    {
      sum += buffer[i];
      count++;
    }
  }
  return (uint16_t)(sum / count);
}
/* end program */
