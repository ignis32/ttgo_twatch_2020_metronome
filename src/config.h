// => Hardware select
// #define LILYGO_WATCH_2019_WITH_TOUCH         // To use T-Watch2019 with touchscreen, please uncomment this line
// #define LILYGO_WATCH_2019_NO_TOUCH           // To use T-Watch2019 Not touchscreen , please uncomment this line
#define LILYGO_WATCH_2020_V1                 // To use T-Watch2020 V1, please uncomment this line
// #define LILYGO_WATCH_2020_V2                 // To use T-Watch2020 V2, please uncomment this line
// #define LILYGO_WATCH_2020_V3                 // To use T-Watch2020 V3, please uncomment this line


// Hardware NOT SUPPORT
//// #define LILYGO_WATCH_BLOCK
// Hardware NOT SUPPORT
   

    #define LILYGO_WATCH_LVGL               //To use LVGL, you need to enable the macro LVGL
  //  #define INTERRUPT_ATTR IRAM_ATTR
    //#define TWATCH_USE_PSRAM_ALLOC_LVGL   //allows for more available RAM, impacts performance
    //#define TWATCH_LVGL_DOUBLE_BUFFER     
    //#define LVGL_BUFFER_SIZE        (240*240) //full frame buffer use 115kB
 //   #define LVGL_BUFFER_SIZE        (240*60)
 //   #define ENABLE_LVGL_FLUSH_DMA



//audio

// Except T-Watch2020, other versions need to be selected according to the actual situation
#if  !defined(LILYGO_WATCH_2020_V1) && !defined(LILYGO_WATCH_2020_V3)

// T-Watch comes with the default backplane, it uses internal DAC
#define STANDARD_BACKPLANE

// Such as MAX98357A, PCM5102 external DAC backplane
// #define EXTERNAL_DAC_BACKPLANE

#else
// T-Watch2020 uses external DAC
#undef STANDARD_BACKPLANE
#define EXTERNAL_DAC_BACKPLANE

#endif


#include <LilyGoWatch.h>