#ifndef HUNGET_LCD_H
#define HUNGET_LCD_H

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>

// =======================================================
// CẤU HÌNH CHÂN THEO ĐÚNG SƠ ĐỒ MẠCH KIT
// =======================================================

// 1. CHÂN DỮ LIỆU (DATA) -> PORT C
// Sử dụng 4 bit cao: D4->PC4, D5->PC5, D6->PC6, D7->PC7
#define LCD_DATA_DDR  DDRC
#define LCD_DATA_PORT PORTC
#define LCD_DATA_PIN  PINC

// 2. CHÂN ĐIỀU KHIỂN (CONTROL) -> PORT D
#define LCD_CTRL_DDR  DDRD
#define LCD_CTRL_PORT PORTD

// Định nghĩa lại chính xác theo sơ đồ J7:
#define LCD_RW        5  // Pin 5 LCD nối PD5
#define LCD_RS        6  // Pin 4 LCD nối PD6
#define LCD_EN        7  // Pin 6 LCD nối PD7

// =======================================================
// CÁC HÀM GIAO TIẾP MỨC THẤP
// =======================================================

// Hàm tạo xung Enable trên PORT D
void LCD_Enable(void) {
	LCD_CTRL_PORT |= (1 << LCD_EN);  // Bật E (PD7)
	_delay_us(5);
	LCD_CTRL_PORT &= ~(1 << LCD_EN); // Tắt E
	_delay_us(50);
}

// Hàm gửi 4 bit dữ liệu ra PORT C
void LCD_Send4Bit(unsigned char Data) {
	// 1. Gửi 4 bit cao của dữ liệu ra PORT C (vào các chân PC4-PC7)
	// Giữ nguyên trạng thái của 4 chân thấp PC0-PC3 bằng mặt nạ bit
	unsigned char current_port_c = LCD_DATA_PORT & 0x0F;
	LCD_DATA_PORT = current_port_c | (Data & 0xF0);
	
	// 2. Tạo xung chốt bên PORT D
	LCD_Enable();
}

// Hàm gửi lệnh (RS=0, RW=0 tại PORT D)
void LCD4_OUT_CMD(unsigned char command) {
	LCD_CTRL_PORT &= ~(1 << LCD_RS);
	LCD_CTRL_PORT &= ~(1 << LCD_RW);
	
	LCD_Send4Bit(command);      // Gửi 4 bit cao (sang PORT C)
	LCD_Send4Bit(command << 4); // Gửi 4 bit thấp (sang PORT C)
	
	if (command < 4) _delay_ms(2);
	else _delay_us(50);
}

// Hàm gửi dữ liệu (RS=1, RW=0 tại PORT D)
void LCD4_OUT_DATA(unsigned char data) {
	LCD_CTRL_PORT |= (1 << LCD_RS);
	LCD_CTRL_PORT &= ~(1 << LCD_RW);
	
	LCD_Send4Bit(data);
	LCD_Send4Bit(data << 4);
	_delay_us(50);
}

// =======================================================
// CÁC HÀM NGƯỜI DÙNG (USER FUNCTIONS)
// =======================================================

void LCD4_INIT(unsigned char cursor, unsigned char blink) {
	// Khởi tạo hướng dữ liệu (Output)
	// PC4-PC7 là Output (Data)
	LCD_DATA_DDR |= 0xF0;
	
	// PD5, PD6, PD7 là Output (Control)
	LCD_CTRL_DDR |= (1<<LCD_RS) | (1<<LCD_RW) | (1<<LCD_EN);
	
	// Reset mức logic các chân điều khiển về 0
	LCD_CTRL_PORT &= ~((1<<LCD_RS) | (1<<LCD_RW) | (1<<LCD_EN));
	
	_delay_ms(40); // Chờ LCD ổn định sau khi cấp nguồn
	
	// Chuỗi Reset (Gửi 0x30 ba lần theo chuẩn HD44780)
	LCD_Send4Bit(0x30); _delay_ms(5);
	LCD_Send4Bit(0x30); _delay_us(150);
	LCD_Send4Bit(0x30);
	
	// Chuyển sang chế độ 4-bit
	LCD_Send4Bit(0x20);
	
	// Cấu hình hiển thị
	LCD4_OUT_CMD(0x28); // 4-bit, 2 dòng, font 5x8
	LCD4_OUT_CMD(0x08); // Tắt hiển thị
	LCD4_OUT_CMD(0x01); // Xóa màn hình
	_delay_ms(2);
	
	unsigned char cmd = 0x0C; // Mặc định bật màn hình
	if(cursor) cmd |= 0x02;
	if(blink) cmd |= 0x01;
	LCD4_OUT_CMD(cmd);
	
	LCD4_OUT_CMD(0x06); // Tự động tăng con trỏ
}

void LCD4_CUR_GOTO(unsigned char row, unsigned char col) {
	unsigned char address;
	if (row == 1) address = 0x80 + col;
	else address = 0xC0 + col;

	LCD4_OUT_CMD(address);
}

void LCD4_OUT_STR(char *str) {
	while (*str) LCD4_OUT_DATA(*str++);
}

void LCD4_OUT_DEC(unsigned int num, unsigned char len) {
	char buffer[10];
	sprintf(buffer, "%05u", num);
	char *ptr = &buffer[5 - len];
	LCD4_OUT_STR(ptr);
}

void LCD4_DIS_SHIFT(unsigned char direct, unsigned char step) {
	unsigned char i;
	if (direct == 0) // Dịch trái
	for(i=0; i<step; i++) LCD4_OUT_CMD(0x18);
	else             // Dịch phải
	for(i=0; i<step; i++) LCD4_OUT_CMD(0x1C);
}

#endif // HUNGET_LCD_H

