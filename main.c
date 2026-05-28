#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/ledc.h"
#include "esp_rom_sys.h"

// ---------------- LCD ----------------
#define LCD_RS 23
#define LCD_E  27
#define LCD_D4 25
#define LCD_D5 19
#define LCD_D6 18
#define LCD_D7 4

// ---------------- LEDS ----------------
#define led_vermelho 5
#define led_amarelo 21
#define led_verde 22

// ---------------- BOTAO ----------------
#define BOTAO 26

// ---------------- POT ----------------
#define POT ADC1_CHANNEL_6 // Pino 34

// ---------------- SAIDAS ----------------
#define A 14
#define B 12
#define C 13

// ---------------- BUZZER ----------------
#define BUZZER 15

// ---------------- DHT22 ----------------
#define DHT_PIN GPIO_NUM_32

// ----- ESTADO DO SISTEMA -----
int state = 0;              // 0 normal | 1 attention | 2 critical
int estado_sistema = 0;     // 0 off | 1 on
int estado_anterior = 1;    // debounce botão

// ---------------- SENSORES ----------------
float temperature = 0;
float humidity = 0;

// ---------------- DHT22 ----------------
static int wait_level(int level, int timeout){
    while(timeout-- > 0){
        if(gpio_get_level(DHT_PIN) == level) return 1;
        esp_rom_delay_us(1);
    }
    return 0;
}

static int read_bit(){
    wait_level(0, 100);
    wait_level(1, 100);
    esp_rom_delay_us(40);
    return gpio_get_level(DHT_PIN);
}

