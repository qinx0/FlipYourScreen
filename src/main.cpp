#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PauseLayer.hpp>

using namespace geode::prelude;

static bool  s_modEnabled  = true;
static bool  s_flipUI      = false;
static float s_targetAngle = 0.f;
static bool  s_initDone    = false;

static void rotateTo(CCNode* node, float angle, float speed) {
    if (!node) return;
    node->stopAllActions();
    if (speed <= 0.f) {
        node->setRotation(angle);
        return;
    }
    node->runAction(CCEaseInOut::create(CCRotateTo::create(speed, angle), 2.0f));
}

static bool isHUDNode(CCNode* n) {
    if (!n) return false;
    auto id = n->getID();
    return id == "progress-bar"
        || id == "percentage-label"
        || id == "UILayer"
        || id == "debug-text";
}

static void syncHUDNode(CCNode* n, float angleDeg, CCPoint origPos) {
    if (!n) return;
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    float cx = winSize.width  * 0.5f;
    float cy = winSize.height * 0.5f;
    // Translate to screen-centre origin, inverse-rotate, translate back
    float px  = origPos.x;
    float py  = origPos.y;
    float rad  = angleDeg * (float)M_PI / 180.f;
    float cosA = cosf(rad);
    float sinA = sinf(rad);
    n->setPosition(CCPoint(
         px * cosA + py * sinA + cx,
        -px * sinA + py * cosA + cy
    ));
    n->setRotation(-angleDeg);
}

class $modify(MyPlayerObject, PlayerObject) {
    void flipGravity(bool isFlipped, bool noEffects) {
        PlayerObject::flipGravity(isFlipped, noEffects);
        if (!s_modEnabled || !s_initDone) return;
        if (m_isSecondPlayer) return;
        auto* pl = PlayLayer::get();
        if (!pl) return;

        float speed   = static_cast<float>(Mod::get()->getSettingValue<double>("rotation-speed"));
        s_targetAngle = isFlipped ? 180.f : 0.f;
        rotateTo(pl, s_targetAngle, speed);
    }
};

struct MyPlayLayer : geode::Modify<MyPlayLayer, PlayLayer> {
    struct Fields {
        std::unordered_map<CCNode*, CCPoint> hudOrigPos;
        bool positionsCaptured = false;
    };

    // Called every frame. On the very first frame, captures HUD positions —
    // by then GD and all mods have finished their init positioning.
    void captureHUDPositions() {
        if (m_fields->positionsCaptured) return;
        m_fields->positionsCaptured = true;

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto* children = this->getChildren();
        for (unsigned int i = 0; i < children->count(); i++) {
            auto* child = static_cast<CCNode*>(children->objectAtIndex(i));
            if (isHUDNode(child)) {
                // Convert from bottom-left origin to screen-centre origin
                // (PlayLayer anchor is now 0.5,0.5 at winSize/2)
                CCPoint pos = child->getPosition();
                pos.x -= winSize.width  * 0.5f;
                pos.y -= winSize.height * 0.5f;
                m_fields->hudOrigPos[child] = pos;
                log::info("HUD {} captured at screen-centre pos: {},{}", child->getID(), pos.x, pos.y);
            }
        }
    }

    void syncHUD(float) {
        captureHUDPositions();
        if (!s_modEnabled || s_flipUI) return;
        float angle = this->getRotation();
        for (auto& [node, origPos] : m_fields->hudOrigPos)
            syncHUDNode(node, angle, origPos);
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        s_initDone = false;

        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        s_modEnabled  = Mod::get()->getSettingValue<bool>("enabled");
        s_flipUI      = Mod::get()->getSettingValue<bool>("rotate-ui");
        s_targetAngle = 0.f;

        if (s_modEnabled) {
            auto winSize = CCDirector::sharedDirector()->getWinSize();
            this->ignoreAnchorPointForPosition(false);
            this->setAnchorPoint({0.5f, 0.5f});
            this->setPosition(winSize / 2);
            this->schedule(schedule_selector(MyPlayLayer::syncHUD));
        }

        s_initDone = true;
        return true;
    }

    void resetLevel() {
        s_initDone = false;
        this->stopAllActions();
        this->setRotation(0.f);
        s_targetAngle = 0.f;
        // Reset HUD nodes to original positions before resetLevel repositions them
        for (auto& [node, origPos] : m_fields->hudOrigPos) {
            node->setPosition(origPos);
            node->setRotation(0.f);
        }
        PlayLayer::resetLevel();
        s_initDone = true;
    }

    void onQuit() {
        s_initDone = false;
        s_targetAngle = 0.f;
        this->stopAllActions();
        this->setRotation(0.f);
        this->ignoreAnchorPointForPosition(true);
        this->setAnchorPoint({0.f, 0.f});
        this->setPosition({0.f, 0.f});
        PlayLayer::onQuit();
    }
};

class $modify(MyPauseLayer, PauseLayer) {

    void onToggleMod(CCObject* sender) {
        s_modEnabled = !s_modEnabled;
        Mod::get()->setSettingValue("enabled", s_modEnabled);

        auto* spr = static_cast<ButtonSprite*>(
            static_cast<CCMenuItemSpriteExtra*>(sender)->getNormalImage());
        spr->setString(s_modEnabled ? "Flip ON" : "Flip OFF");

        if (!s_modEnabled) {
            auto* pl = PlayLayer::get();
            if (!pl) return;
            float speed = static_cast<float>(Mod::get()->getSettingValue<double>("rotation-speed"));
            rotateTo(pl, 0.f, speed);
            s_targetAngle = 0.f;
        }
    }

    void onToggleUI(CCObject* sender) {
        s_flipUI = !s_flipUI;
        Mod::get()->setSettingValue("rotate-ui", s_flipUI);

        auto* spr = static_cast<ButtonSprite*>(
            static_cast<CCMenuItemSpriteExtra*>(sender)->getNormalImage());
        spr->setString(s_flipUI ? "UI Flip ON" : "UI Flip OFF");

        // When turning UI flip ON, reset HUD nodes back to their original positions
        auto* pl = static_cast<MyPlayLayer*>(PlayLayer::get());
        if (!pl) return;
        if (s_flipUI) {
            for (auto& [node, origPos] : pl->m_fields->hudOrigPos) {
                node->setPosition(origPos);
                node->setRotation(0.f);
            }
        }
    }

    void customSetup() {
        PauseLayer::customSetup();

        auto* flipSpr = ButtonSprite::create(
            s_modEnabled ? "Flip ON" : "Flip OFF",
            "goldFont.fnt", "GJ_button_01.png", 0.6f);
        auto* flipBtn = CCMenuItemSpriteExtra::create(
            flipSpr, this, menu_selector(MyPauseLayer::onToggleMod));

        auto* uiSpr = ButtonSprite::create(
            s_flipUI ? "UI Flip ON" : "UI Flip OFF",
            "goldFont.fnt", "GJ_button_02.png", 0.6f);
        auto* uiBtn = CCMenuItemSpriteExtra::create(
            uiSpr, this, menu_selector(MyPauseLayer::onToggleUI));

        auto* menu = CCMenu::create();
        menu->addChild(flipBtn);
        menu->addChild(uiBtn);
        menu->alignItemsVerticallyWithPadding(4.f);
        menu->setPosition({60.f, 40.f});
        menu->setID("flip-your-screen-menu"_spr);
        this->addChild(menu, 10);
    }
};
