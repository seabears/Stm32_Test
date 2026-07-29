#include "card_animation.h"

#include "ssd1306.h"

#include <stdint.h>

#define CARD_TOP          3U
#define CARD_HEIGHT      58U
#define CARD_FRAME_COUNT 11U

typedef struct
{
  char rank;
  const uint8_t *suit;
} PlayingCard;

static const uint8_t suit_heart[7] =
{
  0x36U, 0x7FU, 0x7FU, 0x7FU, 0x3EU, 0x1CU, 0x08U
};

static const uint8_t suit_spade[7] =
{
  0x08U, 0x1CU, 0x3EU, 0x7FU, 0x7FU, 0x1CU, 0x3EU
};

static const uint8_t suit_diamond[7] =
{
  0x08U, 0x1CU, 0x3EU, 0x7FU, 0x3EU, 0x1CU, 0x08U
};

static const uint8_t suit_club[7] =
{
  0x1CU, 0x1CU, 0x6BU, 0x7FU, 0x6BU, 0x1CU, 0x3EU
};

static const PlayingCard cards[] =
{
  {'A', suit_heart},
  {'K', suit_spade},
  {'Q', suit_diamond},
  {'J', suit_club}
};

static const uint8_t frame_width[CARD_FRAME_COUNT] =
{
  46U, 38U, 30U, 22U, 14U, 6U, 14U, 22U, 30U, 38U, 46U
};

static uint8_t card_index;
static uint8_t frame_index;

static void DrawHorizontalLine(uint8_t x, uint8_t y, uint8_t length)
{
  uint8_t offset;

  for (offset = 0U; offset < length; ++offset)
  {
    OLED_DrawPixel((uint8_t)(x + offset), y, true);
  }
}

static void DrawVerticalLine(uint8_t x, uint8_t y, uint8_t length)
{
  uint8_t offset;

  for (offset = 0U; offset < length; ++offset)
  {
    OLED_DrawPixel(x, (uint8_t)(y + offset), true);
  }
}

static void DrawCardOutline(uint8_t left, uint8_t width)
{
  uint8_t right = (uint8_t)(left + width - 1U);
  uint8_t bottom = (uint8_t)(CARD_TOP + CARD_HEIGHT - 1U);

  if (width <= 6U)
  {
    DrawVerticalLine((uint8_t)(left + (width / 2U)),
                     (uint8_t)(CARD_TOP + 1U),
                     (uint8_t)(CARD_HEIGHT - 2U));
    return;
  }

  DrawHorizontalLine((uint8_t)(left + 2U),
                     CARD_TOP,
                     (uint8_t)(width - 4U));
  DrawHorizontalLine((uint8_t)(left + 2U),
                     bottom,
                     (uint8_t)(width - 4U));
  DrawVerticalLine(left,
                   (uint8_t)(CARD_TOP + 2U),
                   (uint8_t)(CARD_HEIGHT - 4U));
  DrawVerticalLine(right,
                   (uint8_t)(CARD_TOP + 2U),
                   (uint8_t)(CARD_HEIGHT - 4U));

  OLED_DrawPixel((uint8_t)(left + 1U), (uint8_t)(CARD_TOP + 1U), true);
  OLED_DrawPixel((uint8_t)(right - 1U), (uint8_t)(CARD_TOP + 1U), true);
  OLED_DrawPixel((uint8_t)(left + 1U), (uint8_t)(bottom - 1U), true);
  OLED_DrawPixel((uint8_t)(right - 1U), (uint8_t)(bottom - 1U), true);
}

static void DrawSuit(const uint8_t *bitmap,
                     uint8_t center_x,
                     uint8_t top,
                     uint8_t scale)
{
  uint8_t row;
  uint8_t column;
  uint8_t scale_x;
  uint8_t scale_y;
  uint8_t left = (uint8_t)(center_x - ((7U * scale) / 2U));

  for (row = 0U; row < 7U; ++row)
  {
    for (column = 0U; column < 7U; ++column)
    {
      if ((bitmap[row] & (1U << (6U - column))) == 0U)
      {
        continue;
      }

      for (scale_y = 0U; scale_y < scale; ++scale_y)
      {
        for (scale_x = 0U; scale_x < scale; ++scale_x)
        {
          OLED_DrawPixel((uint8_t)(left + (column * scale) + scale_x),
                         (uint8_t)(top + (row * scale) + scale_y),
                         true);
        }
      }
    }
  }
}

static void DrawFrame(void)
{
  uint8_t width = frame_width[frame_index];
  uint8_t left = (uint8_t)((OLED_WIDTH - width) / 2U);
  uint8_t visible_card = card_index;

  if (frame_index > (CARD_FRAME_COUNT / 2U))
  {
    visible_card = (uint8_t)((card_index + 1U) %
                             (sizeof(cards) / sizeof(cards[0])));
  }

  OLED_Clear();
  DrawCardOutline(left, width);

  if (width >= 22U)
  {
    OLED_SetCursor((uint8_t)(left + 4U), (uint8_t)(CARD_TOP + 4U));
    OLED_WriteChar(cards[visible_card].rank);
  }

  if (width >= 30U)
  {
    DrawSuit(cards[visible_card].suit,
             (uint8_t)(OLED_WIDTH / 2U),
             24U,
             2U);
  }
}

bool CardAnimation_Init(void)
{
  card_index = 0U;
  frame_index = 0U;
  DrawFrame();
  return OLED_Update();
}

bool CardAnimation_Update(void)
{
  ++frame_index;

  if (frame_index >= CARD_FRAME_COUNT)
  {
    frame_index = 0U;
    card_index = (uint8_t)((card_index + 1U) %
                           (sizeof(cards) / sizeof(cards[0])));
  }

  DrawFrame();
  return OLED_Update();
}
