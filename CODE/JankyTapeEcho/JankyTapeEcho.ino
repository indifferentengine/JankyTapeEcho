/*
 * Janky Tape Echo v2.00
 * Copyright (c) INDIFFERENT ENGINE Ltd. 2023
 * 
 * https://www.indifferentengine.com
 * 
 * Created by Adam Paul 27/12/2021
 */

//Useful for LFO
#define TWO_PI 6.283185307179586476925286766559

enum JankType {
  SINE,
  SQUARE,
  RANDOM,
  SNAG
};

enum SwitchType {
  LATCHING,
  MOMENTARY
};

/* ===================================================================================================================
 * ===================================================================================================================
 * ===================================================================================================================
 * CONFIGURE MACHINE FEATURES
 * Here you can configure the machine features.
 * 
 * CLEAN_DOWN_WHEN_DEACTIVATED 
 * Enables "auto clean-down" mode for the echo effect. When a tape echo is deactivated, the tape 
 * between the write head and read head will contain whatever audio data had already been written.
 * This means that when the effect is next engaged, that audio data will get played out. If using
 * loud, distorted high-gain sounds this can lead to a sudden loud feedback noise for a short time
 * when the effect is first engaged. All tape echos suffer from this problem. The "auto clear down" feature
 * looks to solve this issue; when enabled it will continue to run the motor for a few seconds after the effect
 * is disabled, thus passing any recorded audio past the read head and clearing it down with the erase head
 * so that you can be sure that, when the effect is next used, the tape will be blank.
 * 
 * CLEAN_DOWN_TIME_MILLISECONDS
 * How long the clean down should run for after deactivating the effect.
 * 
 * SWITCH_ON_MOTOR_KICK_TIME_MILLISECONDS
 * By running the motor at a high RPM for a few milliseconds whenever the effect is first engaged
 * it allows us to support long delay times (low RPM motor speeds) without stalling out the motor.
 * 
 * MIN_PWM_MOTOR_SPEED
 * The minimum motor speed (expressed as an 8 bit value, so a range of 0 to 255).
 * If your motor fails to spin when the TIME control is set to the longest time value
 * try increasing the the MIN_PWM_MOTOR_SPEED.
 * 
 * SWITCH_TYPE
 * What type of remote switch does this machine use? Defaults to MOMENTARY.
 * Momentary switching allows you to also use a "hold" mode :)
 */

const bool CLEAN_DOWN_WHEN_DEACTIVATED = false;
const int CLEAN_DOWN_TIME_MILLISECONDS = 1000;
const bool SWITCH_ON_MOTOR_KICK_ENABLED = true;
const int SWITCH_ON_MOTOR_KICK_TIME_MILLISECONDS = 50;
const int MIN_PWM_MOTOR_SPEED = 4;
const SwitchType SWITCH_TYPE = MOMENTARY;
/* ===================================================================================================================
 * ===================================================================================================================
 * ===================================================================================================================
 * JANKYNESS SETTINGS
 * Play with these settings to get different jankyness effects
 * 
 * SINE mode modulates the motor speed with a sine LFO.
 * SINE_JANK_MAX_HZ sets the maximum frequency of the LFO.
 * SINE_JANK_MAX_AMPLITUDE sets the LFOs max amplitude expressed as an 8-bit value.
 * The applied modulation increases both Hz and amplitude as the JANK control is turned up.
 * 
 * SQUARE mode modulates the motor speed with a square-wave LFO.
 * SQUARE_JANK_MAX_HZ sets the maximum frequency of the LFO.
 * SQUARE_JANK_MAX_AMPLITUDE sets the LFOs max amplitude expressed as 8-bit value.
 * The applied modulation increases both Hz and amplitude as the JANK control is turned up.
 * 
 * RANDOM mode modulates teh motor speed at random.
 * RANDOM_JANK_FREQUENCY sets the maximum frequency at which the speed is randomly changed.
 * RANDOM_JANK_MAX_AMPLITUDE sets the maximum speed offset from the speed determined by the TIME setting
 * The time between speed changes is random, but changes occur more frequently as the JANK control is turned up.
 * 
 * SNAG mode simulates tape snagging (those nice sudden pitch drops)
 * SNAG_MOTOR_KILL_TIME is how long to shut the motor off when a "snag" occurs (how long the tape is stuck for)
 * SNAG_JANK_MIN_DELAY is the minimum time that must elapse before another snag can occur.
 * SNAG_JANK_MAX_DELAY is the maximum time that can pass before another snag is heard.
 * The time between snags is random, but snags occur more frequently as the JANK control is turned up.
 * 
 */
