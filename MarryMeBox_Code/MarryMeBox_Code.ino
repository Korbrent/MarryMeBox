/**
 * If anyone is reading this code, good luck.
 * It was not written with the intent to be read by others.
 * Comments were written for myself and without thought to future readers.
 * - Kor
 */

#include <Servo.h>
#define SERVO_PIN 9 // Must be a PWM enabled pin
Servo servo; // Servo motor object

// OLED
/** Useful links
 * https://cdn-learn.adafruit.com/downloads/pdf/adafruit-gfx-graphics-library.pdf
 * https://adafruit.github.io/Adafruit-GFX-Library/html/class_adafruit___g_f_x.html
 * https://www.instructables.com/How-to-Display-Images-on-OLED-Using-Arduino/
 */
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <string.h>

// OLED init
#define i2c_Address 0x3c
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1   //   QT-PY / XIAO
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// OLED constants
#define CHAR_WIDTH 5
#define CHAR_HEIGHT 7
#define LINE_GAP 2
#define MESSAGE_BUFFER 25
const int NUM_ROWS = floor((double) SCREEN_HEIGHT / (CHAR_HEIGHT + LINE_GAP));

#define error errorBlink(); return;

// OLED variables
int current_font_size = 1;
int current_rows[NUM_ROWS];
char message_box[MESSAGE_BUFFER];
int message_len;

// Program States
#define INITIAL 0
#define DECLINED 1
#define ACCEPTED 2

// Button Pinouts
const int yes_button = 3;
const int no_button = 2;

// Debouncing button state
int no_button_state;
int last_no_button_state = LOW;
unsigned long last_debounce_time = 0;
unsigned long debounce_delay = 50;

volatile int state = INITIAL; // INITIAL, DECLINED, and ACCEPTED
int loop_count = 0;

// Blink the light in event of a bug or error
void errorBlink(){
  int k = 15;
  while(k-- >= 0){
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);
    delay(25);
  }
}

///////////////// MESSAGE FUNCTIONS /////////////////

// Load the message box
void loadMessageBox(char *str){
  int i;
  for(i = 0; i < (MESSAGE_BUFFER - 1) && str[i] != '\0'; i++){
    message_box[i] = str[i];
  }
  message_box[i] = '\0'; // Either we ran out of buffer space or hit the null terminator. Either way, its the null terminator
  message_len = i;
}

// Set cursor position
void setCursor(int row, bool isCentered){
  int x, y;
  y = row * (CHAR_HEIGHT + LINE_GAP);
  if(isCentered) {
    x = getCenteredX();
  } else {
    x = 0;
  }
  display.setCursor(x, y);
}

// Get the centered coordinate for the x value.
int getCenteredX() {
  int center = ((double) SCREEN_WIDTH / 2);
  int char_width = CHAR_WIDTH * current_font_size;
  int str_width = (message_len) * (char_width + current_font_size);
  int centering_offset = ((double) str_width / 2);
  int x = center - centering_offset;
  return x;
}

// Refreshes the display
void refresh(){
  // delay(250);
  display.display();
}

// Clears a single row from the LED
void clearRow(int row){
  int y = row * (CHAR_HEIGHT + LINE_GAP);
  int x;
  for(int i = y; i < y + (CHAR_HEIGHT + LINE_GAP); i++) {
    for(x = 0; x < SCREEN_WIDTH; x++) {
      display.drawPixel(x, i, 0);
    }
  }
  current_rows[row] = -1;
  // refresh();
}

// Clears the display
void clearDisplay(){
  display.clearDisplay();
  for(int i = 0; i < NUM_ROWS; i++)
    current_rows[i] = -1;
  // refresh();
}

// Sets the font size
void setFontSize(int font_size){
  current_font_size = font_size;
  display.setTextSize(font_size);
}

/**
 * Sets the cursor to the right location then prints the message.
 * Assumes everything has already been cleaned up.
 */
void setMessage(bool isCentered, int row){
  setCursor(row, isCentered);
  display.println(message_box);
}

/**
 * Queues a message for display. Cleans up messages occupying the way before calling setMessage.
 */
