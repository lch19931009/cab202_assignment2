//
//  Invaders.c
//  
//
//  Created by chunhonglee on 17/5/16.
//
//

//#include "Invaders.h"
#include "small_graphics.h"
#include "ascii_font_small.h"

#include <avr/io.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <avr/io.h>
#include <util/delay.h>
#include <assert.h>
#include <stdint.h>

#include "cpu_speed.h"
#include "graphics.h"
#include "lcd.h"
#include "sprite.h"

#define F_CPU 8000000UL

#define NUM_BUTTONS 6
#define BTN_DPAD_LEFT 0
#define BTN_DPAD_RIGHT 1
#define BTN_DPAD_UP 2
#define BTN_DPAD_DOWN 3
#define BTN_LEFT 4
#define BTN_RIGHT 5

#define BTN_STATE_UP 0
#define BTN_STATE_DOWN 1

#define NUM_INV 15
#define NUM_WALL 24
#define WALL_W 4
#define WALL_H 2

#define FREQUENCY 8000000.0
#define PRESCALER 256.0

Sprite ship;
Sprite enemy[NUM_INV];
Sprite player_bullet;
Sprite invader_bullet[NUM_INV];
Sprite walls[NUM_WALL];
volatile unsigned char btn_hists[NUM_BUTTONS];
volatile unsigned char btn_states[NUM_BUTTONS];

int score = 0;
volatile int live = 3;
volatile int level = 1;
volatile bool over = false;
bool start = false;
int invader_bullet_count = 0;
volatile signed int adc_value;
long adc_delay;
float max_adc = 1023.0;
long adc_move = 0;
uint16_t adc_result0;
uint16_t adc_result1;


unsigned char player [] ={
    0b10011001,
    0b11011011,
    0b11100111
};

unsigned char invader [] = {
    0b00111100,
    0b11011011,
    0b01111110,
    0b01100110
};

unsigned char bullet [] = {
    0b10000000
};

unsigned char wall [] = {
    0b10000000
};

void init_hardware(void){
    // screen contrast
    lcd_init(LCD_DEFAULT_CONTRAST);
    
    //lcd_write(0, 0b00001101);
    
    // input buttons
    DDRF &= ~((1<<6)|(1<<5));
    DDRD &= ~((1<<1)|1);
    DDRB &= ~((1<<7)|(1<<1));
    
    // Timer
    TCCR0A = 0x00;
    TCCR0B |= (1<<2);
    TIMSK0 |= 1;
    
    // interrupts
    sei();
}

void adc_init(){
    // AREF = AVcc
    ADMUX = (1<<REFS0);
    
    // ADC Enable and pre-scaler of 128
    // 8000000/128 = 62500
    ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1);//|(1<<ADPS0)|(1<<ADATE)|(1<<ADIE);
    //ADCSRA |= (1<<ADSC);
}

uint16_t adc_read(uint8_t ch)
{
    // select the corresponding channel 0~7
    // ANDing with '7' will always keep the value
    // of 'ch' between 0 and 7
    ch &= 0b00000111;  // AND operation with 7
    ADMUX = (ADMUX & 0xF8)|ch;     // clears the bottom 3 bits before ORing
    
    // start single conversion
    // write '1' to ADSC
    ADCSRA |= (1<<ADSC);
    
    // wait for conversion to complete
    // ADSC becomes '0' again
    // till then, run loop continuously
    while(ADCSRA & (1<<ADSC));
    
    return (ADC);
}

void welcome(){
    draw_string(LCD_X/2-18, LCD_Y/2-7, "Welcome");
    draw_string(LCD_X/2-18, LCD_Y/2+1, "Level:");
    
    char level_str[10];
    sprintf(level_str, "%d", level);
    draw_string(LCD_X/2 +12, LCD_Y/2+1, level_str);
    
    if ((btn_states[BTN_LEFT] == BTN_STATE_DOWN)){
        start = true;
        for (int i = 0; i < 3; i++){
            for (int ii = 0; ii < 5; ii++){
                
                if ((level == 1) || (level == 2)){
                    enemy[ii+(5*i)].dx = -1;
                }
                
                if (level == 3){
                    for (int i = 0; i < 3; i++){
                        int random = 1+rand()%2;
                        for (int ii = 0; ii < 5; ii++){
                            enemy[ii+(5*i)].dx = -random;
                        }
                    }
                }
                if (level != 1){
                    
                    adc_init();
                    ship.x = (adc_result1*(long)(LCD_X-8)) / max_adc;
                    
                    enemy[ii+(5*i)].dy = 1;
                    for (int l = 0; l < NUM_WALL/WALL_W; l++){
                        for (int k = 0; k < WALL_H; k++){
                            for (int j = 0; j < WALL_W; j++){
                                init_sprite(&walls[(l*8)+j+(k*4)], 17 + j + l*(18+WALL_W), LCD_Y -12+k, 1, 1, wall);
                            }
                        }
                    }
                }
            }
        }
    }
}

