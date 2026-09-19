// Часы погодная станция v2.3
// Модифицированный скетч (создан voltNik / Исправлен)

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <stdio.h>
#include <DS1302.h>
#include <Adafruit_BMP085.h>
#include <EEPROMex.h>

//****************************
// Пины кнопок управления
#define BTN_UP 8       // в часах короткое: Mute / длинное: меню звука | в меню: + | в секундомере: СБРОС
#define BTN_DOWN 9     // в часах: сброс на Главный экран / Ночной режим | длинное: Сброс EEPROM | в меню: -
#define BTN_SET 10     // в часах: Настройка времени | в секундомере: Старт / Стоп
#define TTP_PIN 2      // в часах: Вход в секундомер | в секундомере: Выход из секундомера

// Пины подключения модуля часов
#define kCePin 3       // RST
#define kIoPin 6       // DAT
#define kSclkPin 7     // CLK

#define FOTORES A0     // A0 пин подключения фоторезистора
#define LCD_LED 5      // ШИМ пин подключения подсветки LCD
#define BUZZER_PIN 11  // пин подключения АКТИВНОГО зуммера

#define BTN_PROTECT 100         // защита от дребезга кнопки 
#define LCD_RENEW 250           // обновление экрана
#define TRANSITION_DELAY 300    // время блокировки кнопок при смене экрана (мс)
//****************************

LiquidCrystal_I2C lcd(0x27, 20, 4);  // адрес 0x27 или 0x3F
DS1302 rtc(kCePin, kIoPin, kSclkPin);
Time t = rtc.time();
Adafruit_BMP085 bmp;

//****************************
int bright, btn_up_val, btn_down_val, btn_set_val, now_year, now_temp; 
float now_press;
byte now_disp, now_month, now_date, now_hour, now_min, now_sec, now_week_day, alarm_hour, alarm_min;
byte night_start_hour = 23, night_end_hour = 7; // Часы ночного режима
long now_millis, lcd_millis, time_millis, btn_up_millis, btn_down_millis, btn_set_millis, disp_millis, horn_millis;
boolean dot, blnk, alarm, horn, note, time_changed;
byte set_time; // 0 - норма, 1..10 - время/будильник, 11..12 - ночной режим, 13..14 - звук
char sep = ':'; // Объявление разделителя

// Переменные звука и оформления
boolean buzzer_enabled = true; // Включение/выключение клика
byte buzzer_sound_type = 1;     // Тип звука (1, 2, 3)
byte ui_theme = 0;              // Тема главного экрана (0 - Стандарт, 1 - Минимализм, 2 - Информер)

// Флаг ручного ночного режима
boolean manual_night_mode = false;

// Блокировка кнопок во время анимации/смены экрана
boolean screen_transition = false;
unsigned long transition_millis = 0;

// Отслеживание удержания кнопок
unsigned long btn_up_press_time = 0;
boolean btn_up_pressed = false;
boolean btn_up_long_triggered = false;

unsigned long btn_down_press_time = 0;
boolean btn_down_pressed = false;
boolean btn_down_long_triggered = false;

int disp[4] = {25000, 3000, 3000, 3000}; // тайминг работы экранов

// Переменные для TTP223 и Секундомера
boolean mode_stopwatch = false;
boolean sw_running = false;
unsigned long sw_start_time = 0;
unsigned long sw_elapsed_time = 0;
boolean last_ttp_state = LOW;
unsigned long ttp_press_time = 0;

