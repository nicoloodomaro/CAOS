#ifndef CONFIG_PROFILE
#define CONFIG_PROFILE    4
#endif

#if (CONFIG_PROFILE == 1)
#include "../test_config/test_1.c"
#elif (CONFIG_PROFILE == 2)
#include "../test_config/test_2.c"
#elif (CONFIG_PROFILE == 3)
#include "../test_config/test_3.c"
#elif (CONFIG_PROFILE == 4)
#include "../test_config/test_4.c"
#elif (CONFIG_PROFILE == 5)
#include "../test_config/test_5.c"
#elif (CONFIG_PROFILE == 6)
#include "../test_config/test_6.c"
#elif (CONFIG_PROFILE == 7)
#include "../test_config/test_7.c"
#elif (CONFIG_PROFILE == 8)
#include "../test_config/test_8.c"
#elif (CONFIG_PROFILE == 9)
#include "../test_config/test_9.c"
#elif (CONFIG_PROFILE == 10)
#include "../test_config/test_10.c"
#elif (CONFIG_PROFILE == 11)
#include "../test_config/test_11.c"
#elif (CONFIG_PROFILE == 12)
#include "../generated/timeline_config_generated.c"
#else
#error "Unsupported CONFIG_PROFILE value. Use 1..12."
#endif
