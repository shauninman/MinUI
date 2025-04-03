#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define CCU_BASE    0x01C20000

enum _state {
    S_DISABLED = 0,
    S_CHANGED,
    S_NOT_CHANGED
} ;

enum _type {
    T_CORE = 0,
    T_SCH,
    T_CPU,
    T_GPU,
    T_RAM,
    T_SWAP
};

struct _cpu_clock {
    int clk;
    uint32_t reg;
};

static const char *sch_name[] = {
    "interactive", 
    "conservative",
    "userspace",
    "ondemand",
    "performance",
    ""
};

static struct _cpu_clock gpu_clock[] = {
    1, 0x8000000c,
    2, 0x80000008,
    3, 0x80000006,
    4, 0x80000004,
    5, 0x80000108,
    6, 0x80000003,
    7, 0x80000209,
    8, 0x80000002,
    9, 0x80000104,
    10, 0x80000206,
    11, 0x8000050c,
    12, 0x80000001,
    13, 0x80000306,
    14, 0x80000204,
    15, 0x80000407,
    16, 0x80000102,
    17, 0x80000406,
    18, 0x80000203,
    19, 0x80000304,
    20, 0x80000405,
    21, 0x80000607,
    22, 0x80000a0b,
    24, 0x80000000,
    25, 0x80000d0c,
    26, 0x80000908,
    27, 0x80000706,
    28, 0x80000504,
    29, 0x80000a08,
    30, 0x80000403,
    31, 0x80000c09,
    32, 0x80000302,
    33, 0x80000604,
    34, 0x80000906,
    35, 0x8000120c,
    36, 0x80000201,
    37, 0x80000a06,
    38, 0x80000704,
    39, 0x80000c07,
    40, 0x80000402,
    41, 0x80000b06,
    42, 0x80000603,
    43, 0x80000804,
    44, 0x80000a05,
    45, 0x80000e07,
    46, 0x8000160b,
    48, 0x80000100,
    49, 0x80001a0c,
    50, 0x80001208,
    51, 0x80000e06,
    52, 0x80000a04,
    53, 0x80001308,
    54, 0x80000803,
    55, 0x80001609,
    56, 0x80000602,
    57, 0x80000b04,
    58, 0x80001006,
    59, 0x80001f0c,
    60, 0x80000401,
    61, 0x80001106,
    62, 0x80000c04,
    63, 0x80001407,
    64, 0x80000702,
    65, 0x80001206,
    66, 0x80000a03,
    67, 0x80000d04,
    68, 0x80001005,
    69, 0x80001607,
    70, 0x8000220b,
    72, 0x80000200,
    73, 0x8000270c,
    74, 0x80001b08,
    75, 0x80001506,
    76, 0x80000f04,
    77, 0x80001c08,
    78, 0x80000c03,
    79, 0x80002009,
    80, 0x80000902,
    81, 0x80001004,
    82, 0x80001706,
    83, 0x80002c0c,
    84, 0x80000601,
    85, 0x80001806,
    86, 0x80001104,
    87, 0x80001c07,
    88, 0x80000a02,
    89, 0x80001906,
    90, 0x80000e03,
    91, 0x80001204,
    92, 0x80001605,
    93, 0x80001e07,
    94, 0x80002e0b,
    96, 0x80000300,
    97, 0x8000340c,
    98, 0x80002408,
    99, 0x80001c06,
    100, 0x80001404,
    101, 0x80002508,
    102, 0x80001003,
    103, 0x80002a09,
    104, 0x80000c02,
    105, 0x80001504,
    106, 0x80001e06,
    107, 0x8000390c,
    108, 0x80000801,
    109, 0x80001f06,
    110, 0x80001604,
    111, 0x80002407,
    112, 0x80000d02,
    113, 0x80002006,
    114, 0x80001203,
    115, 0x80001704,
    116, 0x80001c05,
    117, 0x80002607,
    118, 0x80003a0b,
    120, 0x80000400,
    121, 0x8000410c,
    122, 0x80002d08,
    123, 0x80002306,
    124, 0x80001904,
    125, 0x80002e08,
    126, 0x80001403,
    127, 0x80003409,
    128, 0x80000f02,
    129, 0x80001a04,
    130, 0x80002506,
    131, 0x8000460c,
    132, 0x80000a01,
    133, 0x80002606,
    134, 0x80001b04,
    135, 0x80002c07,
    136, 0x80001002,
    137, 0x80002706,
    138, 0x80001603,
    139, 0x80001c04,
    140, 0x80002205,
    141, 0x80002e07,
    142, 0x8000460b,
    144, 0x80000500,
    145, 0x80004e0c,
    146, 0x80003608,
    147, 0x80002a06,
    148, 0x80001e04,
    149, 0x80003708,
    150, 0x80001803,
    151, 0x80003e09,
    152, 0x80001202,
    153, 0x80001f04,
    154, 0x80002c06,
    155, 0x8000530c,
    156, 0x80000c01,
    157, 0x80002d06,
    158, 0x80002004,
    159, 0x80003407,
    160, 0x80001302,
    161, 0x80002e06,
    162, 0x80001a03,
    163, 0x80002104,
    164, 0x80002805,
    165, 0x80003607,
    166, 0x8000520b,
    168, 0x80000600,
    169, 0x80005b0c,
    170, 0x80003f08,
    171, 0x80003106,
    172, 0x80002304,
    173, 0x80004008,
    174, 0x80001c03,
    175, 0x80004809,
    176, 0x80001502,
    177, 0x80002404,
    178, 0x80003306,
    179, 0x8000600c,
    180, 0x80000e01,
    181, 0x80003406,
    182, 0x80002504,
    183, 0x80003c07,
    184, 0x80001602,
    185, 0x80003506,
    186, 0x80001e03,
    187, 0x80002604,
    188, 0x80002e05,
    189, 0x80003e07,
    190, 0x80005e0b,
    192, 0x80000700,
    193, 0x8000680c,
    194, 0x80004808,
    195, 0x80003806,
    196, 0x80002804,
    197, 0x80004908,
    198, 0x80002003,
    199, 0x80005209,
    200, 0x80001802,
    201, 0x80002904,
    202, 0x80003a06,
    203, 0x80006d0c,
    204, 0x80001001,
    205, 0x80003b06,
    206, 0x80002a04,
    207, 0x80004407,
    208, 0x80001902,
    209, 0x80003c06,
    210, 0x80002203,
    211, 0x80002b04,
    212, 0x80003405,
    213, 0x80004607,
    214, 0x80006a0b,
    216, 0x80000800,
    217, 0x8000750c,
    218, 0x80005108,
    219, 0x80003f06,
    220, 0x80002d04,
    221, 0x80005208,
    222, 0x80002403,
    223, 0x80005c09,
    224, 0x80001b02,
    225, 0x80002e04,
    226, 0x80004106,
    227, 0x80007a0c,
    228, 0x80001201,
    229, 0x80004206,
    230, 0x80002f04,
    231, 0x80004c07,
    232, 0x80001c02,
    233, 0x80004306,
    234, 0x80002603,
    235, 0x80003004,
    236, 0x80003a05,
    237, 0x80004e07,
    238, 0x8000760b,
    240, 0x80000900,
    242, 0x80005a08,
    243, 0x80004606,
    244, 0x80003204,
    245, 0x80005b08,
    246, 0x80002803,
    247, 0x80006609,
    248, 0x80001e02,
    249, 0x80003304,
    250, 0x80004806,
    252, 0x80001401,
    253, 0x80004906,
    254, 0x80003404,
    255, 0x80005407,
    256, 0x80001f02,
    257, 0x80004a06,
    258, 0x80002a03,
    259, 0x80003504,
    260, 0x80004005,
    261, 0x80005607,
    264, 0x80000a00,
    266, 0x80006308,
    267, 0x80004d06,
    268, 0x80003704,
    269, 0x80006408,
    270, 0x80002c03,
    271, 0x80007009,
    272, 0x80002102,
    273, 0x80003804,
    274, 0x80004f06,
    276, 0x80001601,
    277, 0x80005006,
    278, 0x80003904,
    279, 0x80005c07,
    280, 0x80002202,
    281, 0x80005106,
    282, 0x80002e03,
    283, 0x80003a04,
    284, 0x80004605,
    285, 0x80005e07,
    288, 0x80000b00,
    290, 0x80006c08,
    291, 0x80005406,
    292, 0x80003c04,
    293, 0x80006d08,
    294, 0x80003003,
    295, 0x80007a09,
    296, 0x80002402,
    297, 0x80003d04,
    298, 0x80005606,
    300, 0x80001801,
    301, 0x80005706,
    302, 0x80003e04,
    303, 0x80006407,
    304, 0x80002502,
    305, 0x80005806,
    306, 0x80003203,
    307, 0x80003f04,
    308, 0x80004c05,
    309, 0x80006607,
    312, 0x80000c00,
    314, 0x80007508,
    315, 0x80005b06,
    316, 0x80004104,
    317, 0x80007608,
    318, 0x80003403,
    320, 0x80002702,
    321, 0x80004204,
    322, 0x80005d06,
    324, 0x80001a01,
    325, 0x80005e06,
    326, 0x80004304,
    327, 0x80006c07,
    328, 0x80002802,
    329, 0x80005f06,
    330, 0x80003603,
    331, 0x80004404,
    332, 0x80005205,
    333, 0x80006e07,
    336, 0x80000d00,
    338, 0x80007e08,
    339, 0x80006206,
    340, 0x80004604,
    341, 0x80007f08,
    342, 0x80003803,
    344, 0x80002a02,
    345, 0x80004704,
    346, 0x80006406,
    348, 0x80001c01,
    349, 0x80006506,
    350, 0x80004804,
    351, 0x80007407,
    352, 0x80002b02,
    353, 0x80006606,
    354, 0x80003a03,
    355, 0x80004904,
    356, 0x80005805,
    357, 0x80007607,
    360, 0x80000e00,
    363, 0x80006906,
    364, 0x80004b04,
    366, 0x80003c03,
    368, 0x80002d02,
    369, 0x80004c04,
    370, 0x80006b06,
    372, 0x80001e01,
    373, 0x80006c06,
    374, 0x80004d04,
    375, 0x80007c07,
    376, 0x80002e02,
    377, 0x80006d06,
    378, 0x80003e03,
    379, 0x80004e04,
    380, 0x80005e05,
    381, 0x80007e07,
    384, 0x80000f00,
    387, 0x80007006,
    388, 0x80005004,
    390, 0x80004003,
    392, 0x80003002,
    393, 0x80005104,
    394, 0x80007206,
    396, 0x80002001,
    397, 0x80007306,
    398, 0x80005204,
    400, 0x80003102,
    401, 0x80007406,
    402, 0x80004203,
    403, 0x80005304,
    404, 0x80006405,
    408, 0x80001000,
    411, 0x80007706,
    412, 0x80005504,
    414, 0x80004403,
    416, 0x80003302,
    417, 0x80005604,
    418, 0x80007906,
    420, 0x80002201,
    421, 0x80007a06,
    422, 0x80005704,
    424, 0x80003402,
    425, 0x80007b06,
    426, 0x80004603,
    427, 0x80005804,
    428, 0x80006a05,
    432, 0x80001100,
    435, 0x80007e06,
    436, 0x80005a04,
    438, 0x80004803,
    440, 0x80003602,
    441, 0x80005b04,
    444, 0x80002401,
    446, 0x80005c04,
    448, 0x80003702,
    450, 0x80004a03,
    451, 0x80005d04,
    452, 0x80007005,
    456, 0x80001200,
    460, 0x80005f04,
    462, 0x80004c03,
    464, 0x80003902,
    465, 0x80006004,
    468, 0x80002601,
    470, 0x80006104,
    472, 0x80003a02,
    474, 0x80004e03,
    475, 0x80006204,
    476, 0x80007605,
    480, 0x80001300,
    484, 0x80006404,
    486, 0x80005003,
    488, 0x80003c02,
    489, 0x80006504,
    492, 0x80002801,
    494, 0x80006604,
    496, 0x80003d02,
    498, 0x80005203,
    499, 0x80006704,
    500, 0x80007c05,
    504, 0x80001400,
    508, 0x80006904,
    510, 0x80005403,
    512, 0x80003f02,
    513, 0x80006a04,
    516, 0x80002a01,
    518, 0x80006b04,
    520, 0x80004002,
    522, 0x80005603,
    523, 0x80006c04,
    528, 0x80001500,
    532, 0x80006e04,
    534, 0x80005803,
    536, 0x80004202,
    537, 0x80006f04,
    540, 0x80002c01,
    542, 0x80007004,
    544, 0x80004302,
    546, 0x80005a03,
    547, 0x80007104,
    552, 0x80001600,
    556, 0x80007304,
    558, 0x80005c03,
    560, 0x80004502,
    561, 0x80007404,
    564, 0x80002e01,
    566, 0x80007504,
    568, 0x80004602,
    570, 0x80005e03,
    571, 0x80007604,
    576, 0x80001700,
    580, 0x80007804,
    582, 0x80006003,
    584, 0x80004802,
    585, 0x80007904,
    588, 0x80003001,
    590, 0x80007a04,
    592, 0x80004902,
    594, 0x80006203,
    595, 0x80007b04,
    600, 0x80001800,
    604, 0x80007d04,
    606, 0x80006403,
    608, 0x80004b02,
    609, 0x80007e04,
    612, 0x80003201,
    614, 0x80007f04,
    616, 0x80004c02,
    618, 0x80006603,
    624, 0x80001900,
    630, 0x80006803,
    632, 0x80004e02,
    636, 0x80003401,
    640, 0x80004f02,
    642, 0x80006a03,
    648, 0x80001a00,
    654, 0x80006c03,
    656, 0x80005102,
    660, 0x80003601,
    664, 0x80005202,
    666, 0x80006e03,
    672, 0x80001b00,
    678, 0x80007003,
    680, 0x80005402,
    684, 0x80003801,
    688, 0x80005502,
    690, 0x80007203,
    696, 0x80001c00,
    702, 0x80007403,
    704, 0x80005702,
    708, 0x80003a01,
    712, 0x80005802,
    714, 0x80007603,
    720, 0x80001d00,
    726, 0x80007803,
    728, 0x80005a02,
    732, 0x80003c01,
    736, 0x80005b02,
    738, 0x80007a03,
    744, 0x80001e00,
    750, 0x80007c03,
    752, 0x80005d02,
    756, 0x80003e01,
    760, 0x80005e02,
    762, 0x80007e03,
    768, 0x80001f00,
    776, 0x80006002,
    780, 0x80004001,
    784, 0x80006102,
    792, 0x80002000,
    800, 0x80006302,
    804, 0x80004201,
    808, 0x80006402,
    816, 0x80002100,
    824, 0x80006602,
    828, 0x80004401,
    832, 0x80006702,
    840, 0x80002200,
    848, 0x80006902,
    852, 0x80004601,
    856, 0x80006a02,
    864, 0x80002300,
    872, 0x80006c02,
    876, 0x80004801,
    880, 0x80006d02,
    888, 0x80002400,
    896, 0x80006f02,
    900, 0x80004a01,
    904, 0x80007002,
    912, 0x80002500,
    920, 0x80007202,
    924, 0x80004c01,
    928, 0x80007302,
    936, 0x80002600,
    944, 0x80007502,
    948, 0x80004e01,
    952, 0x80007602,
    960, 0x80002700,
    968, 0x80007802,
    972, 0x80005001,
    976, 0x80007902,
    984, 0x80002800,
    992, 0x80007b02,
    996, 0x80005201,
    1000, 0x80007c02,
    1008, 0x80002900,
    1016, 0x80007e02,
    1020, 0x80005401,
    1024, 0x80007f02,
    1032, 0x80002a00,
    1044, 0x80005601,
    1056, 0x80002b00,
    1068, 0x80005801,
    1080, 0x80002c00,
    1092, 0x80005a01,
    1104, 0x80002d00,
    1116, 0x80005c01,
    1128, 0x80002e00,
    1140, 0x80005e01,
    1152, 0x80002f00,
    1164, 0x80006001,
    1176, 0x80003000,
    1188, 0x80006201,
    1200, 0x80003100,
    1212, 0x80006401,
    1224, 0x80003200,
    1236, 0x80006601,
    1248, 0x80003300,
    1260, 0x80006801,
    1272, 0x80003400,
    1284, 0x80006a01,
    1296, 0x80003500,
    1308, 0x80006c01,
    1320, 0x80003600,
    1332, 0x80006e01,
    1344, 0x80003700,
    1356, 0x80007001,
    1368, 0x80003800,
    1380, 0x80007201,
    1392, 0x80003900,
    1404, 0x80007401,
    1416, 0x80003a00,
    1428, 0x80007601,
    1440, 0x80003b00,
    1452, 0x80007801,
    1464, 0x80003c00,
    1476, 0x80007a01,
    1488, 0x80003d00,
    1500, 0x80007c01,
    1512, 0x80003e00,
    1524, 0x80007e01,
    1536, 0x80003f00,
    1560, 0x80004000,
    1584, 0x80004100,
    1608, 0x80004200,
    1632, 0x80004300,
    1656, 0x80004400,
    1680, 0x80004500,
    1704, 0x80004600,
    1728, 0x80004700,
    1752, 0x80004800,
    1776, 0x80004900,
    1800, 0x80004a00,
    1824, 0x80004b00,
    1848, 0x80004c00,
    1872, 0x80004d00,
    1896, 0x80004e00,
    1920, 0x80004f00,
    1944, 0x80005000,
    1968, 0x80005100,
    1992, 0x80005200,
    2016, 0x80005300,
    2040, 0x80005400,
    2064, 0x80005500,
    2088, 0x80005600,
    2112, 0x80005700,
    2136, 0x80005800,
    2160, 0x80005900,
    2184, 0x80005a00,
    2208, 0x80005b00,
    2232, 0x80005c00,
    2256, 0x80005d00,
    2280, 0x80005e00,
    2304, 0x80005f00,
    2328, 0x80006000,
    2352, 0x80006100,
    2376, 0x80006200,
    2400, 0x80006300,
    2424, 0x80006400,
    2448, 0x80006500,
    2472, 0x80006600,
    2496, 0x80006700,
    2520, 0x80006800,
    2544, 0x80006900,
    2568, 0x80006a00,
    2592, 0x80006b00,
    2616, 0x80006c00,
    2640, 0x80006d00,
    2664, 0x80006e00,
    2688, 0x80006f00,
    2712, 0x80007000,
    2736, 0x80007100,
    2760, 0x80007200,
    2784, 0x80007300,
    2808, 0x80007400,
    2832, 0x80007500,
    2856, 0x80007600,
    2880, 0x80007700,
    2904, 0x80007800,
    2928, 0x80007900,
    2952, 0x80007a00,
    2976, 0x80007b00,
    3000, 0x80007c00,
    3024, 0x80007d00,
    3048, 0x80007e00,
    3072, 0x80007f00,
};

