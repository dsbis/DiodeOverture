#define FHT_N 64 // For microphone input
#define LOG_OUT 1 
#define PIN 3
#define INPUT_SIZE 256 // For guitar input

#include <Adafruit_NeoPixel.h>
#include <FHT.h>

const float SAMPLE_FREQ = 8919; // Sample Frequency in kHz
const int MICROPHONE_SAMPLE_SIZE = 5;
const long GUITAR_THRESHOLD = 130;

// Autocorrelation variables
byte guitarInput[INPUT_SIZE];
int count;
int len = sizeof(guitarInput);
int i, k;
long sum, sum_old;
long thresh = 0;
double freq_per = 0;
byte pd_state = 0;

// Neopixel variables
int numLights = 60;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(numLights, PIN, NEO_GRB + NEO_KHZ800);
uint32_t off = strip.Color(0,0,0);

// Helmet light segment helper variables
int leftEar_start = 0;
int leftEar_end = 13;
int head_start = 14;
int head_end = 45;
int rightEar_start = 46;
int rightEar_end = 60;

// Additional helper variables
int brightnessMonitor = 0;
int microphoneSum = 0;

/**
 * Set-Up the System
 */
void setup() {
  Serial.begin(115200);

  strip.begin();
  strip.setBrightness(20); //adjust brightness here
  strip.show(); // Initialize all pixels to 'off'

  for(int i = 0; i < INPUT_SIZE; i++) {
    guitarInput[i] = 0;
  }
}

/**
 * Constantly loop the code
 */
void loop() {

  // Retrieve microphone data as fht_log_out array
  getMicrophoneReadings();

  // Retrieve guitar data as freq_per
  getGuitarFrequency();

  // Avoid noise pollution and only process microphone data from when lights are off
  brightnessMonitor = (brightnessMonitor + 1) % MICROPHONE_SAMPLE_SIZE;
  microphoneSum += fht_log_out[2];
  
  if(brightnessMonitor == 0) {
    int avg = microphoneSum / MICROPHONE_SAMPLE_SIZE;
    int brightness = map(avg, 0, 150, 0, 255); // The input min and input max values (0 && 150) were retreived from empirical testing, they might differ in different contexts
    brightness = constrain(brightness, 0, 255);
    microphoneSum = 0;
    strip.setBrightness(brightness);
  }
  
  // Only update if a frequency was detected and playing is detected
  if(freq_per < 1100 && thresh > GUITAR_THRESHOLD) {
    uint32_t newColor = map(freq_per, 80,1100, 0, 42000);
    strip.fill(strip.ColorHSV(newColor), 0, 0);
  } else {  
    strip.fill(off, 0, 0);
  }
  strip.show();
}

/***********************************/
/**        UTILITY METHODS        **/
/***********************************/

/**
 * Gets the microphone frequency spectrum and stores it into fht_log_out
 * Adapted from example code from Open Music Labs
 * http://wiki.openmusiclabs.com/wiki/FHTExample
 * Each element of the array represent a specific frequency band
 * Lower frequencies (elements 0 and 1) tend to always record high due to typical room noise, their use isn't recommended
 * Example response peaks by element
 * 2 - 262 Hz
 * 3 - 392 Hz
 * 4 - 550 Hz
 * 5 - 700 Hz
 * 6 - 830 Hz
 * 7 - 950 Hz
 * 8 - 1100 Hz
 */
void getMicrophoneReadings() {
  cli();  // UDRE interrupt slows this way down on arduino1.0
  for (int i = 0 ; i < FHT_N ; i++) { // save 256 samples
    int k = analogRead(A5);   
    fht_input[i] = k; // put real data into bins
  }
  fht_window(); // window the data for better frequency response
  fht_reorder(); // reorder the data before doing the fht
  fht_run(); // process the data in the fht
  fht_mag_log(); // take the output of the fht
  sei();
  Serial.write(255); // send a start byte
  Serial.write(fht_log_out, FHT_N/2); // send out the data
}

/**
 * Collect guitar signal and process using auto-correlate algorithm adapted from MrMark code
 * https://forum.arduino.cc/index.php?topic=540969.msg3687113#msg3687113
 * Result is stored in freq_per variable
 */
void getGuitarFrequency() {
   // Collect analog signal for autocorrelation
  for (count = 0; count < INPUT_SIZE; count++) {
    guitarInput[count] = analogRead(A2) >> 2;
  }

  // Calculate mean to remove DC offset
  long meanSum = 0 ;
  for (count = 0; count < INPUT_SIZE; count++) {
    meanSum += guitarInput[count] ;
  }
  int mean = meanSum / INPUT_SIZE ;

  // Autocorrelation Logic
  sum = 0;
  int period = 0;
  for (i = 0; i < len; i++)
  {
    // Autocorrelation
    sum_old = sum;
    sum = 0;
    for (k = 0; k < len - i; k++) sum += (guitarInput[k] - mean) * (guitarInput[k + i] - mean);

    // Peak Detect State Machine 
    if (pd_state == 2 && (sum - sum_old) <= 0)
    {
      period = i;
      pd_state = 3; 
    }
    if (pd_state == 1 && (sum > thresh) && (sum - sum_old) > 0) pd_state = 2;
    if (!i) {
      thresh = sum * 0.5;
      pd_state = 1;
    }
  }
  // Frequency identified in Hz
  if (thresh > GUITAR_THRESHOLD) {
    freq_per = SAMPLE_FREQ / period;
  }
}

/**
 * Sets the entire LED strip to the set color
 */
void colorFill(uint32_t c){  
  for(uint16_t i=0; i<strip.numPixels(); i++) {  
    strip.setPixelColor(i, c);  
  }  
  strip.show();  
}
