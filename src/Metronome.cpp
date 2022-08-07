#include "config.h"
#include <global.h>
#include <arduino-timer.h>
#include <ArduinoTapTempo.h>
//#include <driver/i2s.h>
#include <WiFi.h>
#include "HTTPClient.h"

#include <SPIFFS.h>

#include "AudioFileSourceSPIFFS.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
#include "config.h"

#define EXTERNAL_DAC_PLAY 1 /// from https://github.com/Xinyuan-LilyGO/TTGO_TWatch_Library/blob/master/examples/Shield/AlarmClock/AlarmClock.ino

AudioGeneratorMP3 *mp3;
AudioFileSourceSPIFFS *file;
AudioOutputI2S *out;
AudioFileSourceID3 *id3;

TTGOClass *ttgo;
TFT_eSPI *tft;

bool irq = false;
bool screen_power = true;

#define DEFAULT_SCREEN_TIMEOUT 20 * 1000 // setting for timeout before disabling screen

// taptempo lib object
ArduinoTapTempo tapTempo;

///   Metronome timer settings

auto bzz_timer = timer_create_default(); // tba - switch to microseconds.
int pulse_length = 200;                  // milliseconds  how long vibro motor will work for one click. Longer pulses are more powerful, as motor takes time to spin up, but it feels more blurry,

int bpm = 120;
#define MIN_BPM 40
#define MAX_BPM 240

#define MP3_CLICK "/hh.mp3" // mp3 file from spiffs data
#define AUDIO_VOLUME 3      //  3.5 is a max, I guess.

static lv_obj_t *sw_vibro;
static lv_obj_t *sw_audio;
static lv_obj_t *slider;
static lv_obj_t *slider_label;

bool metronome_running = false;

bool bzz(void *) // metronome click implementation
{
    Serial.print("bzz");

    //  Vibrate
    if (lv_switch_get_state(sw_vibro))
    {
        ttgo->motor->onec(pulse_length);
    }

    if (mp3->isRunning()) // need to stop previous mp3  before launching another.
        mp3->stop();

    // Play  audio "click"
    if (lv_switch_get_state(sw_audio))
    {
        file->open(MP3_CLICK);
        mp3->begin(id3, out);
    }

    return true; // That's required for timer.
}

int bpm_to_delay_ms(int bpm)
{
    return int(60 * 1000) / bpm;
}

void start_metronome()
{
    bzz_timer.every(bpm_to_delay_ms(bpm), bzz);
    metronome_running = true;
}

void stop_metronome()
{
    bzz_timer.cancel();
    metronome_running = false;
}

void set_bpm(int new_bpm)
{
    Serial.print("Setting BPM to ");Serial.println(new_bpm);
    bpm = new_bpm;
    static char buf[4]; /* max 3 bytes for number plus 1 null terminating byte */
    snprintf(buf, 4, "%u", bpm);
    lv_label_set_text(slider_label, buf);
    lv_slider_set_value(slider, bpm, LV_ANIM_ON);
   
    if (metronome_running)
    {
        stop_metronome(); // Refresh BPM requires restarting arduino timer tasks. Kinda junky, but surpisingly works.
        start_metronome();
    }
}

// --------------------   UI --------------------

// One handler for both audio and  vibro switches.
static void switch_event_handler(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED)
    {
        if ((lv_switch_get_state(sw_audio) || lv_switch_get_state(sw_vibro)) && !metronome_running)
        {
            Serial.println("Starting metronome");
            start_metronome();
        }

        if ((!lv_switch_get_state(sw_audio) && !lv_switch_get_state(sw_vibro)) && metronome_running)
        {
            Serial.println("Stopping metronome");
            stop_metronome();
        };
    }
}

// Draw UI switches
void gui_on_off_switches()
{

    // vibro switch
    sw_vibro = lv_switch_create(lv_scr_act(), NULL);
    lv_obj_align(sw_vibro, NULL, LV_ALIGN_CENTER, -10, +80);
    lv_obj_set_event_cb(sw_vibro, switch_event_handler);
    lv_obj_set_size(sw_vibro, 80, 40);

    /// sound switch
    sw_audio = lv_switch_create(lv_scr_act(), NULL);
    lv_obj_align(sw_audio, NULL, LV_ALIGN_CENTER, -10, -90);
    lv_obj_set_event_cb(sw_audio, switch_event_handler);
    lv_obj_set_size(sw_audio, 80, 40);
}

