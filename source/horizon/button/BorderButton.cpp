#include "Aether/horizon/button/BorderButton.hpp"

// Corner radius of rectangle
#define CORNER_RAD 8
// Horizontal padding for text
#define PADDING 20

namespace Aether {
    BorderButton::BorderButton(int x, int y, int w, int h, unsigned int b, std::string t, unsigned int s, std::function<void()> f) : Element(x, y, w, h) {
        this->box = new Box(this->x(), this->y(), this->w(), this->h(), b, CORNER_RAD);
        this->text = new Text(this->x() + this->w()/2, this->y() + this->h()/2, t, s);
        this->text->setXY(this->text->x() - this->text->w()/2, this->text->y() - this->text->h()/2);
        this->text->setScroll(true);
        this->text->setScrollSpeed(40);
        this->text->setScrollWaitTime(1500);
        this->addElement(this->box);
        this->addElement(this->text);
        this->setCallback(f);
    }

    Colour BorderButton::getBorderColour() {
        return this->box->getColour();
    }

    void BorderButton::setBorderColour(Colour c) {
        this->box->setColour(c);
    }

    Colour BorderButton::getTextColour() {
        return this->text->getColour();
    }

    void BorderButton::setTextColour(Colour c) {
        this->text->setColour(c);
    }

    std::string BorderButton::getString() {
        return this->text->string();
    }

    void BorderButton::setString(std::string s) {
        this->text->setString(s);
        if (this->text->texW() > this->w() - 1.5*PADDING) {
            this->text->setW(this->w() - 2*PADDING);
        }
        this->text->setXY(this->x() + (this->w() - this->text->w())/2, this->y() + (this->h() - this->text->h())/2);
    }

    void BorderButton::setW(int w) {
        Element::setW(w);
        this->box->setBoxSize(this->w(), this->h());
        this->setString(this->text->string());
    }

    void BorderButton::setH(int h) {
        Element::setH(h);
        this->box->setBoxSize(this->w(), this->h());
        this->text->setY(this->y() + (this->h() - this->text->h())/2);
    }

    SDL_Texture * BorderButton::renderHighlightBG() {
        return SDLHelper::renderFilledRoundRect(this->w(), this->h(), CORNER_RAD + 2);
    }

    SDL_Texture * BorderButton::renderHighlight() {
        return SDLHelper::renderRoundRect(this->w() + 2*(this->hiSize - this->box->border()), this->h() + 2*(this->hiSize - this->box->border()), CORNER_RAD + 2, this->hiSize);
    }

    SDL_Texture * BorderButton::renderSelection() {
        return SDLHelper::renderFilledRoundRect(this->w(), this->h(), CORNER_RAD + 2);
    }
};