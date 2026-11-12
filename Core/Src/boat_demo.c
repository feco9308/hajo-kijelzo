#include "boat_demo.h"
#include "sharp_ls044q7dh01.h"
#include <stdbool.h>
#include <string.h>
#include <math.h>

/* 9600-byte monochrome framebuffer. Same LSB pixel order as the LCD driver. */
static uint8_t boat_pixels[SHARP_HEIGHT][SHARP_LINE_BYTES];
static BoatDemoData applied_data = {35, 142, 101, 56, 247, 2, 24, 1};

BoatDemoData boat_demo_get_data(void)
{
    return applied_data;
}

bool boat_demo_data_valid(const BoatDemoData *data)
{
    return data != NULL && data->awa_degrees >= -180 && data->awa_degrees <= 180 &&
           data->aws_tenths <= 999U && data->tws_tenths <= 999U &&
           data->sog_tenths <= 999U && data->cog_degrees <= 359U &&
           data->xte_hundredths >= -999 && data->xte_hundredths <= 999 &&
           data->distance_tenths <= 999U && data->waypoint >= 1U && data->waypoint <= 99U;
}

typedef struct {
    char character;
    uint8_t columns[5];
} Glyph;

/* Compact 5x7 uppercase and numeric pixel glyphs. Bit0 is the top row. */
static const Glyph font[] = {
    {' ', {0,0,0,0,0}},
    {'+', {0x08,0x08,0x3E,0x08,0x08}},
    {'-', {0x08,0x08,0x08,0x08,0x08}},
    {'.', {0,0x60,0x60,0,0}},
    {'0', {0x3E,0x51,0x49,0x45,0x3E}},
    {'1', {0,0x42,0x7F,0x40,0}},
    {'2', {0x42,0x61,0x51,0x49,0x46}},
    {'3', {0x21,0x41,0x45,0x4B,0x31}},
    {'4', {0x18,0x14,0x12,0x7F,0x10}},
    {'5', {0x27,0x45,0x45,0x45,0x39}},
    {'6', {0x3C,0x4A,0x49,0x49,0x30}},
    {'7', {0x01,0x71,0x09,0x05,0x03}},
    {'8', {0x36,0x49,0x49,0x49,0x36}},
    {'9', {0x06,0x49,0x49,0x29,0x1E}},
    {'A', {0x7E,0x11,0x11,0x11,0x7E}},
    {'B', {0x7F,0x49,0x49,0x49,0x36}},
    {'C', {0x3E,0x41,0x41,0x41,0x22}},
    {'D', {0x7F,0x41,0x41,0x22,0x1C}},
    {'E', {0x7F,0x49,0x49,0x49,0x41}},
    {'F', {0x7F,0x09,0x09,0x09,0x01}},
    {'G', {0x3E,0x41,0x49,0x49,0x7A}},
    {'H', {0x7F,0x08,0x08,0x08,0x7F}},
    {'I', {0,0x41,0x7F,0x41,0}},
    {'J', {0x20,0x40,0x41,0x3F,0x01}},
    {'K', {0x7F,0x08,0x14,0x22,0x41}},
    {'L', {0x7F,0x40,0x40,0x40,0x40}},
    {'M', {0x7F,0x02,0x0C,0x02,0x7F}},
    {'N', {0x7F,0x04,0x08,0x10,0x7F}},
    {'O', {0x3E,0x41,0x41,0x41,0x3E}},
    {'P', {0x7F,0x09,0x09,0x09,0x06}},
    {'Q', {0x3E,0x41,0x51,0x21,0x5E}},
    {'R', {0x7F,0x09,0x19,0x29,0x46}},
    {'S', {0x46,0x49,0x49,0x49,0x31}},
    {'T', {0x01,0x01,0x7F,0x01,0x01}},
    {'U', {0x3F,0x40,0x40,0x40,0x3F}},
    {'V', {0x1F,0x20,0x40,0x20,0x1F}},
    {'W', {0x3F,0x40,0x38,0x40,0x3F}},
    {'X', {0x63,0x14,0x08,0x14,0x63}},
    {'Y', {0x07,0x08,0x70,0x08,0x07}},
    {'Z', {0x61,0x51,0x49,0x45,0x43}}
};

