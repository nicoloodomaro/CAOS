#ifndef CONFIG_PROFILE
#define CONFIG_PROFILE    4
#endif

#if (CONFIG_PROFILE == 1)
#include "timeline_config_1.c"
#elif (CONFIG_PROFILE == 2)
#include "timeline_config_2.c"
#elif (CONFIG_PROFILE == 3)
#include "timeline_config_3.c"
#elif (CONFIG_PROFILE == 4)
#include "timeline_config_4.c"
#elif (CONFIG_PROFILE == 5)
#include "timeline_config_5.c"
#elif (CONFIG_PROFILE == 6)
#include "timeline_config_6.c"
#elif (CONFIG_PROFILE == 7)
#include "timeline_config_7.c"
#elif (CONFIG_PROFILE == 8)
#include "timeline_config_8.c"
#elif (CONFIG_PROFILE == 9)
#include "timeline_config_9.c"
#elif (CONFIG_PROFILE == 10)
#include "timeline_config_10.c"
#elif (CONFIG_PROFILE == 11)
#include "timeline_config_11.c"
#else
#error "Unsupported CONFIG_PROFILE value. Use 1..11."
#endif
