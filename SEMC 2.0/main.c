#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/ledc.h"
#include "esp_rom_sys.h"

// ----- LCD -----
#define LCD_RS 23
#define LCD_E  27
#define LCD_D4 25
#define LCD_D5 19
#define LCD_D6 18
#define LCD_D7 4

// ----- LEDS -----
#define led_vermelho 5
#define led_amarelo 21
#define led_verde 22

// ----- BOTAO -----
#define BOTAO 26

// ----- POTENCIOMETRO -----
#define POT ADC1_CHANNEL_6 // Pino 34

// ----- DEVICES -----
#define A 14
#define B 12
#define C 13

// ------ BUZZER ------
#define BUZZER 15

// ----- VARIAVEIS -----
int state = 0; // 0 = NORMAL | 1 = ATTENTION | 2 = CRITICAL

int estado_sistema = 0; // 0 = OFF | 1 = ON

int estado_anterior = 1; // Detecta o clique do botao

// Status dos devices (verdadeiro ou falso)
bool device_A;
bool device_B;
bool device_C;

// ----- LCD BAIXO NIVEL -----
// Controle do LCD direto do hardware

void lcd_pulse_enable(){ // gera pulso no ENABLE

    gpio_set_level(LCD_E, 1);
    esp_rom_delay_us(1);
    gpio_set_level(LCD_E, 0);
    esp_rom_delay_us(100);
}

// envia 4 bits
void lcd_send_nibble(uint8_t nibble){

    gpio_set_level(LCD_D4, (nibble >> 0) & 1);
    gpio_set_level(LCD_D5, (nibble >> 1) & 1);
    gpio_set_level(LCD_D6, (nibble >> 2) & 1);
    gpio_set_level(LCD_D7, (nibble >> 3) & 1);
    lcd_pulse_enable();
}

// Byte completo
void lcd_send_byte(uint8_t data, int rs){

    gpio_set_level(LCD_RS, rs);
    // parte alta
    lcd_send_nibble(data >> 4);
    // parte baixa
    lcd_send_nibble(data & 0x0F);
    esp_rom_delay_us(40);
}

void lcd_init(){ // Inicia o LCD 

    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(LCD_RS, 0);
    lcd_send_nibble(0x03);

    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_send_nibble(0x03);
    esp_rom_delay_us(100);
    lcd_send_nibble(0x03);

    // modo 4 bits
    lcd_send_nibble(0x02);
    // configuração
    lcd_send_byte(0x28, 0);
    // display ON
    lcd_send_byte(0x0C, 0);
    // cursor
    lcd_send_byte(0x06, 0);
    // limpa display
    lcd_send_byte(0x01, 0);

    vTaskDelay(pdMS_TO_TICKS(5));
}

// posiciona cursor
void lcd_set_cursor(int col, int row){

    int addr;
    if(row == 0){
        addr = 0x80;
    }
    else{
        addr = 0xC0;
    }

    addr += col;
    lcd_send_byte(addr, 0);
}

// imprime string
void lcd_print(const char *str){

    while(*str){
        lcd_send_byte(*str++, 1);
    }
}

// limpa linha
void lcd_clear_line(int row){
    lcd_set_cursor(0, row);
    lcd_print("                ");
}

// ----- INFORMACOES NO LCD -----
void atualizar_lcd(int carga){ // atualiza LCD

    lcd_clear_line(0);
    lcd_clear_line(1);

    int devices_ativos =
        device_A +
        device_B +
        device_C;

    // ----- LINHA 1 -----
    lcd_set_cursor(0,0);
    if(state == 0){
        lcd_print("[NORMAL]");
    }
    else if(state == 1){
        lcd_print("[ATTENTION]");
    }
    else{
        lcd_print("[CRITICAL]");
    }

    // ----- LINHA 2 -----
    lcd_set_cursor(0,1);
    char buffer[16];
 
    // D = Devices | C = Carga
    sprintf(buffer, "D: %d | C:%3d%%", devices_ativos, carga);
    lcd_print(buffer);
}

// ----- BUZZER -----
// liga buzzer
void buzzer_on(){ 

    ledc_set_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,128);
    ledc_update_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0);
}

// desliga buzzer
void buzzer_off(){

    ledc_set_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0);
}

// Beep em modo de atencao
void attention_buzzer(){

    buzzer_on();
    vTaskDelay(pdMS_TO_TICKS(500));
    buzzer_off();
}