static void pixel(int x, int y)
{
    if (x < 0 || y < 0 || x >= (int)SHARP_WIDTH || y >= (int)SHARP_HEIGHT) return;
    boat_pixels[y][x / 8] &= (uint8_t)~(1U << (x % 8));
}

static void line(int x0, int y0, int x1, int y1)
{
    int dx = x1 >= x0 ? x1 - x0 : x0 - x1;
    int dy = y1 >= y0 ? y0 - y1 : y1 - y0;
    int step_x = x0 < x1 ? 1 : -1;
    int step_y = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        pixel(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int doubled_error = 2 * error;
        if (doubled_error >= dy) { error += dy; x0 += step_x; }
        if (doubled_error <= dx) { error += dx; y0 += step_y; }
    }
}

static void text(int x, int y, const char *value, unsigned scale)
{
    while (*value) {
        const uint8_t *glyph = font[0].columns;
        for (unsigned i = 0; i < sizeof font / sizeof font[0]; ++i) {
            if (font[i].character == *value) { glyph = font[i].columns; break; }
        }
        for (unsigned column = 0; column < 5U; ++column) {
            for (unsigned row = 0; row < 7U; ++row) {
                if (!(glyph[column] & (1U << row))) continue;
                for (unsigned sy = 0; sy < scale; ++sy) {
                    for (unsigned sx = 0; sx < scale; ++sx)
                        pixel(x + (int)(column * scale + sx), y + (int)(row * scale + sy));
                }
            }
        }
        x += (int)(6U * scale);
        ++value;
    }
}

static void circle(int cx, int cy, int radius)
{
    int x = radius;
    int y = 0;
    int error = 1 - radius;
    while (x >= y) {
        pixel(cx+x,cy+y); pixel(cx+y,cy+x);
        pixel(cx-y,cy+x); pixel(cx-x,cy+y);
        pixel(cx-x,cy-y); pixel(cx-y,cy-x);
        pixel(cx+y,cy-x); pixel(cx+x,cy-y);
        ++y;
        if (error < 0) error += 2*y + 1;
        else { --x; error += 2*(y-x) + 1; }
    }
}

