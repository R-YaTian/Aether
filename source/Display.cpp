#include "Aether/Display.hpp"
#include "Aether/ThreadPool.hpp"
#include "Aether/utils/Utils.hpp"

// Delay (in ms) to pause after button held
#define HOLD_DELAY 400
// Delay (in ms) for moving between items
#define MOVE_DELAY 100

// Struct representing a "clock", which stores time between ticks
struct Clock {
    bool init = false;
    uint32_t last_tick = 0;
    uint32_t delta = 0;

    void tick() {
        uint32_t tick = SDL_GetTicks();
        // Avoid large dt on initial launch
        if (last_tick != 0) {
            delta = tick - last_tick;
        }
        last_tick = tick;
    }
};
static struct Clock dtClock;

namespace Aether {
    Display::Display(const std::string & name, const unsigned int width, const unsigned int height, const Renderer::LogHandler & log) : Element(0, 0, width, height) {
        this->setParent(nullptr);

        this->bgImg = nullptr;
        this->fadeAlpha = 0;
        this->fadeOut = false;
        this->fading = false;
        this->heldButton = Button::NO_BUTTON;
        this->heldTime = 0;
        this->holdDelay_ = MOVE_DELAY;

        this->hiAnim = [](uint32_t t){
            return Colour(255, 255, 255, 255);
        };

        this->screen = nullptr;

        // Initialize SDL (loop set to false if an error)
        this->renderer = new Renderer();
        this->renderer->setLogHandler(log);
        this->loop_ = renderer->initialize(name, width, height);
        this->fps_ = false;
    }

    void Display::setShowFPS(bool b) {
        this->fps_ = b;
    }

    void Display::setBackgroundColour(uint8_t r, uint8_t g, uint8_t b) {
        this->bg.setR(r);
        this->bg.setG(g);
        this->bg.setB(b);
        this->bg.setA(255);

        // Remove image from background
        delete this->bgImg;
        this->bgImg = nullptr;
    }

    bool Display::setBackgroundImage(std::string path) {
        // Double check the file exists
        if (!Utils::fileExists(path)) {
            return false;
        }

        // Create texture from image
        delete this->bgImg;
        this->bgImg = this->renderer->renderImageSurface(path);
        this->bgImg->convertToTexture();
        this->bg = Colour(0, 0, 0, 255);
        if (this->bgImg->type() == Drawable::Type::None) {
            return false;
        }
        return true;
    }

    void Display::setFont(std::string p) {
        this->renderer->setFont(p);
    }

    void Display::setFontSpacing(double h) {
        this->renderer->setFontSpacing(h);
    }

    void Display::addOverlay(Overlay * o) {
        o->reuse();
        this->overlays.push_back(o);
        this->screen->setInactive();
        if (this->overlays.size() > 1) {
            this->overlays[this->overlays.size() - 2]->setInactive();
        }
    }

    void Display::dropScreen() {
        if (this->screen != nullptr) {
            this->screen->onUnload();
        }
        this->screen = nullptr;
    }

    void Display::setScreen(Screen * s) {
        this->screenOps.push(std::make_pair(ScreenOp::Set, s));
    }

    void Display::pushScreen() {
        this->screenOps.push(std::make_pair(ScreenOp::Push, nullptr));
    }

    void Display::popScreen() {
        this->screenOps.push(std::make_pair(ScreenOp::Pop, nullptr));
    }

    void Display::setHighlightColours(Colour bg, Colour sel) {
        this->hiBGColour = bg;
        this->selColour = sel;
    }

    void Display::setHighlightAnimation(std::function<Colour(uint32_t)> f) {
        this->hiAnim = f;
    }

    int Display::holdDelay() {
        return this->holdDelay_;
    }

    void Display::setHoldDelay(int d) {
        this->holdDelay_ = d;
    }

    void Display::setFadeIn() {
        this->fadeAlpha = 255;
        this->fadeIn = true;
        this->fading = true;
    }

    void Display::setFadeOut() {
        this->fadeOut = true;
    }

    bool Display::loop() {
        // Avoid first large delta due to loading time
        if (!dtClock.init) {
            dtClock.tick();
            dtClock.init = true;
        }

        // Perform all requested screen operations
        while (!this->screenOps.empty()) {
            std::pair<ScreenOp, Screen *> item = this->screenOps.front();
            switch (item.first) {
                // Push screen on stack
                case ScreenOp::Push:
                    this->screenStack.push(this->screen);
                    this->screen = nullptr;
                    break;

                // Pop screen from stack
                case ScreenOp::Pop:
                    if (!this->screenStack.empty()) {
                        if (this->screen != nullptr) {
                            this->screen->onUnload();
                        }
                        this->screen = this->screenStack.top();
                        this->screenStack.pop();
                    }
                    break;

                // Set screen stored in queue
                case ScreenOp::Set:
                    if (this->screen != nullptr) {
                        this->screen->onUnload();
                    }
                    this->screen = item.second;
                    if (this->screen != nullptr) {
                        this->screen->onLoad();
                    }
                    break;
            }
            this->screenOps.pop();
        }

        // Quit loop if no screen is present
        if (this->screen == nullptr) {
            return false;
        }

        // Poll all events + pass
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_WINDOWEVENT:
                    if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
                        this->exit();
                    }
                    break;

                case SDL_JOYBUTTONUP:
                case SDL_JOYBUTTONDOWN:
                case SDL_FINGERDOWN:
                case SDL_FINGERMOTION:
                case SDL_FINGERUP:
                    // Create InputEvent and pass to screen/overlay
                    InputEvent * event = new InputEvent(e);