static struct _cpu_clock cpu_clock[] = {
    96, 0x80000110,
    144, 0x80000120,
    192, 0x80000130,
    216, 0x80000220,
    240, 0x80000410,
    288, 0x80000230,
    336, 0x80000610,
    360, 0x80000420,
    384, 0x80000330,
    432, 0x80000520,
    480, 0x80000430,
    504, 0x80000620,
    528, 0x80000a10,
    576, 0x80000530,
    624, 0x80000c10,
    648, 0x80000820,
    672, 0x80000630,
    720, 0x80000920,
    768, 0x80000730,
    792, 0x80000a20,
    816, 0x80001010,
    864, 0x80000830,
    864, 0x80001110,
    912, 0x80001210,
    936, 0x80000c20,
    960, 0x80000930,
    1008, 0x80000d20,
    1056, 0x80000a30,
    1080, 0x80000e20,
    1104, 0x80001610,
    1152, 0x80000b30,
    1200, 0x80001810,
    1224, 0x80001020,
    1248, 0x80000c30,
    1296, 0x80001120,
    1344, 0x80000d30,
    1368, 0x80001220,
    1392, 0x80001c10,
    1440, 0x80000e30,
    1488, 0x80001e10,
    1512, 0x80001420,
    1536, 0x80000f30,
    1584, 0x80001520,
    1632, 0x80001030,
    1656, 0x80001620,
    1728, 0x80001130,
    1800, 0x80001820,
    1824, 0x80001230,
    1872, 0x80001920,
    1920, 0x80001330,
    1944, 0x80001a20,
    2016, 0x80001430,
    2088, 0x80001c20,
    2112, 0x80001530,
    2160, 0x80001d20,
    2208, 0x80001630,
    2232, 0x80001e20,
    2304, 0x80001730,
    2400, 0x80001830,
    2496, 0x80001930,
    2592, 0x80001a30,
    2688, 0x80001b30,
    2784, 0x80001c30,
    2880, 0x80001d30,
    2976, 0x80001e30,
    3072, 0x80001f30,
};