//**************************** 
// Массив с днями недели
const char* week_day[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

// Восемь пользовательских символов для больших цифр
byte custom[8][8] = {
  { B11111,B11111,B11111,B00000,B00000,B00000,B00000,B00000 },
  { B11100,B11110,B11111,B11111,B11111,B11111,B11111,B11111 },
  { B11111,B11111,B11111,B11111,B11111,B11111,B01111,B00111 },
  { B00000,B00000,B00000,B00000,B00000,B11111,B11111,B11111 },
  { B11111,B11111,B11111,B11111,B11111,B11111,B11110,B11100 },
  { B11111,B11111,B11111,B00000,B00000,B00000,B11111,B11111 },
  { B11111,B00000,B00000,B00000,B00000,B11111,B11111,B11111 },
  { B00111,B01111,B11111,B11111,B11111,B11111,B11111,B11111 }
};

const char *bigChars[][2] = {   
  {"\024\024\024", "\024\024\024"}, // Space
  {"\377", "\007"}, // !
  {"\005\005", "\024\024"}, // "
  {"\004\377\004\377\004", "\001\377\001\377\001"}, // #
  {"\010\377\006", "\007\377\005"}, // $
  {"\001\024\004\001", "\004\001\024\004"}, // %
  {"\010\006\002\024", "\003\007\002\004"}, // &
  {"\005", "\024"}, // '
  {"\010\001", "\003\004"}, // (
  {"\001\002", "\004\005"}, // )
  {"\001\004\004\001", "\004\001\001\004"}, // *
  {"\004\377\004", "\001\377\001"}, // +
  {"\024", "\005"}, // ,
  {"\004\004\004", "\024\024\024"}, // -
  {"\024", "\004"}, // .
  {"\024\024\004\001", "\004\001\024\024"}, // /
  {"\010\001\002", "\003\004\005"}, // 0
  {"\001\002\024", "\024\377\024"}, // 1
  {"\006\006\002", "\003\007\007"}, // 2
  {"\006\006\002", "\007\007\005"}, // 3
  {"\003\004\002", "\024\024\377"}, // 4
  {"\377\006\006", "\007\007\005"}, // 5
  {"\010\006\006", "\003\007\005"}, // 6
  {"\001\001\002", "\024\010\024"}, // 7
  {"\010\006\002", "\003\007\005"}, // 8
  {"\010\006\002", "\024\024\377"}, // 9
  {"\004", "\001"}, // :
  {"\004", "\005"}, // ;
  {"\024\004\001", "\001\001\004"}, // <
  {"\004\004\004", "\001\001\001"}, // =
  {"\001\004\024", "\004\001\001"}, // >
  {"\001\006\002", "\024\007\024"}, // ?
  {"\010\006\002", "\003\004\004"}, // @
  {"\010\006\002", "\377\024\377"}, // A
  {"\377\006\005", "\377\007\002"}, // B
  {"\010\001\001", "\003\004\004"}, // C
  {"\377\001\002", "\377\004\005"}, // D
  {"\377\006\006", "\377\007\007"}, // E
  {"\377\006\006", "\377\024\024"}, // F
  {"\010\001\001", "\003\004\002"}, // G
  {"\377\004\377", "\377\024\377"}, // H
  {"\001\377\001", "\004\377\004"}, // I
  {"\024\024\377", "\004\004\005"}, // J
  {"\377\004\005", "\377\024\002"}, // K
  {"\377\024\024", "\377\004\004"}, // L
  {"\010\003\005\002", "\377\024\024\377"}, // M
  {"\010\002\024\377", "\377\024\003\005"}, // N
  {"\010\001\002", "\003\004\005"}, // 0/0
  {"\377\006\002", "\377\024\024"}, // P
  {"\010\001\002\024", "\003\004\377\004"}, // Q
  {"\377\006\002", "\377\024\002"}, // R
  {"\010\006\006", "\007\007\005"}, // S
  {"\001\377\001", "\024\377\024"}, // T
  {"\377\024\377", "\003\004\005"}, // U
  {"\003\024\024\005", "\024\002\010\024"}, // V
  {"\377\024\024\377", "\003\010\002\005"}, // W
  {"\003\004\005", "\010\024\002"}, // X
  {"\003\004\005", "\024\377\024"}, // Y
  {"\001\006\005", "\010\007\004"}, // Z
  {"\377\001", "\377\004"}, // [
  {"\001\004\024\024", "\024\024\001\004"}, // Backslash
  {"\001\377", "\004\377"}, // ]
  {"\010\002", "\024\024"}, // ^
  {"\024\024\024", "\004\004\004"}, // _ 
};

int writeBigChar(char ch, int x, int y) {
  const char *(*blocks)[2] = NULL;
  if (ch < ' ' || ch > '_') return 0;
  blocks = &bigChars[ch-' '];
  for (int half = 0; half <= 1; half++) {
    int t = x;
    for (const char *cp = (*blocks)[half]; *cp; cp++) {
      if (t < 20) { 
        lcd.setCursor(t, y+half); 
        lcd.write(*cp);
      }
      t++;
    }
    if (t < 20) {
      lcd.setCursor(t, y+half);
      lcd.write(' ');
    }
  }
  return strlen((*blocks)[0]);
}

void writeBigString(char *str, int x, int y) {
  char c;
  while ((c = *str++)) {
    if (x >= 20) break;
    x += writeBigChar(c, x, y) + 1;
  }
}

// Воспроизведение звуков
void buzzerClick(int durationMs) {
  if (!buzzer_enabled) return;

  switch (buzzer_sound_type) {
    case 1:
      digitalWrite(BUZZER_PIN, HIGH);
      delay(durationMs);
      digitalWrite(BUZZER_PIN, LOW);
      break;
    case 2:
      digitalWrite(BUZZER_PIN, HIGH);
      delay(durationMs / 2);
      digitalWrite(BUZZER_PIN, LOW);
      delay(20);
      digitalWrite(BUZZER_PIN, HIGH);
      delay(durationMs / 2);
      digitalWrite(BUZZER_PIN, LOW);
      break;
    case 3:
      digitalWrite(BUZZER_PIN, HIGH);
      delay(durationMs * 2);
      digitalWrite(BUZZER_PIN, LOW);
      break;
  }
}

// Функция сброса настроек к заводским
void factoryReset() {
  alarm_hour = 0;
  alarm_min = 0;
  alarm = false;
  night_start_hour = 23;
  night_end_hour = 7;
  buzzer_enabled = true;
  buzzer_sound_type = 1;
  ui_theme = 0;
  manual_night_mode = false;

  EEPROM.updateByte(0, alarm_hour);
  EEPROM.updateByte(1, alarm_min);
  EEPROM.updateByte(2, alarm);
  EEPROM.updateByte(3, night_start_hour);
  EEPROM.updateByte(4, night_end_hour);
  EEPROM.updateByte(5, buzzer_enabled);
  EEPROM.updateByte(6, buzzer_sound_type);
  EEPROM.updateByte(7, ui_theme);

  lcd.clear();
  lcd.setCursor(3, 1);
  lcd.print("FACTORY RESET!");
  
  digitalWrite(BUZZER_PIN, HIGH);
  delay(600);
  digitalWrite(BUZZER_PIN, LOW);
  delay(1000);
  
  set_time = 0;
  now_disp = 0;
  lcd.clear();
}

// Проверка активности ночного режима
boolean isNightMode() {
  if (manual_night_mode) return true;

  if (night_start_hour > night_end_hour) {
    return (now_hour >= night_start_hour || now_hour < night_end_hour);
  } else if (night_start_hour < night_end_hour) {
    return (now_hour >= night_start_hour && now_hour < night_end_hour);
  } else {
    return false; 
  }
}

//****************************
void setup() {
  Serial.begin(9600);
  
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SET, INPUT_PULLUP);
  pinMode(TTP_PIN, INPUT);

  pinMode(FOTORES, INPUT);
  pinMode(LCD_LED, OUTPUT);
  analogWrite(LCD_LED, 255);
  
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Чтение сохраненных настроек из EEPROM
  alarm_hour = EEPROM.readByte(0);
  alarm_min = EEPROM.readByte(1);
  alarm = EEPROM.readByte(2);
  night_start_hour = EEPROM.readByte(3);
  night_end_hour = EEPROM.readByte(4);
  buzzer_enabled = EEPROM.readByte(5);
  buzzer_sound_type = EEPROM.readByte(6);
  ui_theme = EEPROM.readByte(7);

  // Валидация диапазонов
  if (alarm_hour > 23) alarm_hour = 0;
  if (alarm_min > 59) alarm_min = 0;
  if (night_start_hour > 23) night_start_hour = 23;
  if (night_end_hour > 23) night_end_hour = 7;
  if (buzzer_sound_type < 1 || buzzer_sound_type > 3) buzzer_sound_type = 1;
  if (ui_theme > 2) ui_theme = 0;

  rtc.writeProtect(false);
  rtc.halt(false);
  
  lcd.init();
  lcd.backlight();
  lcd.clear();

  if (!bmp.begin()) {
    lcd.print("ERROR! NO BARO!!!");
    while (1) {}
  }
  
  for (int i=0; i<8; i++) lcd.createChar(i+1, custom[i]);
  
  writeBigString("CLOCK", 0, 0);
  writeBigString("2.3", 2, 2);
  lcd.setCursor(13,3);
  lcd.print("voltNik");

  buzzerClick(150);
  delay(1000);

  lcd.clear();
  time_read();
}