//The type of jankyness we want.
const JankType jankType = RANDOM; //SINE, SQUARE, RANDOM, SNAG

//SINE mode settings
const float SINE_JANK_MAX_FREQUENCY = 10;
const float SINE_JANK_MAX_AMPLITUDE = 100;

//SQUARE mode settings
const float SQUARE_JANK_MAX_FREQUENCY = 10;
const float SQUARE_JANK_MAX_AMPLITUDE = 100;

//RANDOM mode settings
const float RANDOM_JANK_FREQUENCY = 4;
const float RANDOM_JANK_MAX_AMPLITUDE = 30;

//SNAG mode settings
const float SNAG_MOTOR_KILL_TIME = 20;
const float SNAG_JANK_MIN_DELAY = 300;
const float SNAG_JANK_MAX_DELAY = 4000;

/* ===================================================================================================================
 * ===================================================================================================================
 * ===================================================================================================================
*/

//===================================================================================================================
//These are configured against the PCB design.
//Not a good idea to change them unless you've a custom PCB
const int MOTOR_PIN = 5;
const int SWITCH_PIN = 6;
const int RELAY_PIN = 2;
//BIAS_ENABLE_PIN allows us to turn on or off the bias oscillator that drives the erase/record function of the tape machine.
//This is useful to reduce current draw, wear on the ferrous coating of the tape and noise levels whilst the pedal is bypassed.
//It also allows us to implement the "HOLD" function, where the bypass switch is held down in order to loop the contents of the tape.
const int BIAS_ENABLE_PIN = 8;

//===================================================================================================================
//Runtime switching vars
int switchOnCounter = 0;
int switchOffCounter = 0;
bool switchOn = false;
bool wasOn = false;
bool previousSwitchState = true;
unsigned long debounceTimeLast = 0;
unsigned long debounceTime = 50;
unsigned long holdTimeMin = 500;
unsigned long holdTime = 0;
bool switchHeld = false;

//===================================================================================================================
//runtime vars for jankyness
float jankValue = 0;
float pwm_Adjust = 0;
int minPwm = MIN_PWM_MOTOR_SPEED;  //this is so that we can raise the min PWM based on the jank control
int maxPwm = 30; //The motor is not linear in it's response, there's little difference in RPM between a duty cycle of 255 and 128. Therefore we use a low max value for pwm to make best use of the linear potentiometer range.
float nonLinearRange = 1000; //this is the pot value below which the motor response is considered non linear (to maximise resolution in low speed range)
float angle = 0;
int randomCount = 0;
int snagTime = 0;
bool snagged = false;
int oneSecondCount = 0;
bool builtInLEDToggle = true;

/* ===================================================================================================================
 * ===================================================================================================================
 * ===================================================================================================================
 * SETUP
 * This runs once on machine start-up, and configures the PWM frequency for the motor, the fixed 1KHz update loop (timer 2)
 * and the configures our IO pins.
*/
void setup() 
{
  
  /*
   * Set the motor PWM frequency
   * 
   * We use 62.5Khz. This is out of the audible frequency range, and above our bias frequency,
   * which makes it easy for us to use low pass filters to target both bias and PWM noise.
   */
  TCCR0B = TCCR0B & B11111000 | B00000001; //62500 Hz
  
  // Setup Timer 2 for interrupt frequency 1Khz
  // Timer 2 forms our fixed-rate update function
  cli(); // stop interrupts
  TCCR2A = 0; // set entire TCCR2A register to 0
  TCCR2B = 0; // same for TCCR2B
  TCNT2  = 0; // initialize counter value to 0
  // set compare match register for 1Khz increments
  OCR2A = 249; // = 16000000 / (64 * 1000) - 1 (must be <256)
  // turn on CTC mode
  TCCR2B |= (1 << WGM21);
  // Set CS22, CS21 and CS20 bits for 64 prescaler
  TCCR2B |= (1 << CS22) | (0 << CS21) | (0 << CS20);
  // enable timer compare interrupt
  TIMSK2 |= (1 << OCIE2A);
  sei(); // allow interrupts

  //Configure the various input and output pins we need 
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);       
  pinMode(BIAS_ENABLE_PIN, OUTPUT); 
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(MOTOR_PIN, OUTPUT);
  
 
  //Start up the serial port. This is useful for debugging.
  Serial.begin(115200);
  Serial.println("JANKY TAPE ECHO");
  Serial.println("Firmware version 2.00");
  Serial.println("(C) Indifferent Engine Ltd 2023");
  Serial.println("[the::compartment::is::flooded::with::radiation]");
}