// ----- ESTADOS -----
// estado normal
void ABC_normal(){

    device_A = true;
    device_B = true;
    device_C = true;

    // LEDs
    gpio_set_level(led_vermelho, 0);
    gpio_set_level(led_amarelo, 0);
    gpio_set_level(led_verde, 1);

    // Devices
    gpio_set_level(A, 1);
    gpio_set_level(B, 1);
    gpio_set_level(C, 1);
}

// estado attention
void ABC_atencao(){

    device_A = true;
    device_B = true;
    device_C = false;

    //LEDs
    gpio_set_level(led_vermelho, 0);
    gpio_set_level(led_amarelo, 1);
    gpio_set_level(led_verde, 0);

    //Devices
    gpio_set_level(A, 1);
    gpio_set_level(B, 1);
    gpio_set_level(C, 0);
}

// estado critical
void ABC_critico(){

    device_A = true;
    device_B = false;
    device_C = false;

    //LEDs
    gpio_set_level(led_vermelho, 1);
    gpio_set_level(led_amarelo, 0);
    gpio_set_level(led_verde, 0);

    //Devices
    gpio_set_level(A, 1);
    gpio_set_level(B, 0);
    gpio_set_level(C, 0);
}

// ----- BOTAO -----
// liga/desliga sistema
void ler_botao(){

    int estado_atual = gpio_get_level(BOTAO);

    // detecta clique
    if(estado_anterior == 1 && estado_atual == 0){

        estado_sistema = !estado_sistema;
        if(estado_sistema){
            printf("SISTEMA ATIVADO\n");
        }
        else{
            printf("SISTEMA DESATIVADO\n");
        }

        // debounce
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    estado_anterior = estado_atual;
}

// ----- app_main -----
void app_main(){

    // ----- GPIOs -----
    gpio_set_direction(BOTAO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BOTAO, GPIO_PULLUP_ONLY);

    gpio_set_direction(led_vermelho, GPIO_MODE_OUTPUT);
    gpio_set_direction(led_amarelo, GPIO_MODE_OUTPUT);
    gpio_set_direction(led_verde, GPIO_MODE_OUTPUT);

    gpio_set_direction(A, GPIO_MODE_OUTPUT);
    gpio_set_direction(B, GPIO_MODE_OUTPUT);
    gpio_set_direction(C, GPIO_MODE_OUTPUT);

    //LCD
    gpio_set_direction(LCD_RS, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_E, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_D4, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_D5, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_D6, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_D7, GPIO_MODE_OUTPUT);

    //POTENCIOMETRO

    // faixa: 0 -> 4095
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(POT,ADC_ATTEN_DB_11);

    //BUZZER
    ledc_timer_config_t timer = {

        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 2000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t canal = {

        .gpio_num = BUZZER,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0
    };
    ledc_channel_config(&canal);

    // ----- inicializacao do LCD -----
    estado_anterior = gpio_get_level(BOTAO);
    lcd_init();

    // ----- While(true) -----
    while(true){

        ler_botao();

        // ----- SISTEMA LIGADO -----
        if(estado_sistema == 1){

            // le a carga consumida (potenciometro)
            int valor_adc = adc1_get_raw(POT);

            // converte para porcentagem
            int carga = (valor_adc * 100) / 4095;

            // ----- MAQUINA DE ESTADOS -----
            if(carga < 40){
                state = 0;
            }
            else if(carga < 80){
                state = 1;
            }
            else{
                state = 2;
            }

            // ----- NORMAL -----
            if(state == 0){
                printf("[ NORMAL ] | Carga: %3d%% | Devices: 3\n", carga);
                ABC_normal();
                buzzer_off();
            }

            // ----- ATTENTION -----
            else if(state == 1){
                printf("[ ATTENTION ] | Carga: %3d%% | Devices: 2\n", carga);
                ABC_atencao();
                attention_buzzer();
            }

            // ----- CRITICAL -----
            else{
                printf("[ CRITICAL ] | Carga: %3d%% | Devices: 1\n", carga);
                ABC_critico();
                buzzer_on();
            }

            // atualiza LCD
            atualizar_lcd(carga);
        }

        // ----- SISTEMA DESLIGADO -----
        else{

            // desliga LEDs
            gpio_set_level(led_vermelho, 0);
            gpio_set_level(led_amarelo, 0);
            gpio_set_level(led_verde, 0);

            // desliga devices
            gpio_set_level(A, 0);
            gpio_set_level(B, 0);
            gpio_set_level(C, 0);

            // desliga buzzer
            buzzer_off();

            // limpa LCD
            lcd_clear_line(0);
            lcd_clear_line(1);

            lcd_set_cursor(1,0);
            lcd_print("[ SYSTEM OFF ]");
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}