void queueMessage(bool isCentered, int row, int font_size){
  if(font_size < 1)
    font_size = 1;

  int char_width = CHAR_WIDTH * font_size;
  int char_per_row = floor((double) SCREEN_WIDTH / (char_width + font_size));
  int lines = ceil((double) message_len / char_per_row);
  int rows_occupied = font_size; // Changed to deal with not autosplitting

  // If more than one line or out of range, not supported.
  if ((lines > 1) || (row + (rows_occupied - 1) >= NUM_ROWS)) {
    error
  }
  
  int last_row, start_row;
  if(current_rows[row] != -1){
    // This row is active, it must be cleared.
		start_row = current_rows[row];
		int i = row;
		// Get all the occupied rows.
		while(current_rows[i] == start_row)
      i++;
		last_row = i - 1;	
  } else {
    start_row = row;
    last_row = row;
  }

  if(last_row < row + (rows_occupied - 1))
    last_row = row + (rows_occupied - 1);

  if((current_rows[last_row] != -1) && (current_rows[last_row] != start_row)){
    // More to clear.
    int i = last_row;
    while(current_rows[i] == current_rows[last_row])
      i++;
  
    last_row = i - 1; // Here is the true last index.
  }
  
  // Clean up on aisle 2
  for(int i = start_row; i <= last_row; i++)
    clearRow(i);

  // Mark our claim
  for(int i = row; i < row + rows_occupied; i++)
    current_rows[i] = row;

  setFontSize(font_size);
  if(lines == 1){
    setMessage(isCentered, row);
    return;
  }
  else {
    error
  }
}

///////////////// BITMAP SHENANIGANS /////////////////

#define BITMAP_WIDTH 29
#define BITMAP_HEIGHT 25
#define NUMFLAKES 5
#define XPOS 0
#define YPOS 1
#define DELTAY 2
int screen = 0;

const unsigned char heart_bitmap [] PROGMEM = {
	0x07, 0xc0, 0x1f, 0x00, 0x1f, 0xf0, 0x7f, 0xc0, 0x3f, 0xf8, 0xfc, 0xe0, 0x7f, 0xfd, 0xfe, 0x70, 
	0x7f, 0xfd, 0xff, 0x30, 0xff, 0xff, 0xff, 0xb8, 0xfe, 0xef, 0xc7, 0xf8, 0xfe, 0xef, 0xbb, 0xf8, 
	0xfe, 0xdd, 0xbb, 0xf8, 0xfe, 0x38, 0xbb, 0xf8, 0x7e, 0xdd, 0xab, 0xf0, 0x7e, 0xef, 0xb7, 0xf0, 
	0x3e, 0xef, 0xcb, 0xe0, 0x1f, 0xff, 0xff, 0xc0, 0x0f, 0xff, 0xff, 0x80, 0x07, 0xff, 0xff, 0x00, 
	0x03, 0xff, 0xfe, 0x00, 0x01, 0xff, 0xfc, 0x00, 0x00, 0xff, 0xf8, 0x00, 0x00, 0x7f, 0xf0, 0x00, 
	0x00, 0x3f, 0xe0, 0x00, 0x00, 0x1f, 0xc0, 0x00, 0x00, 0x0f, 0x80, 0x00, 0x00, 0x07, 0x00, 0x00, 
	0x00, 0x02, 0x00, 0x00
};