static int cur_sel = 0;
static int max_cpu_item = sizeof(cpu_clock) / sizeof(struct _cpu_clock);
static int max_gpu_item = sizeof(gpu_clock) / sizeof(struct _cpu_clock);

static uint8_t *pmem = NULL;
static int mem_fd = -1;
static int core_val[2] = {0};
static int cpu_val[2] = {0};
static int gpu_val[2] = {0};
static int ram_val[2] = {0};
static int sch_val[2] = {0};
static int swap_val[2] = {0};

static int get_core(int index)
{
    FILE *fd = NULL;
    char buf[255] = {0};

    sprintf(buf, "cat /sys/devices/system/cpu/cpu%d/online", index % 4);
    fd = popen(buf, "r");
    if (fd == NULL) {
        return -1;
    }
    fgets(buf, sizeof(buf), fd);
    pclose(fd);
    return atoi(buf);
}

static enum _state ret_state(int v0, int v1)
{
    if (v1 <= 0) {
        return S_DISABLED;
    }
    return v1 == v0 ? S_NOT_CHANGED : S_CHANGED;
}

static enum _state get_state(enum _type t)
{
    if (t == T_SCH) {
        return ret_state(sch_val[0], sch_val[1]);
    }
    else if (t == T_CORE) {
        return ret_state(core_val[0], core_val[1]);
    }
    else if (t == T_CPU) {
        return ret_state(cpu_val[0], cpu_val[1]);
    }
    else if (t == T_GPU) {
        return ret_state(gpu_val[0], gpu_val[1]);
    }
    else if (t == T_RAM) {
        return ret_state(ram_val[0], ram_val[1]);
    }
    else if (t == T_SWAP) {
        return ret_state(swap_val[0], swap_val[1]);
    }
}