// BPM slider


// handler
static void slider_event_cb(lv_obj_t *slider, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED)
    {

        set_bpm(lv_slider_get_value(slider));
    }
}

// silder ui
void gui_bpm_slider()
{

    // slider itself
    slider = lv_slider_create(lv_scr_act(), NULL);
    lv_obj_set_width(slider, LV_DPI * 2 - 20);
    lv_obj_set_height(slider, 24);
    lv_obj_align(slider, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_event_cb(slider, slider_event_cb);
    lv_slider_set_range(slider, MIN_BPM, MAX_BPM);
    lv_slider_set_value(slider, bpm, LV_ANIM_ON);

    // label with BPM value
    slider_label = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_text(slider_label, "120");
    lv_obj_set_auto_realign(slider_label, true);
    lv_obj_align(slider_label, slider, LV_ALIGN_OUT_BOTTOM_MID, 0, -70);

    // font settings for readable lable
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_text_font(&style, LV_STATE_DEFAULT, &lv_font_montserrat_22); // <--- you have to enable other font sizes in menuconfig
    lv_obj_add_style(slider_label, LV_LABEL_PART_MAIN, &style);               // <--- obj is the label
}

// fine tune tempo buttons

static void dec_tempo_button_handler(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED)
    {
        printf("Dec bpm\n");
        set_bpm(bpm - 1);

       // lv_slider_set_value(slider, bpm, LV_ANIM_ON);
    }
}

static void inc_tempo_button_handler(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED)
    {
        printf("Dec bpm\n");
        set_bpm(bpm + 1);
       // lv_slider_set_value(slider, bpm, LV_ANIM_ON);
    }
}

static void tap_tempo_button_handler(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED)
    {
        printf("tap temp\n");

        tapTempo.update(true); tapTempo.update(false);  //simulate button tap and release
        int tap_bpm = int(tapTempo.getBPM());
        printf("detected tempo %d\n", tap_bpm );
        set_bpm(tap_bpm);
       // lv_slider_set_value(slider, bpm, LV_ANIM_ON);
    }
}


void gui_dec_bpm_btn(void)
{
    lv_obj_t *label;

    lv_obj_t *dec_bpm = lv_btn_create(lv_scr_act(), NULL);
    lv_obj_set_event_cb(dec_bpm, dec_tempo_button_handler);
    lv_obj_align(dec_bpm, NULL, LV_ALIGN_IN_TOP_MID, -40, 10);
    lv_obj_set_height(dec_bpm, 50);
    lv_obj_set_width(dec_bpm, 50);
    label = lv_label_create(dec_bpm, NULL);
    lv_label_set_text(label, "-");
}

void gui_inc_bpm_btn(void)
{
    lv_obj_t *label;

    lv_obj_t *inc_bpm = lv_btn_create(lv_scr_act(), NULL);
    lv_obj_set_event_cb(inc_bpm, inc_tempo_button_handler);
    lv_obj_align(inc_bpm, NULL, LV_ALIGN_IN_TOP_MID, +120, 10);
    lv_obj_set_height(inc_bpm, 50);
    lv_obj_set_width(inc_bpm, 50);
    label = lv_label_create(inc_bpm, NULL);
    lv_label_set_text(label, "+");
}

void gui_tap_bpm_btn(void)
{
    lv_obj_t *label;

    lv_obj_t *tap_bpm = lv_btn_create(lv_scr_act(), NULL);
    lv_obj_set_event_cb(tap_bpm, tap_tempo_button_handler);
    lv_obj_align(tap_bpm, NULL, LV_ALIGN_IN_TOP_MID, +130, +160);
    lv_obj_set_height(tap_bpm, 50);
    lv_obj_set_width(tap_bpm, 50);
    label = lv_label_create(tap_bpm, NULL);
    lv_label_set_text(label, "tap");
}


void create_gui()
{
    gui_on_off_switches();
    gui_bpm_slider();
    gui_dec_bpm_btn();
    gui_inc_bpm_btn();
    gui_tap_bpm_btn();
}