void(* resetFunc) (void) = 0;

//****************************
void loop() {
  now_millis = millis();

  // Снятие блокировки кнопок
  if (screen_transition && (now_millis - transition_millis > TRANSITION_DELAY)) {
    screen_transition = false;
  }
  
  // -------------------------------------------------------------
  // Обработка Сенсорной Кнопки TTP223 (Пин 2): ВХОД / ВЫХОД
  // -------------------------------------------------------------
  boolean ttp_state = digitalRead(TTP_PIN);
  if (ttp_state == HIGH && last_ttp_state == LOW && !screen_transition) {
    ttp_press_time = now_millis;
  }
  
  if (ttp_state == LOW && last_ttp_state == HIGH && !screen_transition) {
    if (now_millis - ttp_press_time > 50) { 
      buzzerClick(30);
      lcd.clear(); // Полный сброс экрана при входе/выходе
      if (!mode_stopwatch) {
        mode_stopwatch = true; // Вход в секундомер
        sw_running = false;
        sw_elapsed_time = 0;
      } else {
        mode_stopwatch = false; // Выход из секундомера
      }
    }
  }
  last_ttp_state = ttp_state;

  // -------------------------------------------------------------
  // Управление в режиме Секундомера (Пин 10 = Старт/Стоп, Пин 8 = Сброс)
  // -------------------------------------------------------------
  if (mode_stopwatch) {
    btn_set_val = digitalRead(BTN_SET);
    btn_up_val = digitalRead(BTN_UP);
    
    // Пин 10 -> Старт / Стоп (Пауза)
    if ((btn_set_val == LOW) && (now_millis - btn_set_millis > BTN_PROTECT)) {
      buzzerClick(30);
      if (!sw_running) {
        sw_start_time = now_millis - sw_elapsed_time;
        sw_running = true;
      } else {
        sw_elapsed_time = now_millis - sw_start_time;
        sw_running = false;
      }
      btn_set_millis = now_millis + 200;
    }

    // Пин 8 -> Сброс времени в 00:00:00
    if ((btn_up_val == LOW) && (now_millis - btn_up_millis > BTN_PROTECT)) {
      buzzerClick(60);
      sw_running = false;
      sw_elapsed_time = 0;
      lcd.clear();
      btn_up_millis = now_millis + 200;
    }
  } 
  // -------------------------------------------------------------
  // Управление в режиме Часов
  // -------------------------------------------------------------
  else {
    btn_up_val = digitalRead(BTN_UP);
    btn_down_val = digitalRead(BTN_DOWN);
    btn_set_val = digitalRead(BTN_SET);
    
    // --- Кнопка ВВЕРХ (Пин 8) ---
    if (btn_up_val == LOW) {
      if (!btn_up_pressed && !screen_transition) {
        btn_up_pressed = true;
        btn_up_press_time = now_millis;
        btn_up_long_triggered = false;
      } else if ((now_millis - btn_up_press_time > 1000) && set_time == 0 && !btn_up_long_triggered && !screen_transition) {
        btn_up_long_triggered = true;
        buzzerClick(100);
        set_time = 13; // Меню звука
        now_disp = 0; 
        disp_millis = now_millis;
        lcd.clear();
      }
    } else {
      if (btn_up_pressed) {
        if (!btn_up_long_triggered && (now_millis - btn_up_millis > BTN_PROTECT) && !screen_transition) {
          horn = false;
          disp_millis = now_millis;

          if (set_time == 0) {
            buzzer_enabled = !buzzer_enabled;
            EEPROM.updateByte(5, buzzer_enabled);
            buzzerClick(30);
          } else {
            buzzerClick(30);
            switch (set_time) {
              case 1: now_hour++; time_changed = true; if (now_hour >= 24) now_hour=0; break;
              case 2: now_min++; time_changed = true; if (now_min >= 60) now_min=0; break;
              case 3: now_sec = 0; time_changed = true; set_time_now(); disp_millis = now_millis; set_time = 0; lcd.clear(); break;
              case 4: alarm_hour++; if (alarm_hour >= 24) alarm_hour=0; break;
              case 5: alarm_min++; if (alarm_min >= 60) alarm_min=0; break;
              case 6: alarm = !alarm; break;
              case 7: now_year++; time_changed = true; if (now_year >= 2100) now_year=2000; break;
              case 8: now_month++; time_changed = true; if (now_month >= 13) now_month=1; break;
              case 9: now_date++; time_changed = true; if (now_date >= 32) now_date=1; break;
              case 10: now_week_day++; time_changed = true; if (now_week_day >= 7) now_week_day=0; break;
              case 11: night_start_hour++; if (night_start_hour >= 24) night_start_hour=0; break;
              case 12: night_end_hour++; if (night_end_hour >= 24) night_end_hour=0; break;
              case 13: buzzer_enabled = !buzzer_enabled; break;
              case 14: buzzer_sound_type++; if (buzzer_sound_type > 3) buzzer_sound_type = 1; break;
            }
          }
          btn_up_millis = now_millis;
        }
        btn_up_pressed = false;
      }
    }

    // --- Кнопка ВНИЗ (Пин 9) ---
    if (btn_down_val == LOW) {
      if (!btn_down_pressed && !screen_transition) {
        btn_down_pressed = true;
        btn_down_press_time = now_millis;
        btn_down_long_triggered = false;
      } else if ((now_millis - btn_down_press_time > 2000) && !btn_down_long_triggered && !screen_transition) {
        btn_down_long_triggered = true;
        factoryReset();
      }
    } else {
      if (btn_down_pressed) {
        if (!btn_down_long_triggered && (now_millis - btn_down_millis > BTN_PROTECT) && !screen_transition) {
          buzzerClick(30);
          horn = false;

          if (set_time == 0) {
            if (now_disp != 0) {
              now_disp = 0;
              disp_millis = now_millis;
              lcd.clear();
            } else {
              manual_night_mode = !manual_night_mode; 
              lcd.clear();
            }
          } else {
            switch (set_time) {
              case 1: now_hour--; time_changed = true; if (now_hour == 255) now_hour=23; break;
              case 2: now_min--; time_changed = true; if (now_min == 255) now_min=59; break;
              case 3: now_sec = 0; time_changed = true; set_time_now(); disp_millis = now_millis; set_time = 0; lcd.clear(); break;
              case 4: alarm_hour--; if (alarm_hour == 255) alarm_hour=23; break;
              case 5: alarm_min--; if (alarm_min == 255) alarm_min=59; break;
              case 6: alarm = !alarm; break;
              case 7: now_year--; time_changed = true; if (now_year < 2000) now_year=2099; break;
              case 8: now_month--; time_changed = true; if (now_month == 0) now_month=12; break;
              case 9: now_date--; time_changed = true; if (now_date == 0) now_date=31; break;
              case 10: if (now_week_day == 0 || now_week_day == 255) now_week_day=6; else now_week_day--; time_changed = true; break;
              case 11: night_start_hour--; if (night_start_hour == 255) night_start_hour=23; break;
              case 12: night_end_hour--; if (night_end_hour == 255) night_end_hour=23; break;
              case 13: buzzer_enabled = !buzzer_enabled; break;
              case 14: buzzer_sound_type--; if (buzzer_sound_type < 1) buzzer_sound_type = 3; break;
            }
          }
          btn_down_millis = now_millis;
        }
        btn_down_pressed = false;
      }
    }

    // --- Кнопка УСТАНОВКА (Пин 10) ---
    if (!screen_transition && (btn_set_val == LOW) && (now_millis - btn_set_millis > BTN_PROTECT)) {
      buzzerClick(40);
      horn = false;
      disp_millis = now_millis; 
      now_disp = 0;

      if (set_time >= 13) {
        if (set_time == 13) {
          set_time = 14;
        } else {
          set_time = 0;
          set_time_now();
          lcd.clear();
        }
      } else {
        set_time = (set_time + 1) % 13;
        lcd.clear();
        
        if (set_time == 0) {
          set_time_now();
        }
      }
      btn_set_millis = now_millis + 300;
    }

    // Секундный таймер
    if (now_millis - time_millis > 1000) {
      dot = !dot;
      if (dot) {sep = ':';} else {sep = '.';};
      if (set_time == 0) {
        time_read();
      }  
      set_lcd_led();
      if ((now_hour == alarm_hour) && (now_min == alarm_min) && (now_sec == 0) && (alarm)) { horn = true; }
      if ((now_hour != alarm_hour) || (now_min != alarm_min)) { horn = false; }

      if (millis() > 4000000000) { resetFunc(); }
      time_millis = now_millis;
    } 

    // Обработка будильника
    if (horn) {
      if (now_millis - horn_millis > 250) {
        note = !note;
        if (note) {
          digitalWrite(BUZZER_PIN, HIGH);
          analogWrite(LCD_LED, 255);
        } else {
          digitalWrite(BUZZER_PIN, LOW);
          analogWrite(LCD_LED, 0);
        }
        horn_millis = now_millis;
      }
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }

    // Автоматическая смена экранов
    if ((now_millis - disp_millis > disp[now_disp]) && (set_time == 0) && !isNightMode()) {
      now_disp = (now_disp + 1) % 4;
      lcd.clear();
      disp_millis = now_millis;
      
      screen_transition = true;
      transition_millis = now_millis;

      btn_up_pressed = false;
      btn_down_pressed = false;
      btn_up_millis = now_millis;
      btn_down_millis = now_millis;
      btn_set_millis = now_millis;
    }
  }

  // Обновление экрана
  if (now_millis - lcd_millis > LCD_RENEW) {
    if (mode_stopwatch) {
      print_stopwatch();
    } else {
      print_lcd();
    }
    lcd_millis = now_millis;
  } 
}