int dht_read(){
    uint8_t data[5] = {0};

    gpio_set_direction(DHT_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(DHT_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(20));

    gpio_set_level(DHT_PIN, 1);
    esp_rom_delay_us(30);

    gpio_set_direction(DHT_PIN, GPIO_MODE_INPUT);

    if(!wait_level(0,100) || !wait_level(1,100) || !wait_level(0,100))
        return -1;

    for(int i = 0; i < 40; i++){
        data[i / 8] = (data[i / 8] << 1) | read_bit();
    }

    humidity = ((data[0] << 8) | data[1]) * 0.1f;
    temperature = ((data[2] << 8) | data[3]) * 0.1f;

    return 0;
}

// ---------------- LCD ----------------
// Gera pulso no enable (LCD lê os dados aqui)
void lcd_pulse_enable(){
    gpio_set_level(LCD_E, 1);
    esp_rom_delay_us(1);
    gpio_set_level(LCD_E, 0);
    esp_rom_delay_us(100);
}

// Envia 4 bits para o LCD (modo 4 bits)
void lcd_send_nibble(uint8_t nibble){

    // Define os níveis dos pinos D4-D7
    gpio_set_level(LCD_D4, (nibble >> 0) & 1);
    gpio_set_level(LCD_D5, (nibble >> 1) & 1);
    gpio_set_level(LCD_D6, (nibble >> 2) & 1);
    gpio_set_level(LCD_D7, (nibble >> 3) & 1);

    // Pulso de enable para capturar os dados
    lcd_pulse_enable();
}

// Envia 1 byte ao LCD (comando ou caractere)
void lcd_send_byte(uint8_t data, int rs){
    // Seleciona modo: comando (0) ou dados (1)
    gpio_set_level(LCD_RS, rs);

    // Envia parte alta do byte
    lcd_send_nibble(data >> 4);

    // Envia parte baixa do byte
    lcd_send_nibble(data & 0x0F);

    esp_rom_delay_us(40);
}

// Inicialização do LCD (sequência obrigatória 4 bits)
void lcd_init(){

    vTaskDelay(pdMS_TO_TICKS(50)); // estabilização

    gpio_set_level(LCD_RS, 0);

    lcd_send_nibble(0x03);
    vTaskDelay(pdMS_TO_TICKS(5));

    lcd_send_nibble(0x03);
    esp_rom_delay_us(100);

    lcd_send_nibble(0x03);
    lcd_send_nibble(0x02);

    lcd_send_byte(0x28, 0); // 2 linhas
    lcd_send_byte(0x0C, 0); // display ON
    lcd_send_byte(0x06, 0); // auto increment
    lcd_send_byte(0x01, 0); // clear
}

// posiciona cursor
void lcd_set_cursor(int col, int row){
    int addr = (row == 0) ? 0x80 : 0xC0;
    lcd_send_byte(addr + col, 0);
}

// escreve string no LCD
void lcd_print(const char *str){
    while(*str) lcd_send_byte(*str++, 1);
}

// "limpa" linha inteira
void lcd_clear(int row){
    lcd_set_cursor(0, row);
    lcd_print("                "); //16 caracteres
}

// ---------------- BUZZER ----------------
void buzzer_set(int on){
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, on ? 128 : 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

// ---------------- SISTEMA ----------------
bool device_A, device_B, device_C; //ABC representam o consumo, nesse caso cada LED representa um device

void ABC_normal(){ //3 Devices
    device_A = true; device_B = true; device_C = true;

    gpio_set_level(led_vermelho, 0);
    gpio_set_level(led_amarelo, 0);
    gpio_set_level(led_verde, 1);

    gpio_set_level(A, 1);
    gpio_set_level(B, 1);
    gpio_set_level(C, 1);
}

void ABC_atencao(){ //2 Devices
    device_A = true; device_B = true; device_C = false;

    gpio_set_level(led_vermelho, 0);
    gpio_set_level(led_amarelo, 1);
    gpio_set_level(led_verde, 0);

    gpio_set_level(A, 1);
    gpio_set_level(B, 1);
    gpio_set_level(C, 0);
}

void ABC_critico(){ // 1 Device
    device_A = true; device_B = false; device_C = false;

    gpio_set_level(led_vermelho, 1);
    gpio_set_level(led_amarelo, 0);
    gpio_set_level(led_verde, 0);

    gpio_set_level(A, 1);
    gpio_set_level(B, 0);
    gpio_set_level(C, 0);
}

// ---------------- BOTAO ----------------
void ler_botao(){
    int atual = gpio_get_level(BOTAO);

    static uint32_t last = 0;

    if(estado_anterior == 1 && atual == 0){
        if(xTaskGetTickCount() - last > pdMS_TO_TICKS(250)){
            estado_sistema = !estado_sistema;
            last = xTaskGetTickCount();
        }
    }

    estado_anterior = atual;
}

// ---------------- LCD UPDATE ----------------
void atualizar_lcd(int carga){

    lcd_clear();

    int devices = device_A + device_B + device_C;

    lcd_set_cursor(0,0);

    if(state == 0) lcd_print("NORMAL");
    else if(state == 1) lcd_print("ATTENTION");
    else lcd_print("CRITICAL");

    char buffer[32];

    lcd_set_cursor(0,1);
    sprintf(buffer, "D-%d C-%d%% T-%.0fC",devices,carga,temperature);

    lcd_print(buffer);
}

// ---------------- Saídas ---------------- 
void gpio_init(){

    // Botão
    gpio_set_direction(BOTAO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BOTAO, GPIO_PULLUP_ONLY);

    // LEDs
    gpio_set_direction(led_vermelho, GPIO_MODE_OUTPUT);
    gpio_set_direction(led_amarelo, GPIO_MODE_OUTPUT);
    gpio_set_direction(led_verde, GPIO_MODE_OUTPUT);

    // Saídas A/B/C
    gpio_set_direction(A, GPIO_MODE_OUTPUT);
    gpio_set_direction(B, GPIO_MODE_OUTPUT);
    gpio_set_direction(C, GPIO_MODE_OUTPUT);

    // LCD
    gpio_set_direction(LCD_RS, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_E, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_D4, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_D5, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_D6, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_D7, GPIO_MODE_OUTPUT);
}

void buzzer_init(){

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
}

// ---------------- Entradas ---------------- 
void hardware_init(){

    // Botão
    gpio_set_direction(BOTAO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BOTAO, GPIO_PULLUP_ONLY);

    // ADC (potenciômetro)
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(POT, ADC_ATTEN_DB_11);
}

// ---------------- MAIN ----------------
void app_main(){

    gpio_init();
    hardware_init();
    buzzer_init();
    lcd_init();

    estado_anterior = gpio_get_level(BOTAO);

    while(true){

        ler_botao();

        if(estado_sistema){

            dht_read();

            int adc = adc1_get_raw(POT);
            int carga = (adc * 100) / 4095;

            if(carga >= 80) state = 2;
            else if(carga >= 40) state = 1;
            else state = 0;

            if(state == 0){
                ABC_normal();
                buzzer_set(0);
            }
            else if(state == 1){
                ABC_atencao();

                static int blink = 0;
                buzzer_set(blink);
                blink = !blink;
            }
            else{
                ABC_critico();
                buzzer_set(1);
            }

            atualizar_lcd(carga);
        }
        else{

            gpio_set_level(led_vermelho, 0);
            gpio_set_level(led_amarelo, 0);
            gpio_set_level(led_verde, 0);

            gpio_set_level(A, 0);
            gpio_set_level(B, 0);
            gpio_set_level(C, 0);

            buzzer_set(0);

            lcd_clear();
            lcd_set_cursor(0,0);
            lcd_print("[SYSTEM OFF]");
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}