const unsigned char pigsnout_78x64 [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xff, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0xff, 0x00, 0x03, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xf8, 0x00, 0x00, 0x7f, 0x80, 
	0x00, 0x00, 0x00, 0x00, 0x1f, 0x80, 0x00, 0x00, 0x07, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00, 
	0x00, 0x00, 0x01, 0xf8, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x00, 
	0x00, 0x01, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x03, 0xc0, 0x00, 0x00, 0x00, 
	0x00, 0x0f, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x80, 0x00, 0x00, 0x0e, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0xe0, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x70, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x00, 
	0x01, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x06, 0x00, 0x03, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x03, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x03, 0x80, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x0e, 0x00, 0x00, 0xf0, 
	0x00, 0x00, 0x3c, 0x00, 0x01, 0xc0, 0x0c, 0x00, 0x03, 0xfc, 0x00, 0x00, 0xff, 0x00, 0x00, 0xc0, 
	0x1c, 0x00, 0x07, 0xfe, 0x00, 0x01, 0xff, 0x80, 0x00, 0xe0, 0x18, 0x00, 0x0f, 0xfe, 0x00, 0x01, 
	0xff, 0xc0, 0x00, 0x60, 0x38, 0x00, 0x1f, 0xff, 0x00, 0x03, 0xff, 0xe0, 0x00, 0x70, 0x30, 0x00, 
	0x3f, 0xff, 0x00, 0x03, 0xff, 0xf0, 0x00, 0x30, 0x30, 0x00, 0x3f, 0xff, 0x80, 0x07, 0xff, 0xf0, 
	0x00, 0x30, 0x30, 0x00, 0x7f, 0xff, 0x80, 0x07, 0xff, 0xf8, 0x00, 0x30, 0x70, 0x00, 0x7f, 0xff, 
	0x80, 0x07, 0xff, 0xf8, 0x00, 0x38, 0x60, 0x00, 0x7f, 0xff, 0x80, 0x07, 0xff, 0xf8, 0x00, 0x18, 
	0x60, 0x00, 0xff, 0xff, 0x80, 0x07, 0xff, 0xfc, 0x00, 0x18, 0x60, 0x00, 0xff, 0xff, 0x80, 0x07, 
	0xff, 0xfc, 0x00, 0x18, 0x60, 0x00, 0xff, 0xff, 0x80, 0x07, 0xff, 0xfc, 0x00, 0x18, 0x60, 0x01, 
	0xff, 0xff, 0x80, 0x07, 0xff, 0xfe, 0x00, 0x18, 0x60, 0x01, 0xff, 0xff, 0x00, 0x03, 0xff, 0xfe, 
	0x00, 0x18, 0x60, 0x01, 0xff, 0xff, 0x00, 0x03, 0xff, 0xfe, 0x00, 0x18, 0x60, 0x01, 0xff, 0xfe, 
	0x00, 0x01, 0xff, 0xfe, 0x00, 0x18, 0x60, 0x01, 0xff, 0xfe, 0x00, 0x01, 0xff, 0xfe, 0x00, 0x18, 
	0x60, 0x00, 0xff, 0xfe, 0x00, 0x01, 0xff, 0xfc, 0x00, 0x18, 0x60, 0x00, 0xff, 0xfc, 0x00, 0x00, 
	0xff, 0xfc, 0x00, 0x18, 0x60, 0x00, 0x7f, 0xf8, 0x00, 0x00, 0x7f, 0xf8, 0x00, 0x18, 0x70, 0x00, 
	0x7f, 0xf8, 0x00, 0x00, 0x7f, 0xf8, 0x00, 0x38, 0x30, 0x00, 0x3f, 0xf0, 0x00, 0x00, 0x3f, 0xf0, 
	0x00, 0x30, 0x30, 0x00, 0x0f, 0xc0, 0x00, 0x00, 0x0f, 0xc0, 0x00, 0x30, 0x38, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 
	0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0xe0, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xc0, 0x07, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x80, 0x07, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x07, 0x80, 0x03, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x01, 0xe0, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 
	0x00, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x1f, 0x80, 0x00, 0x00, 0x00, 
	0x00, 0x07, 0xe0, 0x00, 0x00, 0x07, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x80, 0x00, 0x00, 0x00, 
	0xff, 0x00, 0x00, 0x00, 0x03, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xe0, 
	0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void snowfall(const uint8_t *bitmap, uint8_t w, uint8_t h) {
  uint8_t icons[NUMFLAKES][3];

  // initialize
  for (uint8_t f = 0; f < NUMFLAKES; f++) {
    icons[f][XPOS] = random(display.width());
    icons[f][YPOS] = 0;
    icons[f][DELTAY] = random(5) + 1;
  }

  // Snowflake screen
  while (screen == 0) {
    // draw each icon
    for (uint8_t f = 0; f < NUMFLAKES; f++) {
      display.drawBitmap(icons[f][XPOS], icons[f][YPOS], bitmap, w, h, SH110X_WHITE);
    }
    display.display();
    delay(200);

    // then erase it + move it
    for (uint8_t f = 0; f < NUMFLAKES; f++) {
      display.drawBitmap(icons[f][XPOS], icons[f][YPOS], bitmap, w, h, SH110X_BLACK);
      // move it
      icons[f][YPOS] += icons[f][DELTAY];
      // if its gone, reinit
      if (icons[f][YPOS] > display.height()) {
        icons[f][XPOS] = random(display.width());
        icons[f][YPOS] = 0;
        icons[f][DELTAY] = random(5) + 1;
      }
    }
  }

  // "Hidden" screen 
  if(screen == 1){
    clearDisplay();
    refresh();
    int w = 78;
    int h = 64;
    int cursor_x, cursor_y;
    cursor_x = ((double) (SCREEN_WIDTH - w) / 2);
    cursor_y = ((double) (SCREEN_HEIGHT - h) / 2);
    display.drawBitmap(cursor_x, cursor_y, pigsnout_78x64, w, h, SH110X_WHITE);
    display.display();
    delay(100);
    while(1);
  }
}

void screen_change(){
  detachInterrupt(digitalPinToInterrupt(no_button));
  screen = 1;
}

///////////////// BUTTON FUNCTIONALITY /////////////////

void yes_pushed(){
  detachInterrupt(digitalPinToInterrupt(yes_button));
  if(state == INITIAL)
    detachInterrupt(digitalPinToInterrupt(no_button));
  
  state = ACCEPTED;
}

void no_pushed(){
  detachInterrupt(digitalPinToInterrupt(no_button));
  state = DECLINED;
  loop_count = 0;
  loadMessageBox("Are you");
  queueMessage(true, 1, 2);
}

int last_change = -1;
void no_repeater(){
  if(loop_count != last_change) {
    last_change = loop_count; 
    switch(loop_count){
      case 0:
        loadMessageBox("sure?");
        queueMessage(true, 4, 2);
        break;
      case 1:
        loadMessageBox("really");
        queueMessage(true, 3, 2);
        loadMessageBox("sure?");
        queueMessage(true, 5, 2);
        break;
      case 2:
        loadMessageBox("absolutely");
        queueMessage(true, 3, 2);
        // loadMessageBox("sure?");
        // queueMessage(true, 5, 2);
        break;
      case 3:
        loadMessageBox("positively");
        queueMessage(true, 3, 2);
        // loadMessageBox("sure?");
        // queueMessage(true, 5, 2);
        break;
    }
    refresh();
  }
}

void declineLoop(){
  if(debouncerDeclineLoop())
    loop_count = (loop_count + 1) % 4;
  no_repeater();
}

bool debouncerDeclineLoop(){
  // Read the state of the no button
  int current_no_button_state = digitalRead(no_button);

  // If the switch changed (due to noise or pressing), reset the counter
  if(current_no_button_state != last_no_button_state){
    last_debounce_time = millis();
  }

  // If the counter is longer than the debounce delay without changing states, it's a "real" signal
  if((millis() - last_debounce_time) > debounce_delay){

    // If the button has changed, update the button state
    if(current_no_button_state != no_button_state) {
      no_button_state = current_no_button_state;

      if(no_button_state == HIGH){
        return true;
      }
    }
  }

  last_no_button_state = current_no_button_state;
  return false;
}

///////////////// SETUP & LOOPS /////////////////

#define INITIAL_ANGLE 180
#define OPENED_ANGLE 50

void setup() {
  pinMode(LED_BUILTIN, OUTPUT); // Error LED
  pinMode(yes_button, INPUT);   // YES button pin
  pinMode(no_button, INPUT);    // NO button pin
  servo.attach(SERVO_PIN);
  display.begin(i2c_Address, true);
  display.setTextColor(SH110X_WHITE);
  display.setTextWrap(false);
  refresh();
  servo.write(INITIAL_ANGLE);
  
  // Pin interrupts for button press
  attachInterrupt(digitalPinToInterrupt(yes_button), yes_pushed, RISING);
  attachInterrupt(digitalPinToInterrupt(no_button), no_pushed, RISING);

  clearDisplay();
  loadMessageBox("Will you");
  queueMessage(true, 1, 2);
  refresh();
}



void initial_loop(){
  switch(loop_count % 4){
    case 0:
      loadMessageBox("marry me?");
      break;
    case 1:
      loadMessageBox("be mine?");
      break;
    case 2:
      loadMessageBox("love me?");
      break;
    case 3:
      loadMessageBox("say yes?");
      break;
  }
  loop_count++;
  queueMessage(true, 4, 2);
  refresh();
}

// Main function
void loop() {
  switch(state){
    case INITIAL:
      initial_loop();
      delay(1000);
      break;
    
    case DECLINED:
      declineLoop();
      break;
      
    case ACCEPTED:
      clearDisplay();
      loadMessageBox("Congratulations");
      queueMessage(true, 4, 1);
      
      for(int i = INITIAL_ANGLE; i >= OPENED_ANGLE; i--){
        servo.write(i);
        delay(10);
      }
      attachInterrupt(digitalPinToInterrupt(no_button), screen_change, RISING);
      snowfall(heart_bitmap, BITMAP_WIDTH, BITMAP_HEIGHT);
      break;
  }
}