void draw_display(){
    draw_string_small(1, 1, "Score:");
    char score_str[10];
    sprintf(score_str, "%d", score);
    draw_string_small(25, 1, score_str);
    
    draw_string_small(35, 1, "Live:");
    char live_str[10];
    sprintf(live_str, "%d", live);
    draw_string_small(55, 1, live_str);
    
    draw_string_small(LCD_X - 20, 1, "Lv:");
    char level_str[10];
    sprintf(level_str, "%d", level);
    draw_string_small(LCD_X - 8, 1, level_str);
}

void setup_game(){
    if (level != 1){
        for (int i = 0; i < NUM_INV; i++){
            for (int l = 0; l < NUM_WALL; l++){
                if ((invader_bullet[i].y == walls[l].y) && (invader_bullet[i].x == walls[l].x) && (walls[l].is_visible == true) && (invader_bullet[i].is_visible = true)){
                    walls[l].is_visible = false;
                    invader_bullet[i].dy = 0;
                    invader_bullet[i].is_visible = false;
                    invader_bullet_count++;
                }
                
                if ((player_bullet.y == walls[l].y) && (player_bullet.x == walls[l].x) && (walls[l].is_visible == true) && (player_bullet.is_visible == true)){
                    walls[l].is_visible = false;
                    player_bullet.is_visible = false;
                    player_bullet.dy = 0;
                }
            }
        }
    }
}

void invader_movement(){
    if (((enemy[0].x == 1) || ((enemy[14].x + 8) == LCD_X - 1)) && (level != 3)) {
        for (int i = 0; i < NUM_INV; i++){
            if (enemy[0].x == 1){
                enemy[i].dx = 1;
            }
            if ((enemy[14].x + 8) == LCD_X -1){
                enemy[i].dx = -1;
            }
        }
    }
}