// Reduce power usage by turning off screen when no user input
void low_energy()
{
    Serial.println("Low power");
    ttgo->power->setPowerOutPut(AXP202_LDO2, false);
    ttgo->displaySleep();
    ttgo->closeBL();
    ttgo->stopLvglTick();
    screen_power = false;
    if (!metronome_running)
    {
        Serial.println("Bye");
        ttgo->shutdown(); // If metronome is stopped, just shutdown the device completely. I hope.
    }
}

// Enable screen and touchpad
void high_energy()
{

    Serial.println("High power");
    ttgo->power->setPowerOutPut(AXP202_LDO2, true);
    ttgo->displayWakeup();
    ttgo->openBL();
    ttgo->startLvglTick();
    lv_disp_trig_activity(NULL);
    lv_tick_inc(LV_DISP_DEF_REFR_PERIOD); /*Force task execution on wake-up*/
                                          /*Restart the timer where lv_tick_inc() is called*/
    lv_task_handler();                    /*Call `lv_task_handler()` manually to process the wake-up event*/
}

void process_powerbutton()
{
    if (irq)
    {
        irq = false;
        ttgo->power->readIRQ();
        if (ttgo->power->isPEKShortPressIRQ())
        {
            Serial.println("Powerbutton");
            screen_power = !screen_power;
            if (screen_power)
            {
                high_energy();
            }
            else
            {
                low_energy();
            }
        }
        ttgo->power->clearIRQ();
    }
}

void setup()
{
    Serial.begin(115200);
    WiFi.mode(WIFI_OFF);
    delay(500);

    if (SPIFFS.begin())
    {
        Serial.println("SPIFFS MOUNTED");
    }
    else
    {
        Serial.println("SPIFFS MOUNT FAIL");
    }
    if (SPIFFS.exists("/hh.mp3"))
    {
        Serial.println("FILE EXIST");
    }
    else
    {
        Serial.println("FILE NOT EXIST");
    }

    // Get TTGOClass instance
    ttgo = TTGOClass::getWatch();

    // Initialize the hardware
    ttgo->begin();
    // Turn on the backlight
    //  ttgo->openBL();

    ////    graphics
    Serial.printf("init LVGL\r\n");
    ttgo->lvgl_begin();

    // Receive objects for easy writing
    tft = ttgo->tft;

    ttgo->tft->fillScreen(TFT_BLACK);
    create_gui();

    // axp202 allows maximum charging current of 1800mA, minimum 300mA
    // tft->print("Set charge control current 500 mA");
    ttgo->power->setChargeControlCur(300);

    //! begin motor attach to 33 pin , In Tttgo-2020 it is IO4
    ttgo->motor_begin();

    /////  audio click setup

    ttgo->enableLDO3(); // enable power for audio output

    file = new AudioFileSourceSPIFFS("/hh.mp3");
    id3 = new AudioFileSourceID3(file);

#if defined(STANDARD_BACKPLANE)
    out = new AudioOutputI2S(0, 1);
#elif defined(EXTERNAL_DAC_BACKPLANE)

    out = new AudioOutputI2S();
    out->SetGain(AUDIO_VOLUME);

    // External DAC decoding
    out->SetPinout(TWATCH_DAC_IIS_BCK, TWATCH_DAC_IIS_WS, TWATCH_DAC_IIS_DOUT);
#endif

    mp3 = new AudioGeneratorMP3();

    //   Powerbutton to turn off the screen

    pinMode(AXP202_INT, INPUT_PULLUP);
    attachInterrupt(
        AXP202_INT, []
        { irq = true; },
        FALLING);
    //! Clear IRQ unprocessed  first
    ttgo->power->enableIRQ(AXP202_PEK_SHORTPRESS_IRQ | AXP202_VBUS_REMOVED_IRQ | AXP202_VBUS_CONNECT_IRQ | AXP202_CHARGING_IRQ, true);
    ttgo->power->clearIRQ();
    high_energy();
    Serial.print("Setup finished");
}

void loop()
{
    // lvgl loop
    if (screen_power)
        lv_task_handler();
    // timer loop
    bzz_timer.tick();

    // mp3 loop
    if (mp3->isRunning())
        mp3->loop();

    process_powerbutton();

    //  turn off the screen if no input
    if (lv_disp_get_inactive_time(NULL) > DEFAULT_SCREEN_TIMEOUT)
        low_energy();
}