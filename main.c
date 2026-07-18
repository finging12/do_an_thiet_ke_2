#define F_CPU 8000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>

#include "hunget_lcd.h" // Thu vien LCD

// =============================================
// Relay
#define RELAY_FAN_DDR      DDRA
#define RELAY_FAN_PORT     PORTA
#define RELAY_FAN_PIN      2    // PA2

#define RELAY_BUZZER_DDR   DDRA
#define RELAY_BUZZER_PORT  PORTA
#define RELAY_BUZZER_PIN   3    // PA3

// =============================================
// Dinh nghia nut bam PORTB

#define BTN_UP_BIT        0     // PB0 - Tăng ngưỡng
#define BTN_DOWN_BIT      1     // PB1 - Giảm ngưỡng
#define BTN_MODE_BIT      2     // PB2 - Auto/Manual
#define BTN_FAN_BIT       3     // PB3 - Quạt thủ công

// =============================================
// BIẾN TOÀN CỤC

uint16_t threshold = 300;    // Ngưỡng mặc định 30.0°C
uint8_t auto_mode = 1;       // 1: Auto, 0: Manual
uint8_t manual_fan = 0;      // 0: Tắt quạt, 1: Bật quạt

// =============================================
// HÀM CẬP NHẬT LED 7 ĐOẠN

void LED7_REFRESH() {
	uint8_t pd_save = PORTD & 0xE0;
	PORTC = 0x00;
	PORTD = (PORTD & 0x1F) | pd_save;
}

// =============================================
// HÀM KHỞI TẠO NÚT BẤM

void BUTTON_INIT() {
	DDRB &= ~((1 << BTN_UP_BIT) | (1 << BTN_DOWN_BIT) | (1 << BTN_MODE_BIT) | (1 << BTN_FAN_BIT));
	PORTB |= (1 << BTN_UP_BIT) | (1 << BTN_DOWN_BIT) | (1 << BTN_MODE_BIT) | (1 << BTN_FAN_BIT);
}

// =============================================
// HÀM ĐỌC NÚT BẤM (ĐÃ SỬA - THÊM CHỐNG RUNG TỐT HƠN)
// =============================================
uint8_t BUTTON_READ(uint8_t pin_bit) {
	static uint16_t hold_counter[4] = {0, 0, 0, 0};
	static uint8_t last_state[4] = {1, 1, 1, 1};  // Trạng thái trước đó
	
	uint8_t current_state = (PINB & (1 << pin_bit)) ? 1 : 0;
	
	if (current_state == 0 && last_state[pin_bit] == 1) {  // Nhấn xuống
		last_state[pin_bit] = 0;
		return 1;
	}
	
	if (current_state == 1 && last_state[pin_bit] == 0) {  // Nhả ra
		last_state[pin_bit] = 1;
	}
	
	return 0;
}

// =============================================
// HÀM KHỞI TẠO RELAY (ĐÃ SỬA - RELAY ACTIVE HIGH)
// =============================================
void RELAY_INIT() {
	RELAY_FAN_DDR |= (1 << RELAY_FAN_PIN) | (1 << RELAY_BUZZER_PIN);
	// Tắt relay ban đầu (Active HIGH: 0 = OFF)
	RELAY_FAN_PORT &= ~(1 << RELAY_FAN_PIN);
	RELAY_BUZZER_PORT &= ~(1 << RELAY_BUZZER_PIN);
}

// =============================================
// HÀM BẬT/TẮT RELAY (ĐÃ SỬA - ACTIVE HIGH)
// =============================================
void RELAY_FAN_ON() {
	RELAY_FAN_PORT |= (1 << RELAY_FAN_PIN);   // HIGH = ON
}

void RELAY_FAN_OFF() {
	RELAY_FAN_PORT &= ~(1 << RELAY_FAN_PIN);  // LOW = OFF
}

void RELAY_BUZZER_ON() {
	RELAY_BUZZER_PORT |= (1 << RELAY_BUZZER_PIN);
}

void RELAY_BUZZER_OFF() {
	RELAY_BUZZER_PORT &= ~(1 << RELAY_BUZZER_PIN);
}

