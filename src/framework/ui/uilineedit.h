#ifndef UILINEEDIT_H
#define UILINEEDIT_H

#include "uiwidget.h"

class UILineEdit : public UIWidget
{
public:
    UILineEdit();

    static UILineEditPtr create();

    virtual void loadStyleFromOTML(const OTMLNodePtr& styleNode);
    virtual void render();

    void update();

    void setText(const std::string& text);
    void setAlign(AlignmentFlag align);
    void setCursorPos(int pos);
    void enableCursor(bool enable = true);

    void moveCursor(bool right);
    void appendCharacter(char c);
    void removeCharacter(bool right);

    void setFont(const FontPtr& font);
    std::string getText() const { return m_text; }
    int getTextPos(Point pos);

protected:
    virtual void onGeometryUpdate(UIGeometryUpdateEvent& event);
    virtual void onFocusChange(UIFocusEvent& event);
    virtual void onKeyPress(UIKeyEvent& event);
    virtual void onMousePress(UIMouseEvent& event);

private:
    void blinkCursor();

    std::string m_text;
    Rect m_drawArea;
    AlignmentFlag m_align;
    int m_cursorPos;
    Point m_startInternalPos;
    int m_startRenderPos;
    int m_cursorTicks;
    int m_textHorizontalMargin;

    std::vector<Rect> m_glyphsCoords;
    std::vector<Rect> m_glyphsTexCoords;
};

#endif