// Отображение секундомера: Крупные часы и минуты (HH:MM), маленькие секунды справа сверху
void print_stopwatch() {
  unsigned long current_time = sw_running ? (now_millis - sw_start_time) : sw_elapsed_time;
  
  unsigned long total_secs = current_time / 1000;
  int hrs = (total_secs / 3600) % 100;
  int mins = (total_secs / 60) % 60;
  int secs = total_secs % 60;

  // Крупные часы и минуты (HH:MM)
  char hm_str[6];
  snprintf(hm_str, sizeof(hm_str), "%02d%c%02d", hrs, sw_running ? ':' : '.', mins);
  writeBigString(hm_str, 0, 1);

  // Маленькие секунды (SS) в правом верхнем углу (строка 1, столбец 18)
  lcd.setCursor(18, 1);
  char sec_buf[3];
  snprintf(sec_buf, sizeof(sec_buf), "%02d", secs);
  lcd.print(sec_buf);

  // Очистка неиспользуемых строк для предотвращения наложения текста
  lcd.setCursor(0, 0);
  lcd.print("                    ");
  lcd.setCursor(0, 3);
  lcd.print("                    ");
}

// Отображение основного экрана и меню
void print_lcd(void) {
  char time_str[6], sec_str[3], date_str[19], pres_str[22], davl_str[5], temp_str[3], alarm_str[6], set_str;

  byte safe_day = now_week_day;
  if (safe_day > 6) safe_day = 0;

  snprintf(time_str, sizeof(time_str), "%02d%c%02d", now_hour, sep, now_min);
  snprintf(sec_str, sizeof(sec_str), "%02d", now_sec);
  snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d %s", now_year, now_month, now_date, week_day[safe_day]);
  dtostrf(now_press, 3, 0, davl_str);
  snprintf(temp_str, sizeof(temp_str), "%02d", now_temp);
  snprintf(pres_str, sizeof(pres_str), "T:%02dC P:%smm", now_temp, davl_str);
  snprintf(alarm_str, sizeof(alarm_str), "%02d:%02d", alarm_hour, alarm_min);
  set_str = alarm ? '+' : '-';

  if ((set_time != 0) && (blnk) && (set_time < 11)) {
    switch (set_time) {
      case 1: time_str[0]=' '; time_str[1]=' '; break;
      case 2: time_str[3]=' '; time_str[4]=' '; break;
      case 3: sec_str[0]=' '; sec_str[1]=' '; break;
      case 4: alarm_str[0]=' '; alarm_str[1]=' '; break;
      case 5: alarm_str[3]=' '; alarm_str[4]=' '; break;
      case 6: set_str=' '; break;
      case 7: date_str[0]=' '; date_str[1]=' '; date_str[2]=' '; date_str[3]=' '; break;
      case 8: date_str[5]=' '; date_str[6]=' '; break;
      case 9: date_str[8]=' '; date_str[9]=' '; break;
      case 10: date_str[11]=' '; date_str[12]=' '; date_str[13]=' '; break;
    }
  }
  blnk = !blnk;

  if (set_time == 11 || set_time == 12) {
    lcd.setCursor(1, 0);
    lcd.print("NIGHT MODE SETUP ");
    
    lcd.setCursor(1, 1);
    if (set_time == 11 && blnk) {
      lcd.print("Start Hour: --:00 ");
    } else {
      char st_buf[20];
      snprintf(st_buf, sizeof(st_buf), "Start Hour: %02d:00 ", night_start_hour);
      lcd.print(st_buf);
    }
    
    lcd.setCursor(1, 2);
    if (set_time == 12 && blnk) {
      lcd.print("End Hour:   --:00 ");
    } else {
      char end_buf[20];
      snprintf(end_buf, sizeof(end_buf), "End Hour:   %02d:00 ", night_end_hour);
      lcd.print(end_buf);
    }
    
    lcd.setCursor(1, 3);
    lcd.print("Press SET to Next ");
    return;
  }

  if (set_time == 13 || set_time == 14) {
    lcd.setCursor(1, 0);
    lcd.print("BUZZER SETTINGS  ");
    
    lcd.setCursor(1, 1);
    if (set_time == 13 && blnk) {
      lcd.print("Sound:      ---   ");
    } else {
      lcd.print("Sound:      ");
      lcd.print(buzzer_enabled ? "ON " : "OFF");
    }
    
    lcd.setCursor(1, 2);
    if (set_time == 14 && blnk) {
      lcd.print("Tone Type:  -     ");
    } else {
      char tone_buf[20];
      snprintf(tone_buf, sizeof(tone_buf), "Tone Type:  Type %d", buzzer_sound_type);
      lcd.print(tone_buf);
    }
    
    lcd.setCursor(1, 3);
    lcd.print("Press SET to Save ");
    return;
  }

  byte display_mode = (set_time != 0) ? 0 : now_disp;

  switch (display_mode) {
    case 0:
      if (ui_theme == 0) { 
        lcd.setCursor(0,0);
        lcd.print(date_str);
        lcd.setCursor(18,1);
        lcd.print(sec_str);
        writeBigString(time_str, 0, 1);
        lcd.setCursor(0,3);
        lcd.print(alarm_str);
        lcd.print(set_str);
        
        if (isNightMode()) {
          lcd.setCursor(6,3);
          lcd.print("[NIGHT]     ");
        } else {
          lcd.setCursor(7,3);
          lcd.print(pres_str);
        }
      } else if (ui_theme == 1) { 
        writeBigString(time_str, 1, 0);
        lcd.setCursor(18,1);
        lcd.print(sec_str);
        lcd.setCursor(1,2);
        lcd.print(date_str);
        lcd.setCursor(0,3);
        lcd.print("ALM:");
        lcd.print(alarm_str);
        lcd.print(set_str);
        lcd.print(isNightMode() ? " [NIGHT]" : "        ");
      } else if (ui_theme == 2) { 
        writeBigString(time_str, 0, 0);
        lcd.setCursor(18,0);
        lcd.print(sec_str);
        lcd.setCursor(0,2);
        char line3[21];
        snprintf(line3, sizeof(line3), "Temp:%02dC Pres:%s", now_temp, davl_str);
        lcd.print(line3);
        lcd.setCursor(0,3);
        char line4[21];
        snprintf(line4, sizeof(line4), "A:%s%c %s", alarm_str, set_str, week_day[safe_day]);
        lcd.print(line4);
      }
      break;

    case 1:
      writeBigString(davl_str, 1, 0); writeBigString("MM", 9, 2);
      break;
    case 2:
      writeBigString(time_str, 0, 0);
      writeBigString(time_str, 2, 2);
      break;
    case 3:
      writeBigString(temp_str, 4, 1); writeBigString("C", 13, 1);
      lcd.setCursor(0,3);
      lcd.print("Clock 2.3 by voltNik");
      break;
  }
}