int main (void){
    set_clock_speed(CPU_8MHz);
    
    // TODO Setup sprite
    init_sprite(&ship, LCD_X/2 - 4, LCD_Y - 4, 8, 3, player);
    for (int i = 0; i < 3; i++){
        for (int ii = 0; ii < 5; ii++){
            init_sprite(&enemy[ii+(5*i)], (6 + (16 * ii)), 8 + (7 * i), 8, 4, invader);
            init_sprite(&invader_bullet[ii+(5*i)], (10 + (16* ii)), 12 + (7 * i), 1, 1, bullet);
            invader_bullet[ii+(5*i)].dy++;
        }
    }

    // TODO Setup hardware
    init_hardware();
    
    // Main loop
    while(!over == true){
        // Draw Screen
        clear_screen();
        
        // welcome n choose level
        if (start == false){
            welcome();
        }
        
        if (start == true){
            clear_screen();
            
            // TODO draw bar
            draw_display();
        
        
            // border line
            //draw_line(0, 6, LCD_X, 6);
            //draw_line(0, 6, 0, LCD_Y);
            //draw_line(LCD_X - 1, 6, LCD_X - 1 , LCD_Y);
        
            // alien movement
            //TODO Make all aliens stop when the right or left most alien reaches the edge of the screen
            //TODO Change the dx value based on which edge
            //TODO Change the x value based on dx
            for (int i = 0; i < NUM_INV; i++){
                enemy[i].x += enemy[i].dx;
                if (level != 1){
                    enemy[i].y += enemy[i].dy;
                }
            }
            
            setup_game();
            
            invader_movement();
            
            adc_result0 = adc_read(0);
            adc_result1 = adc_read(1);
            
            if (level == 2){
                long max_lcd_adc = (adc_result1*(long)(LCD_X-8)) / max_adc;
                ship.x = max_lcd_adc;
                /*if (ship.x + adc_move != max_lcd_adc){
                    adc_move = max_lcd_adc - ship.x;
                }
                
                if (adc_move >= 3){
                    adc_delay = 3;
                    adc_move -= 3;
                }
                if (adc_move < 3 && adc_move > 0){
                    adc_move = 0;
                }
                if (adc_move > -3 && adc_move < 0){
                    adc_move = 0;
                }
                if (adc_move <= -3){
                    adc_delay = -3;
                    adc_move += 3;
                }
                ship.x += adc_delay;*/
            }
            
            if ((level == 3) && (player_bullet.y == LCD_Y/2 + 4)){
                if (adc_result0 <= 256){
                    player_bullet.dx = -2;
                }
                if(adc_result0 <= 512 && adc_result0 > 256){
                    player_bullet.dx = -1;
                }
                if(adc_result0 > 512 && adc_result0 <= 768){
                    player_bullet.dx = 1;
                }
                if(adc_result0 > 768){
                    player_bullet.dx = 2;
                }
            }

        
            // TODO buttons
            if  ((btn_states[BTN_LEFT] == BTN_STATE_DOWN) && (ship.x > 1) && (level == 1)){
                ship.x--;
            }
        
            if ((btn_states[BTN_RIGHT] == BTN_STATE_DOWN) && (ship.x < (LCD_X - 9)) && (level == 1)){
                ship.x++;
            }
            
            // TODO level 2 buttons
        
            if ((btn_states[BTN_DPAD_LEFT] == BTN_STATE_DOWN) && (ship.x > 1) && (level == 3)){
                ship.x--;
            }
        
            if ((btn_states[BTN_DPAD_RIGHT] == BTN_STATE_DOWN) && (ship.x < (LCD_X - 9)) && (level == 3)){
                ship.x++;
            }
            
            
            if ((over != true) && (btn_states[BTN_DPAD_UP] == BTN_STATE_DOWN) && (player_bullet.is_visible == false)) {
                if (btn_states[BTN_STATE_UP] == BTN_STATE_UP){
                    init_sprite(&player_bullet, ship.x + 4, LCD_Y - 4, 1, 1, bullet);
                    player_bullet.dy = -1;
                }
            }
            
            draw_sprite(&player_bullet);
            
            for (int i = 0; i < NUM_INV; i++){
                for (int ii = 0; ii < 4; ii++){
                    for (int iii = 0; iii < 8; iii++){
                        if ((player_bullet.y == enemy[i].y + ii) && (player_bullet.x == enemy[i].x + iii) && (enemy[i].is_visible == true) && (player_bullet.is_visible == true)){
                            score++;
                            player_bullet.is_visible = false;
                            player_bullet.dy = 0;
                            enemy[i].is_visible = false;
                            if (level == 2){
                                enemy[i].dy = 0;
                            }
                        }
                    }
                }
                
                for (int j = 0; j < 3; j++){
                    for (int jj = 0; jj < 8; jj++){
                        if ((invader_bullet[i].y == ship.y + j) && (invader_bullet[i].x == ship.x + jj) && (invader_bullet[i].is_visible == true)){
                            live--;
                            invader_bullet[i].is_visible = false;
                            invader_bullet_count++;
                        }
                    }
                }
                
                if ((invader_bullet[i].y == LCD_Y) && (invader_bullet[i].dy != 0)){
                    invader_bullet[i].dy = 0;
                    invader_bullet_count++;
                }
                
                if (invader_bullet_count == (NUM_INV - score)){
                    for (int i = 0; i < NUM_INV; i++){
                        if (enemy[i].is_visible == true){
                            invader_bullet[i].x = enemy[i].x + 3;
                            invader_bullet[i].y = enemy[i].y + 3;
                            invader_bullet_count = 0;
                            invader_bullet[i].dy = 1;
                            invader_bullet[i].is_visible = true;
                        }
                    }
                }
            }
            
            if (level != 1){
                for (int i = 0; i < 3; i++){
                    int random = 1+rand()%2;
                    for (int ii = 0; ii < 5; ii++){
                        if (level == 3){
                            if (enemy[i*5].x <= 1){
                                enemy[ii+(i*5)].dx = random;
                            }
                            
                            if (enemy[((i+1)*5)-1].x + 8 >= LCD_X - 1){
                                enemy[ii+(i*5)].dx = - random;
                            }
                            
                            /*if (enemy[0].x <= 1){
                                enemy[ii].dx = random;
                            }
                            if ((enemy[4].x + 8) >= LCD_X - 1){
                                enemy[ii].dx = -3;
                            }
                            if (enemy[5].x <= 1){
                                enemy[5 + ii].dx = 2;
                            }
                            if ((enemy[9].x + 8 >= LCD_X - 1)){
                                enemy[5 + ii].dx = -2;
                            }
                            if (enemy[10].x == 1){
                                enemy[10 + ii].dx = 1;
                            }
                            if ((enemy[14].x + 8) == LCD_X - 1){
                                enemy[10 + ii].dx = -1;
                            }*/
                        }
                        
                        // middle screen == LCD_Y/2 + 4
                        // draw_line(0, LCD_Y/2 + 4, LCD_X, LCD_Y/2 + 4);
                        if (level != 1){
                            if (enemy[14].y == LCD_Y/2 + 4){
                                for (int j = 0; j < NUM_INV; j++){
                                    enemy[j].dy = -1;
                                }
                            }
                            /*if (enemy[ii+(5*i)].y == 8){
                                for (int j = 0; j < 5; j++){
                                    if (enemy[j+(5*i)].dy != 0){
                                        enemy[j+(5*i)].is_visible = true;
                                    }
                                }
                            }*/
                            if (enemy[0].y == 8){
                                for (int i = 0; i < NUM_INV; i++){
                                    enemy[i].dy = 1;
                                }
                            }
                        }
                    }
                }
            }

            
            // when player bullet hits the top
            if (player_bullet.y == 7){
                player_bullet.is_visible = false;
                player_bullet.dy = 0;
            }
            
            // Game over condition
            if ((score == 15) || (live == 0)) {
                over = true;
            }
            
            // bullet moving
            player_bullet.y += player_bullet.dy;
            if (level == 3){
                player_bullet.x += player_bullet.dx;
            }
            for (int i = 0; i < NUM_INV; i++){
                invader_bullet[i].y += invader_bullet[i].dy;
            }
        
            // TODO draw sprite
            draw_sprite(&ship);
            for (int i = 0; i < NUM_INV; i++){
                draw_sprite(&enemy[i]);
                draw_sprite(&invader_bullet[i]);
            }
            
            for (int i = 0; i < NUM_WALL; i++){
                draw_sprite(&walls[i]);
            }
        
            // Game over
            if (over == true){
                clear_screen();
                draw_string(LCD_X/2 - 22, LCD_Y/2 - 3, "Game over!");
            }
        }
    
        show_screen();
        
        // Have a rest lol
        _delay_ms(50);
    }
    
    return 0;
}