static char *format_unsigned(char *output, uint32_t value)
{
    char reversed[10];
    unsigned count = 0;
    do {
        reversed[count++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);
    while (count != 0U) *output++ = reversed[--count];
    *output = '\0';
    return output;
}

static void format_fixed(char *output, int32_t value, unsigned decimals)
{
    uint32_t magnitude = value < 0 ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
    if (value < 0) *output++ = '-';
    uint32_t divisor = decimals == 2U ? 100U : 10U;
    output = format_unsigned(output, magnitude / divisor);
    *output++ = '.';
    if (decimals == 2U) *output++ = (char)('0' + (magnitude % 100U) / 10U);
    *output++ = (char)('0' + magnitude % 10U);
    *output = '\0';
}

static void wind_dial(int32_t angle)
{
    static const int8_t outer_x[12] = {0,27,47,54,47,27,0,-27,-47,-54,-47,-27};
    static const int8_t outer_y[12] = {-54,-47,-27,0,27,47,54,47,27,0,-27,-47};
    static const int8_t inner_x[12] = {0,24,42,49,42,24,0,-24,-42,-49,-42,-24};
    static const int8_t inner_y[12] = {-49,-42,-24,0,24,42,49,42,24,0,-24,-42};
    circle(88, 98, 54);
    for (unsigned i = 0; i < 12U; ++i)
        line(88+inner_x[i],98+inner_y[i],88+outer_x[i],98+outer_y[i]);
    text(9, 33, "AWA", 1);
    text(86, 33, "0", 1);
    line(88,82,78,113); line(78,113,98,113); line(98,113,88,82);
    line(88,87,88,108);

    /* Positive AWA is starboard. Arrow points inward from the wind source. */
    float radians = (float)angle * (3.14159265358979323846f / 180.0f);
    float sine = sinf(radians);
    float cosine = cosf(radians);
    int from_x = 88 + (int)(52.0f * sine);
    int from_y = 98 - (int)(52.0f * cosine);
    int tip_x = 88 + (int)(21.0f * sine);
    int tip_y = 98 - (int)(21.0f * cosine);
    line(from_x, from_y, tip_x, tip_y);
    line(tip_x, tip_y, tip_x+(int)(10.0f*sine+5.0f*cosine),
         tip_y+(int)(-10.0f*cosine+5.0f*sine));
    line(tip_x, tip_y, tip_x+(int)(10.0f*sine-5.0f*cosine),
         tip_y+(int)(-10.0f*cosine-5.0f*sine));

    char value[12];
    char *digits = value;
    if (angle < 0) *digits++ = '-';
    else *digits++ = '+';
    format_unsigned(digits, (uint32_t)(angle < 0 ? -angle : angle));
    int width = (int)strlen(value) * 18 - 3;
    text(88-width/2, 160, value, 3);
    text(145, 174, "DEG", 1);
}

static HAL_StatusTypeDef render(const BoatDemoData *data)
{
    char value[16];
    memset(boat_pixels, 0xFF, sizeof boat_pixels);
    line(0,0,319,0); line(319,0,319,239);
    line(319,239,0,239); line(0,239,0,0);
    text(9,6,"VITORLAS",2);
    text(264,6,"DEMO",2);
    line(0,25,319,25);
    line(180,26,180,184);
    wind_dial(data->awa_degrees);

    text(192,33,"AWS",2);
    format_fixed(value, (int32_t)data->aws_tenths, 1);
    text(191,53,value,4);
    text(294,74,"KN",1);
    line(181,91,319,91);
    text(192,98,"TWS",2);
    format_fixed(value, (int32_t)data->tws_tenths, 1);
    text(193,119,value,3);
    text(287,132,"KN",1);
    line(181,149,319,149);
    char waypoint[] = "WP00";
    waypoint[2] = (char)('0' + data->waypoint / 10U);
    waypoint[3] = (char)('0' + data->waypoint % 10U);
    text(192,156,waypoint,1);
    text(192,171,"DST",1);
    format_fixed(value, (int32_t)data->distance_tenths, 1);
    text(216,171,value,1);
    text(252,171,"NM",1);

    line(0,185,319,185);
    line(106,185,106,239); line(213,185,213,239);
    text(9,193,"SOG",1);
    format_fixed(value, (int32_t)data->sog_tenths, 1);
    text(9,207,value,3); text(80,222,"KN",1);
    text(115,193,"COG",1);
    format_unsigned(value, data->cog_degrees);
    text(115,207,value,3); text(185,222,"DEG",1);
    text(222,193,"XTE",1);
    format_fixed(value, data->xte_hundredths, 2);
    text(221,207,value,strlen(value) > 4U ? 2U : 3U);
    text(299,222,"NM",1);

    for (uint16_t y = 0; y < SHARP_HEIGHT; ++y) {
        HAL_StatusTypeDef s = sharp_write_line(y + 1U, boat_pixels[y]);
        if (s != HAL_OK) return s;
    }
    return HAL_OK;
}

HAL_StatusTypeDef boat_demo_show(void)
{
    return render(&applied_data);
}

HAL_StatusTypeDef boat_demo_set_data(const BoatDemoData *data)
{
    if (!boat_demo_data_valid(data)) return HAL_ERROR;
    HAL_StatusTypeDef status = render(data);
    if (status == HAL_OK) applied_data = *data;
    return status;
}