static int find_best_match_cpu_clock(int clk)
{
    int cc = 0;

    for (cc = 0; cc < max_cpu_item; cc++) {
        if (cpu_clock[cc].clk >= clk) {
            // printf("Found Best Match CPU %dMHz (0x%08x)\n", cpu_clock[cc].clk, cpu_clock[cc].reg);
            return cc;
        }
    }
    return -1;
}

static int find_best_match_gpu_clock(int clk)
{
    int cc = 0;

    for (cc = 0; cc < max_gpu_item; cc++) {
        if (gpu_clock[cc].clk >= clk) {
            // printf("Found Best Match GPU %dMHz (0x%08x)\n", gpu_clock[cc].clk, gpu_clock[cc].reg);
            return cc;
        }
    }
    return -1;
}

static void read_value(void)
{
    uint32_t m, p, n, k;
    uint32_t v = 0;
    uint32_t p_idx[] = {1, 2, 4, 8};

    core_val[0] = get_core(0) + get_core(1) + get_core(2) + get_core(3);
    v = *((uint32_t *)&pmem[0x00]);
    m = (v & 3) + 1;
    k = ((v >> 4) & 3) + 1;
    n = ((v >> 8) & 0x1f) + 1;
    p = p_idx[(v >> 16) & 3];
    cpu_val[0] = find_best_match_cpu_clock(24 * n * k);
    // printf("CPU %dMHz,%dMHz (0x%08x,0x%08x, n:%d, k:%d, m:%d, p:%d)\n",
    //     (24 * n * k) / (m * p), cpu_clock[cpu_val[0]].clk,
    //     v, cpu_clock[cpu_val[0]].reg,
    //     n, k, m, p);

    v = *((uint32_t *)&pmem[0x38]);
    m = (v & 0xf) + 1;
    n = ((v >> 8) & 0x7f) + 1;
    gpu_val[0] = find_best_match_gpu_clock((24 * n) / m);
    // printf("GPU %dMHz,%dMHz (0x%08x,0x%08x, n:%d, m:%d)\n",
    //     (24 * n) / m, gpu_clock[gpu_val[0]].clk,
    //     v, gpu_clock[gpu_val[0]].reg,
    //     n, m);

    v = *((uint32_t *)&pmem[0xf8]);
    if ((v & (1 << 16)) == 0) {
        v = *((uint32_t *)&pmem[0x20]);
        m = (v & 3) + 1;
        k = ((v >> 4) & 3) + 1;
        n = ((v >> 8) & 0x1f) + 1;
        ram_val[0] = (n * k) / m;
        // printf("DDR0 %dMHz (0x%08x)\n", 24 * ram_val[0], v);
    }
    else {
        v = *((uint32_t *)&pmem[0x4c]);
        n = (v >> 8) & 0x3f;
        ram_val[0] = n;
        // printf("DDR1 %dMHz (0x%08x)\n", 24 * ram_val[0], v);
    }

    FILE *fd = NULL;
    char buf[255] = {0};

    fd = popen("cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "r");
    if (fd ) {
        int len = 0;
        int cc = 0;

        fgets(buf, sizeof(buf), fd);
        len = strlen(buf);
        for (cc = 0; cc < len; cc++) {
            if (buf[cc] == '\r') {
                buf[cc] = 0;
            }
            if (buf[cc] == '\n') {
                buf[cc] = 0;
            }
        }
        // printf("Governor: %s\n", buf);
        for (cc = 0; cc < 5; cc++) {
            if (!strcmp(buf, sch_name[cc])) {
                sch_val[0] = cc;
                // printf("Find Governor Index: %d\n", cc);
                break;
            }
        }
        pclose(fd);
    }

    memset(buf, 0, sizeof(buf));
    fd = popen("swapon -s | grep swap", "r");
    if (fd ) {
        fgets(buf, sizeof(buf), fd);
        if (strlen(buf) > 10) {
            swap_val[0] = 1;
        }
        pclose(fd);
    }
    // printf("SWAP %dMB\n", swap_val[0] ? 1024 : 0);

    sch_val[1] = sch_val[0];
    core_val[1] = core_val[0];
    cpu_val[1] = cpu_val[0];
    gpu_val[1] = gpu_val[0];
    ram_val[1] = ram_val[0];
    swap_val[1] = swap_val[0];
}

static void check_before_set(int num, int v)
{
    char buf[255] = {0};

    if (get_core(num) != v) {
        sprintf(buf, "echo %d > /sys/devices/system/cpu/cpu%d/online", v ? 1 : 0, num);
        system(buf);
    }
}

static void set_core(int n)
{
    if (n <= 1) {
        // printf("New CPU Core: 1\n");
        check_before_set(0, 1);
        check_before_set(1, 0);
        check_before_set(2, 0);
        check_before_set(3, 0);
    }
    else if (n == 2) {
        // printf("New CPU Core: 2\n");
        check_before_set(0, 1);
        check_before_set(1, 1);
        check_before_set(2, 0);
        check_before_set(3, 0);
    }
    else if (n == 3) {
        // printf("New CPU Core: 3\n");
        check_before_set(0, 1);
        check_before_set(1, 1);
        check_before_set(2, 1);
        check_before_set(3, 0);
    }
    else {
        // printf("New CPU Core: 4\n");
        check_before_set(0, 1);
        check_before_set(1, 1);
        check_before_set(2, 1);
        check_before_set(3, 1);
    }
}

static void set_swap(int v)
{
	return; // disabled, no swap on NextUI
	
    #define SWAP "/mnt/SDCARD/App/utils/swap"

    int org = 0;
    FILE *fd = NULL;
    char buf[255] = {0};

    fd = popen("swapon -s | grep swap", "r");
    if (fd ) {
        fgets(buf, sizeof(buf), fd);
        if (strlen(buf) > 10) {
            org = 1;
        }
        pclose(fd);
    }

    if (org != v) {
        if (v) {
            system("swapon " SWAP);
        }
        else {
            system("swapoff " SWAP);
        }
    }
}

static void set_ram(uint32_t v)
{
    uint32_t *p = (uint32_t *)&pmem[0x4c];

    v &= 0x3f;
    *p = (1 << 31) | (v << 8);
    // printf("New DDR1 Clock is %dMHz\n", (24 * v));
}

static void set_cpu(uint32_t v)
{
    uint32_t *p = (uint32_t *)&pmem[0x00];

    *p = cpu_clock[v].reg;
    // printf("New CPU Clock is %dMHz (0x%08x)\n", cpu_clock[v].clk, cpu_clock[v].reg);
}

static void set_gpu(uint32_t v)
{
    uint32_t *p = (uint32_t *)&pmem[0x38];

    *p = gpu_clock[v].reg;
    // printf("New GPU Clock is %dMHz (0x%08x)\n", gpu_clock[v].clk, gpu_clock[v].reg);
}

static void set_sch(uint32_t v)
{
    char buf[255] = {0};

    sprintf(buf, "echo %s > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", sch_name[v % 5]);
    system(buf);
    // printf("New Schedule: %s\n", sch_name[v % 5]);
}

static void set_sch_by_name(const char *s)
{
    char buf[255] = {0};

    sprintf(buf, "echo %s > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", s);
    system(buf);
    // printf("New Schedule: %s\n", s);
}

int main(int argc, char **argv) {
    mem_fd = open("/dev/mem", O_RDWR);
    if (mem_fd < 0) {
        printf("Failed to open /dev/mem\n");
        return -1;
    }
    pmem = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, CCU_BASE);
    if (pmem == MAP_FAILED) {
        printf("Failed to map memory\n");
        return -1;
    }
    // printf("pmem %p\n", pmem);
    read_value();

    if (argc == 7) {
        // printf("User Prefers: %s, %s, %d, %d, %d, %d\n\n", argv[1], argv[2], atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]));
        set_sch_by_name(argv[1]);
        set_core(atoi(argv[2]));
        cpu_val[0] = find_best_match_cpu_clock(atoi(argv[3]));
        gpu_val[0] = find_best_match_gpu_clock(atoi(argv[4]));
        set_cpu(cpu_val[0]);
        set_gpu(gpu_val[0]);
        set_ram(atoi(argv[5]) / 24);
        set_swap(atoi(argv[6]));
    }
	else {
	    printf("Usage: \n");
	    printf("\t%s governor core cpu-freq gpu-freq ddr-freq swap\n\n", argv[0]);
	    printf("\tgovernor: interactive, conservative, userspace, ondemand, performance\n");
	    printf("\tcore: 1~4\n");
	    printf("\tcpu-freq: MHz\n");
	    printf("\tgpu-freq: MHz\n");
	    printf("\tddr-freq: MHz\n");
	    printf("\tswap: on | off (ignored)\n\n");
	    printf("Examples:\n");
	    printf("\t%s userspace 1 648 384 1080 0\n", argv[0]);
	    printf("\t%s performance 4 1512 384 1080 1\n\n", argv[0]);
	}

    return 0;
}
