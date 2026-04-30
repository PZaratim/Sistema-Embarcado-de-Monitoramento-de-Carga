#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <stdbool.h>
#include "esp_rom_sys.h"

#define LCD_RS 23
#define LCD_E  27
#define LCD_D4 25
#define LCD_D5 19
#define LCD_D6 18
#define LCD_D7 4
#define led_vermelho 5
#define led_amarelo 21
#define led_verde 22
#define BOTAO 26
#define A 14 //device_A
#define B 12 //device_B
#define C 13 //device_C

int state = 0;                    //estado do sistema 1, 2, 3
int load_total = 0;               //variável
int estado_sistema = 0;           // 0 = desligado | 1 = Ligado
int estado_anterior = 1;
int direcao = 1;

bool device_A;                    //crítico
bool device_B;                    //médio
bool device_C;                    //normal

//Nível baixo do LCD
void lcd_pulse_enable() {
    gpio_set_level(LCD_E, 1);
    esp_rom_delay_us(1);
    gpio_set_level(LCD_E, 0);
    esp_rom_delay_us(100);
}

void lcd_send_nibble(uint8_t nibble) {
    gpio_set_level(LCD_D4, (nibble >> 0) & 1);
    gpio_set_level(LCD_D5, (nibble >> 1) & 1);
    gpio_set_level(LCD_D6, (nibble >> 2) & 1);
    gpio_set_level(LCD_D7, (nibble >> 3) & 1);
    lcd_pulse_enable();
}

void lcd_send_byte(uint8_t data, int rs) {
    gpio_set_level(LCD_RS, rs);
    lcd_send_nibble(data >> 4);
    lcd_send_nibble(data & 0x0F);
    esp_rom_delay_us(40);
}

void lcd_init() {
    vTaskDelay(pdMS_TO_TICKS(50));

    gpio_set_level(LCD_RS, 0);

    lcd_send_nibble(0x03);
    vTaskDelay(pdMS_TO_TICKS(5));

    lcd_send_nibble(0x03);
    esp_rom_delay_us(100);

    lcd_send_nibble(0x03);
    lcd_send_nibble(0x02);

    lcd_send_byte(0x28, 0);
    lcd_send_byte(0x0C, 0);
    lcd_send_byte(0x06, 0);
    lcd_send_byte(0x01, 0);

    vTaskDelay(pdMS_TO_TICKS(5));
}

void lcd_set_cursor(int col, int row) {
    int addr = (row == 0) ? 0x80 : 0xC0;
    addr += col;
    lcd_send_byte(addr, 0);
}

void lcd_print(const char *str) {
    while (*str) {
        lcd_send_byte(*str++, 1);
    }
}

//LCD mensagem linha 1
void atualizar_lcd(){
    lcd_set_cursor(0,0);
    lcd_print("                ");
    lcd_set_cursor(0,1);
    lcd_print("                ");
    lcd_set_cursor(0,0);

    if(state == 0){
        lcd_print("NORMAL");
    }
    else if(state == 1){
        lcd_print("ATTENTION ");
    }
    else{
        lcd_print("CRITICAL! ");
    }

    lcd_set_cursor(0,1);

    char buffer[16];
    sprintf(buffer, "LOAD:%3d%%|", load_total);
    lcd_print(buffer);

    if(direcao > 0){
        lcd_print(" UP ");
    } else {
        lcd_print(" DOWN ");
    }
}

//Estado dos dispositivos
void ABC_normal(){
    device_A = true;
    device_B = true;
    device_C = true;
    gpio_set_level(led_vermelho, 0);
    gpio_set_level(led_amarelo, 0);
    gpio_set_level(led_verde, 1);
    gpio_set_level(A, 1);
    gpio_set_level(B, 1);
    gpio_set_level(C, 1);
}

void ABC_atencao(){
    device_A = true;
    device_B = true;
    device_C = false;
    gpio_set_level(led_vermelho, 0);
    gpio_set_level(led_amarelo, 1);
    gpio_set_level(led_verde, 0);
    gpio_set_level(A, 1);
    gpio_set_level(B, 1);
    gpio_set_level(C, 0);
}

void ABC_critico(){
    device_A = true;
    device_B = false;
    device_C = false;
    gpio_set_level(led_vermelho, 1);
    gpio_set_level(led_amarelo, 0);
    gpio_set_level(led_verde, 0);
    gpio_set_level(A, 1);
    gpio_set_level(B, 0);
    gpio_set_level(C, 0);

}

//Botão liga e desliga do sistema
void ler_botao() {
    int estado_atual = gpio_get_level(BOTAO);

    if (estado_anterior == 1 && estado_atual == 0) {
        estado_sistema = !estado_sistema;

        if (estado_sistema) {
            printf("SISTEMA ATIVADO\n");
        } else {
            printf("SISTEMA DESATIVADO\n");
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
    estado_anterior = estado_atual;
}

void app_main(){
    gpio_set_direction(BOTAO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BOTAO, GPIO_PULLUP_ONLY);

    gpio_set_direction(led_vermelho, GPIO_MODE_OUTPUT);
    gpio_set_direction(led_amarelo, GPIO_MODE_OUTPUT);
    gpio_set_direction(led_verde, GPIO_MODE_OUTPUT);
    gpio_set_direction(A, GPIO_MODE_OUTPUT);
    gpio_set_direction(B, GPIO_MODE_OUTPUT);
    gpio_set_direction(C, GPIO_MODE_OUTPUT);

    gpio_set_direction(LCD_RS, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_E,  GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_D4, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_D5, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_D6, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_D7, GPIO_MODE_OUTPUT);

    estado_anterior = gpio_get_level(BOTAO);
    lcd_init();

    while (true){
        ler_botao();

        if(estado_sistema == 1){

            if(load_total < 40){
                state = 0;
            }
            else if(load_total < 80){
                state = 1;
            }
            else{
                state = 2;
            }

            if(state == 0){
                printf("[ NORMAL ] | Carga: %3d%% | SUBINDO\n", load_total);
                ABC_normal();
                load_total += 5;
                direcao = 1;
            }
            else if(state == 1){

                if(direcao == 1){
                    printf("[ATENCAO] | Carga: %3d%% | SUBINDO\n", load_total);
                } 
                else {
                    printf("[PERIGO] | Carga: %3d%% | DESCENDO\n", load_total);
                }

                ABC_atencao();
                load_total += (10 * direcao);
            }
            else{
                printf("[ CRITICO ] | Carga: %3d%% | DESCENDO\n", load_total);
                ABC_critico();
                load_total -= 5;
                direcao = -1;
            }

            atualizar_lcd();

        } 
        else { //Sistema desligado
            gpio_set_level(led_vermelho, 0);
            gpio_set_level(led_amarelo, 0);
            gpio_set_level(led_verde, 0);

            load_total = 0;
            state = 0;
            direcao = 1;

            lcd_set_cursor(0,0);
            lcd_print("                ");
            lcd_set_cursor(0,1);
            lcd_print("                ");

            lcd_set_cursor(2,0);
            lcd_print("[SYSTEM OFF]");
        }

        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}