/* ===================================================================================================================
 * ===================================================================================================================
 * ===================================================================================================================
 * LOOP
 * This runs every frame. In here we process IO and any non-time-dependent logic.
*/
void loop() {
  
  //read switch state, we use this to turn the effect on/off.
  bool switchState = digitalRead(SWITCH_PIN) == HIGH ? true : false;

  //Switching logic, with debounce
  if ((jankyMillis() - debounceTimeLast) > debounceTime) 
  {
     if (switchState != previousSwitchState) 
     {
        if (SWITCH_TYPE == LATCHING) 
        {
          switchOn = switchState;
        }
        else 
        {
          if (!switchState && 
                previousSwitchState) {
            //switch was just depressed, so set the hold time
            //so that we can calculate how long the switch has been held down for
            holdTime = jankyMillis();
          }
          if (switchState 
                && !previousSwitchState)
          {
            //switch was just released,
            //so perform the effect on/off toggle
            //Unless the switch was in the "held" mode, then we ignore the switch release
            if (!switchOn || !switchHeld) {
              switchOn = !switchOn;
            }
          }  
        }
        previousSwitchState = switchState;
        debounceTimeLast = jankyMillis();
     }
  }

  //When using momentary switching, we can set a hold mode to do something interesting when the switch is held down.
  if (SWITCH_TYPE == MOMENTARY
      && switchOn 
      && !switchState) 
  {
    if (jankyMillis() - holdTime > holdTimeMin) 
    {
      //Switch is being held, set the flag
      switchHeld = true;
    }
  }
  else {
    //Switch is not being held, clear the flag
    switchHeld = false;
  }

  //Some more switch logic
  if (wasOn && !switchOn) 
  {
    //switch was just deactivated this frame
    switchOffCounter = 0;
  }
  else if (!wasOn && switchOn) {
    //switch was just activated this frame
     switchOnCounter = 0;
  }

  //read the digital rotary controls for TIME and JANK
  //ADC on the nano is 10bit, meaning a range of 0 to 1024
  int timePotValue = analogRead(A0); //0->1024
  int jankPotValue = analogRead(A1); //0->1024
 
 
  //Get a normalised jank value (meaning between 0 and 1)
  jankValue = (1.0f / 1024) * jankPotValue;
  int pwmValue = 0;
  /*
   * The TIME value is used to work out what pwm value to send to the motor
   * In the v2, the motor controller is very non-linear, so we hack in a 
   * little non-linear response here to allow us to get more resolution 
   * out of the potentiometer. It levels back out to linear at the top
   * of the range.
   * 
   * [todo] make this not so... horrible. Maybe a function look up table?
  */
  if ((1024-timePotValue) < nonLinearRange) {

    //Map potentiometer ADC reading to PWM output value
    //we do this via an inverse parabola (x = y * y), resulting in y = sqrt(x)
    float minRad = TWO_PI/4.0f;
    float maxRad = TWO_PI/2.0f;
    float val = (((maxRad-minRad)/nonLinearRange) * (1024-timePotValue)) + minRad;
    
    float y = (sqrt((1024-timePotValue)/nonLinearRange));
    pwmValue = (int) (((maxPwm - minPwm) * y) + minPwm);
  }
  else {
    //A little bit of linear response at the top of the pot range
    pwmValue = ((255.0f - maxPwm) / (1024.0f - nonLinearRange)) * ((1024.0f-nonLinearRange)-timePotValue);
    pwmValue += maxPwm;
  } 

  //Apply the pwm_Adjust calculated from the Jank settings
  pwmValue += pwm_Adjust;
  pwmValue = switchOn ? constrain(pwmValue, minPwm, 255) : 0;  
  
  //if we're currently simulating a tape "snag", then turn off the motor
  pwmValue = snagged ? 0 : pwmValue;

  bool cleaningDown = false;
  //If tape auto-clean-down is activated, we handle that here.
  //If the effect is off but we're still cleaning down the tape then keep running the motor
  if (CLEAN_DOWN_WHEN_DEACTIVATED
      && !switchOn) 
  {
    pwmValue = switchOffCounter < CLEAN_DOWN_TIME_MILLISECONDS ? 255 : 0;
    cleaningDown = switchOffCounter < CLEAN_DOWN_TIME_MILLISECONDS ? true : false;   
  }

  //This kicks the motor at full power for the first few millis after the effect is switched on.
  //That way, if the TIME value is set long (i.e. low motor speed), the motor can get enough current
  //to actually start. This allows us to support much lower minimum PWM values than would otherwise
  //be possible
  if (switchOn 
      && SWITCH_ON_MOTOR_KICK_ENABLED
      && switchOnCounter < SWITCH_ON_MOTOR_KICK_TIME_MILLISECONDS) 
  {
    pwmValue = 255;
  }

  //write out pwm value to motor control pin
  analogWrite(MOTOR_PIN,pwmValue);
  
  //set pin values for switching relay, LEDs and what not.
  digitalWrite(LED_BUILTIN, builtInLEDToggle ? HIGH : LOW);
  //Set the true bypass switching to the correct position by controlling the relay
  digitalWrite(RELAY_PIN, switchOn ? HIGH : LOW);
  //Enable or disable the bias oscillator
  digitalWrite(BIAS_ENABLE_PIN, (switchOn && !switchHeld) || cleaningDown ? HIGH : LOW);
  
  //remember the previous state of the switch for comparison next frame
  wasOn = switchOn;
}

