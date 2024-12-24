class OffsetCustomItem : public AbstractMenu::CustomItem
{
    public:
        float *offset_val_ = nullptr;
        int adc_idx_ = 0;
        char name_[20];
        void Init(float *offset_val, int adc_idx, const char *name)
        {
            offset_val_ = offset_val;
            adc_idx_ = adc_idx;
            strcpy(name_, name);
        }

        void Draw(OneBitGraphicsDisplay& display,
                  int                    currentIndex,
                  int                    numItemsTotal,
                  Rectangle              boundsToDrawIn,
                  bool                   isEditing) override
        {
            auto       remainingBounds = display.GetBounds();
            const auto topRowHeight    = GetTopRowHeight(remainingBounds.GetHeight());
            const auto topRowRect      = remainingBounds.RemoveFromTop(topRowHeight);

            char sbuf[20];
            float fval = hw.GetAdcValue(adc_idx_) + *offset_val_;
            sprintf(sbuf, "%0.2f V", fval*5.0f);

            DrawTopRow(display, currentIndex, numItemsTotal, name_, topRowRect, !isEditing);
            if (isEditing)
                remainingBounds = DrawLRArrowsAndGetRemRect(display, remainingBounds, true, true);

            const auto font = GetValueFont(sbuf, remainingBounds);
            display.WriteStringAligned(sbuf, font, remainingBounds, Alignment::centered, true);
        }

        bool canBeEnteredForEditing_ = true;
        bool CanBeEnteredForEditing() const override
        {
            return canBeEnteredForEditing_;
        }

        void     ModifyValue(int16_t  increments,
                             uint16_t stepsPerRevolution,
                             bool     isFunctionButtonPressed) override
        {
            float new_val = *offset_val_ + increments * 0.001f;
            if (new_val > 1.0f)
                new_val = 1.0f;
            else if (new_val < -1.0f)
                new_val = -1.0f;
            *offset_val_ = new_val;
        };

        void  ModifyValue(float valueSliderPosition0To1,
                          bool  isFunctionButtonPressed) override
        {
        };

        void OnOkayButton() override {}

        void DrawTopRow(OneBitGraphicsDisplay& display,
                        int                    currentIndex,
                        int                    numItemsTotal,
                        const char*            text,
                        Rectangle              rect,
                        bool                   isSelected) const
        {
            const bool hasPrev = currentIndex > 0;
            const bool hasNext = currentIndex < numItemsTotal - 1;
            // draw the arrows
            if(isSelected)
            {
                    rect = DrawLRArrowsAndGetRemRect(display, rect, hasPrev, hasNext);
            }
            const auto font = GetValueFont(text, rect);
            display.WriteStringAligned(text, font, rect, Alignment::centered, true);
        }

        Rectangle DrawLRArrowsAndGetRemRect(OneBitGraphicsDisplay& display,
                                            Rectangle              topRowRect,
                                            bool leftAvailable,
                                            bool rightAvailable) const
        {
            auto leftArrowRect = topRowRect.RemoveFromLeft(9)
                                     .WithSizeKeepingCenter(5, 9)
                                     .Translated(0, -1);
            auto rightArrowRect = topRowRect.RemoveFromRight(9)
                                      .WithSizeKeepingCenter(5, 9)
                                      .Translated(0, -1);

            if(leftAvailable)
            {
                for(int16_t x = leftArrowRect.GetRight() - 1;
                    x >= leftArrowRect.GetX();
                    x--)
                {
                    display.DrawLine(x,
                                     leftArrowRect.GetY(),
                                     x,
                                     leftArrowRect.GetBottom(),
                                     true);

                    leftArrowRect = leftArrowRect.Reduced(0, 1);
                    if(leftArrowRect.IsEmpty())
                        break;
                }
            }
            if(rightAvailable)
            {
                for(int16_t x = rightArrowRect.GetX();
                    x < rightArrowRect.GetRight();
                    x++)
                {
                    display.DrawLine(x,
                                     rightArrowRect.GetY(),
                                     x,
                                     rightArrowRect.GetBottom(),
                                     true);

                    rightArrowRect = rightArrowRect.Reduced(0, 1);
                    if(rightArrowRect.IsEmpty())
                        break;
                }
            }

            return topRowRect;
        }

        FontDef GetValueFont(const char*      textToDraw,
                             const Rectangle& availableSpace) const
        {
            (void)(textToDraw); // ignore unused variable warning
            if(availableSpace.GetHeight() < 10)
                return Font_6x8;
            else if(availableSpace.GetHeight() < 18)
                return Font_7x10;
            else
                return Font_11x18;
        }

        int GetTopRowHeight(int displayHeight) const
    {
        return displayHeight / 2;
    }
};