ISR(TIMER0_OVF_vect){
    btn_hists[BTN_DPAD_LEFT] = (btn_hists[BTN_DPAD_LEFT]<<1)|(PINB&(1<<1));
    btn_hists[BTN_DPAD_RIGHT] = (btn_hists[BTN_DPAD_RIGHT]<<1)|(PIND&(1<<0));
    btn_hists[BTN_DPAD_UP] = (btn_hists[BTN_DPAD_UP]<<1)|(PIND&(1<<1));
    btn_hists[BTN_DPAD_DOWN] = (btn_hists[BTN_DPAD_DOWN]<<1)|(PINB&(1<<7));
    btn_hists[BTN_LEFT] = (btn_hists[BTN_LEFT]<<1)|(PINF&(1<<6));
    btn_hists[BTN_RIGHT] = (btn_hists[BTN_RIGHT]<<1)|(PINF&(1<<5));
    
    for (int i = 0; i < NUM_BUTTONS; i++){
        if (btn_hists[i] == 0){
            if (btn_states[i] == BTN_STATE_DOWN){
                if ((start != true) && (btn_states[BTN_RIGHT] == BTN_STATE_DOWN)) {
                    if (level != 3){
                        level++;
                    }
                    else {
                        level = 1;
                    }
                }
            }
            btn_states[i] = BTN_STATE_UP;
        }
        
        else if (btn_hists[i] >= 1){
            btn_states[i] = BTN_STATE_DOWN;
        }
    }
}