void time_read() {
  t = rtc.time();
  now_year = t.yr;
  now_month = t.mon;
  now_date = t.date; 
  now_hour = t.hr;
  now_min = t.min;
  now_sec = t.sec;
  
  now_week_day = t.day;
  if (now_week_day > 6) {
    now_week_day = now_week_day % 7;
  }

  now_temp = bmp.readTemperature();
  now_press = bmp.readPressure()/133.3;
}

// Настройка яркости
void set_lcd_led() {
  if (set_time != 0 || mode_stopwatch) {
    analogWrite(LCD_LED, 255);
    return;
  }

  if (isNightMode()) {
    analogWrite(LCD_LED, 0);
    return;
  }
  
  int raw_light = analogRead(FOTORES);
  bright = map(raw_light, 50, 950, 5, 255); 
  bright = constrain(bright, 10, 255);
  
  analogWrite(LCD_LED, bright);
}

void set_time_now() {
  if (time_changed) {
    Time tt(now_year, now_month, now_date, now_hour, now_min, now_sec, now_week_day);
    rtc.time(tt);
  }
  time_changed = false;
  
  EEPROM.updateByte(0, alarm_hour);
  EEPROM.updateByte(1, alarm_min);
  EEPROM.updateByte(2, alarm);
  EEPROM.updateByte(3, night_start_hour);
  EEPROM.updateByte(4, night_end_hour);
  EEPROM.updateByte(5, buzzer_enabled);
  EEPROM.updateByte(6, buzzer_sound_type);
  EEPROM.updateByte(7, ui_theme);
}