// =============================================
// HÀM KHỞI TẠO ADC
// =============================================
void ADC_INIT_CONFIG() {
	DDRA &= ~(1 << PA1);
	ADMUX = (1 << REFS0);
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

// =============================================
// HÀM ĐỌC ADC
// =============================================
uint16_t ADC_READ(uint8_t channel) {
	ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
	ADCSRA |= (1 << ADSC);
	while(ADCSRA & (1 << ADSC));
	return ADCW;
}

// =============================================
// HÀM XỬ LÝ NÚT BẤM
// =============================================
void HANDLE_BUTTONS() {
	if (BUTTON_READ(BTN_UP_BIT)) {
		threshold += 10;
		if (threshold > 500) threshold = 500;
	}
	
	if (BUTTON_READ(BTN_DOWN_BIT)) {
		if (threshold >= 20) {
			threshold -= 10;
		}
		if (threshold < 10) threshold = 10;
	}
	
	if (BUTTON_READ(BTN_MODE_BIT)) {
		auto_mode = !auto_mode;
	}
	
	if (BUTTON_READ(BTN_FAN_BIT)) {
		if (!auto_mode) {
			manual_fan = !manual_fan;
		}
	}
}

// =============================================
// HÀM ĐIỀU KHIỂN 7 LED (PD0-PD6)
// =============================================
void LED_ON() {
	PORTD &= 0x80;
}

void LED_OFF() {
	PORTD |= 0x7F;
}

// =============================================
// HÀM CHÍNH
// =============================================
int main(void) {
	// 1. TẮT JTAG (QUAN TRỌNG - LÀM 2 LẦN)
	MCUCSR = (1 << JTD);  // SỬA: Ghi trực tiếp
	MCUCSR = (1 << JTD);  // SỬA: Làm 2 lần

	// 2. KHỞI TẠO LED 7 ĐOẠN
	DDRC = 0xFF;
	PORTC = 0x00;

	// 3. KHỞI TẠO 7 LED ĐƠN (PD0-PD6)
	DDRD |= 0x7F;
	LED_ON();

	// 4. KHỞI TẠO
	LCD4_INIT(0, 0);
	ADC_INIT_CONFIG();
	RELAY_INIT();
	BUTTON_INIT();

	// --- MÀN HÌNH CHÀO ---
	LCD4_CUR_GOTO(1, 4);
	LCD4_OUT_STR("Welcome!");
	LCD4_CUR_GOTO(2, 2);
	LCD4_OUT_STR("  20224193");
	_delay_ms(1500);
	LCD4_OUT_CMD(0x01);
	_delay_ms(2);

	char buffer[16];
	uint32_t sum_adc;
	uint16_t avg_adc;
	uint16_t temp_val;

	while (1) {
		HANDLE_BUTTONS();
		
		// --- ĐỌC NHIỆT ĐỘ ---
		sum_adc = 0;
		for (int i = 0; i < 10; i++) {
			sum_adc += ADC_READ(1);
			_delay_ms(1);
		}
		avg_adc = sum_adc / 10;
		uint32_t voltage_mv = ((uint32_t)avg_adc * 5000) / 1024;  // SỬA: 1024
		temp_val = voltage_mv;

		// --- LCD DÒNG 1 ---
		LCD4_CUR_GOTO(1, 0);
		sprintf(buffer, "Temp: %u.%u C   ", temp_val / 10, temp_val % 10);
		LCD4_OUT_STR(buffer);
		LED7_REFRESH();

		// --- LCD DÒNG 2 ---
		LCD4_CUR_GOTO(2, 0);
		
		if (auto_mode) {
			if (temp_val > threshold) {
				sprintf(buffer, "HIGH  Th:%u.%u C ", threshold / 10, threshold % 10);
				LCD4_OUT_STR(buffer);
				LED7_REFRESH();
				
				LED_OFF();
				
				// SỬA: Dùng hàm RELAY mới
				RELAY_FAN_ON();
				RELAY_BUZZER_ON();
				
			} else {
				sprintf(buffer, "NORM  Th:%u.%u C ", threshold / 10, threshold % 10);
				LCD4_OUT_STR(buffer);
				LED7_REFRESH();
				
				LED_ON();
				
				// SỬA: Dùng hàm RELAY mới
				RELAY_FAN_OFF();
				RELAY_BUZZER_OFF();
			}
			
		} else {
			sprintf(buffer, "MANUAL Th:%u.%u C", threshold / 10, threshold % 10);
			LCD4_OUT_STR(buffer);
			LED7_REFRESH();
			
			LED_ON();
			
			if (manual_fan) {
				// SỬA: Dùng hàm RELAY mới
				RELAY_FAN_ON();
				RELAY_BUZZER_ON();
			} else {
				// SỬA: Dùng hàm RELAY mới
				RELAY_FAN_OFF();
				RELAY_BUZZER_OFF();
			}
		}

		_delay_ms(20);
	}
	return 0;
}