                    // Set touched variable
                    bool old = this->isTouch;
                    this->isTouch = (event->button() == Button::NO_BUTTON);

                    // Ignore first directional press or A (ie. only highlight)
                    if (old == true && this->isTouch == false && ((event->id() != FAKE_ID && (event->button() >= DPAD_LEFT && event->button() <= DPAD_DOWN)) || (event->button() == A))) {
                        delete event;
                        break;
                    }

                    if (event->id() == FAKE_ID && (event->button() < DPAD_LEFT || event->button() > DPAD_DOWN)) {
                        this->isTouch = true;
                    }

                    if (this->overlays.size() == 0) {
                        this->screen->handleEvent(event);
                    } else {
                        this->overlays[this->overlays.size()-1]->handleEvent(event);
                    }
                    delete event;
                    break;
            }

            // (Re)set held button
            if (e.type == SDL_JOYBUTTONUP && e.jbutton.which != FAKE_ID && e.jbutton.button == this->heldButton) {
                this->heldButton = Button::NO_BUTTON;
            } else if (e.type == SDL_JOYBUTTONDOWN && e.jbutton.which != FAKE_ID) {
                Button tmp = Utils::SDLtoButton(e.jbutton.button);
                // Only add held if a directional button
                if (tmp >= Button::DPAD_LEFT && tmp <= Button::RSTICK_DOWN) {
                    this->heldButton = Utils::SDLtoButton(e.jbutton.button);
                    this->heldTime = -HOLD_DELAY;
                }
            }
        }

        // Update screen
        dtClock.tick();
        this->screen->update(dtClock.delta);

        // Update overlays
        for (size_t i = 0; i < this->overlays.size(); i++) {
            if (this->overlays[i]->shouldClose()) {
                this->overlays.erase(this->overlays.begin() + i);
                i--;
                if (this->overlays.size() == 0) {
                    this->screen->setActive();
                } else {
                    this->overlays[this->overlays.size() - 1]->setActive();
                }
                continue;
            }

            this->overlays[i]->update(dtClock.delta);
        }

        // Push button pressed/released event if held
        if (this->heldButton != Button::NO_BUTTON) {
            this->heldTime += dtClock.delta;
            if (this->heldTime >= this->holdDelay_) {
                this->heldTime -= this->holdDelay_;
                // Send pushed event
                SDL_Event event;
                event.type = SDL_JOYBUTTONDOWN;
                event.jbutton.which = FAKE_ID;
                event.jbutton.button = (uint8_t)this->heldButton;
                event.jbutton.state = SDL_PRESSED;
                SDL_PushEvent(&event);
                // Send released event (so basically a verrry fast button press)
                SDL_Event event2;
                event2.type = SDL_JOYBUTTONUP;
                event2.jbutton.which = FAKE_ID;
                event2.jbutton.button = (uint8_t)this->heldButton;
                event2.jbutton.state = SDL_RELEASED;
                SDL_PushEvent(&event2);
            }
        }

        // Clear screen/draw background
        this->renderer->fillWindow(this->bg);
        if (this->bgImg) {
            this->bgImg->render(0, 0, this->renderer->windowWidth(), this->renderer->windowHeight());
        }

        // Update highlight border colour
        this->hiBorderColour = this->hiAnim(dtClock.last_tick);

        // Render screen
        this->screen->render();

        // Draw overlays
        for (size_t i = 0; i < this->overlays.size(); i++) {
            this->overlays[i]->render();
        }

        // Handle/render fade in/out
        if (this->fading) {
            if (this->fadeIn) {
                this->renderer->fillWindow(Colour(0, 0, 0, this->fadeAlpha));
                this->fadeAlpha -= 800*(dtClock.delta/1000.0);
                if (this->fadeAlpha < 0) {
                    this->fadeAlpha = 0;
                    this->fadeIn = false;
                    this->fading = false;
                }
            } else {
                if (this->fadeOut) {
                    this->renderer->fillWindow(Colour(0, 0, 0, this->fadeAlpha));
                    this->fadeAlpha += 800*(dtClock.delta/1000.0);
                    if (this->fadeAlpha > 255) {
                        this->fadeAlpha = 255;
                        this->loop_ = false;
                    }
                }
            }
        }

        // Draw FPS
        if (this->fps_) {
            std::string ss = "FPS: " + std::to_string((int)(1.0/(dtClock.delta/1000.0))) + " (" + std::to_string(dtClock.delta) + " ms) -- ~Mem: " + std::to_string(this->renderer->memoryUsage()/1024) + " kB -- T/S: " + std::to_string(this->renderer->textureCount()) + "/" + std::to_string(this->renderer->surfaceCount());
            Drawable * tt = this->renderer->renderTextSurface(ss, 20);
            tt->setColour(Colour(0, 150, 150, 255));
            tt->render(5, 695);
            delete tt;
        }

        this->renderer->present();

        // Update ThreadPool
        ThreadPool::process();

        return this->loop_;
    }

    void Display::exit() {
        if (this->fadeOut && !this->fading) {
            this->fadeAlpha = 0;
            this->fading = true;
        } else {
            this->loop_ = false;
        }
    }

    Display::~Display() {
        // As this is an element we can delete the cached highlight textures
        delete this->hiBGTex;
        delete this->hiBorderTex;
        delete this->selTex;
        this->hiBGTex = nullptr;
        this->hiBorderTex = nullptr;
        this->selTex = nullptr;

        // Clean up SDL
        ThreadPool::finalize();
        delete this->bgImg;
        this->renderer->cleanup();
        delete this->renderer;
    }
};