//Millis function that compensates for oscillator change
unsigned long jankyMillis() {
  return millis()/64;
}

/* ===================================================================================================================
 * ===================================================================================================================
 * ===================================================================================================================
 * ISR TIMER2
 * This runs every 1 millisecond. We do time-dependent work in here, like processing LFOs and what not.
 * Essentially, any logic that benefits from being run at a fixed rate.
*/
ISR(TIMER2_COMPA_vect){

  //
  //Handle our Jankyness calculations for various jankyness types.
  //
  if (jankType == SINE 
      || jankType == SQUARE)
  {
    float hZ = jankValue * (jankType == SINE ? SINE_JANK_MAX_FREQUENCY : SQUARE_JANK_MAX_FREQUENCY);
    float amplitude = jankValue * (jankType == SINE ? SINE_JANK_MAX_AMPLITUDE : SQUARE_JANK_MAX_AMPLITUDE);

    //We have to lower the maximum speed of the motor
    //Otherwise if the TIME control is set to minimum (motor at it's fastest)
    //then the jank control won't function correctly, as it will be trying to set the
    //pwm to 255 + the jank value. So we reduce the pwm by the amplitude of the jank LFO
    maxPwm = (int)(255.0f - amplitude);
    minPwm = (int)(MIN_PWM_MOTOR_SPEED + amplitude);
    
    //A simple LFO that can easily be converted into a sine wave.
    if (hZ != 0) {
      angle += (TWO_PI * hZ ) / 1000.0f; // interrupt happens at 1Khz, so /1000
      if (angle >=TWO_PI) 
      {
        angle -= TWO_PI;
      }
    }
    else 
    {
      angle = 0;
    }

    if (jankType == SINE) 
    {
      pwm_Adjust = sin(angle) * amplitude;
    }
    else 
    {
      pwm_Adjust = amplitude * (angle < PI ? -1.0f : 1.0f);
    }                       
    
  }
  else if (jankType == RANDOM) 
  {
    if (randomCount <= 0) 
    {
      float amplitude = jankValue * RANDOM_JANK_MAX_AMPLITUDE;
      pwm_Adjust = random(-amplitude*0.5f, amplitude*0.5f);
      randomCount = random((1.0f - jankValue) * (1000.0f/ RANDOM_JANK_FREQUENCY));
    }
    else 
    {
      randomCount -= 1;
    }   
  }
  else if (jankType == SNAG) 
  {
    if (snagTime > 0) {
      snagged = true;
      --snagTime;
    }
    else {    
      snagged = false;
      if (randomCount <= 0) 
      {
        float amplitude = jankValue * RANDOM_JANK_MAX_AMPLITUDE;
        pwm_Adjust = 0;
        randomCount = SNAG_JANK_MIN_DELAY + random((1.0f - jankValue) * SNAG_JANK_MAX_DELAY);
        snagTime = SNAG_MOTOR_KILL_TIME;
      }
      else 
      {
        randomCount -= 1;
      } 
    }
  }
  //incremement the time since switch was pressed
  if (switchOnCounter < SWITCH_ON_MOTOR_KICK_TIME_MILLISECONDS)
  { 
    ++switchOnCounter;
  }
  if (switchOffCounter < CLEAN_DOWN_TIME_MILLISECONDS)
  { 
    ++switchOffCounter;
  }

  //toggle the built in LED so we can see at a glance if our program is executing
  if (oneSecondCount >= 250) {
    builtInLEDToggle = !builtInLEDToggle;
    oneSecondCount = 0;
  }
  ++oneSecondCount;
}



//===================================================================================================================
//===================================================================================================================
//===================================================================================================================
//And that's it. Go buy a T-Shirt or something, alright?
//https://www.indifferentengine.com
