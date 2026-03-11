#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>
#include <unordered_map>

using namespace geode::prelude;

// ─────────────────────────────────────────────
//  State
// ─────────────────────────────────────────────
static bool  s_modEnabled  = true;
static bool  s_flipUI      = false;
static float s_targetAngle = 0.f; // current target rotation (0 or 180)

// ─────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────
CCNode* getGameWrapper(PlayLayer* pl) {
    return pl ? pl->getChildByID("gravity-game-wrapper"_spr) : nullptr;
}

void animateRotation(CCNode* node, float targetAngle, float speed) {
    if (!node) return;
    node->stopAllActions();
    node->runAction(
        CCEaseInOut::create(CCRotateTo::create(speed, targetAngle), 2.0f)
    );
}

// Stores original HUD positions so we can restore them on unflip.
// Key = node pointer, value = original position
static std::unordered_map<CCNode*, CCPoint> s_originalHUDPositions;

void storeOriginalHUDPos(CCNode* node) {
    if (!node) return;
    // Only store once (don't overwrite with flipped position)
    if (s_originalHUDPositions.find(node) == s_originalHUDPositions.end())
        s_originalHUDPositions[node] = node->getPosition();
}

void clearStoredHUDPositions() {
    s_originalHUDPositions.clear();
}

// Flip a HUD node: rotate 180° AND mirror its position across the screen centre.
// This makes top-left elements move to bottom-right, matching the rotated game view.
void setHUDNodeFlipped(CCNode* node, bool flipped) {
    if (!node) return;
    auto winSize = CCDirector::sharedDirector()->getWinSize();

    // Always store original before we change anything
    storeOriginalHUDPos(node);

    node->setRotation(flipped ? 180.f : 0.f);

    if (flipped) {
        // Mirror position across screen centre: (x,y) → (W-x, H-y)
        CCPoint orig = s_originalHUDPositions[node];
        node->setPosition({winSize.width - orig.x, winSize.height - orig.y});
    } else {
        // Restore original position
        if (s_originalHUDPositions.count(node))
            node->setPosition(s_originalHUDPositions[node]);
    }
}

void resetHUDNodes(PlayLayer* pl) {
    if (!pl) return;
    for (auto* n : {(CCNode*)pl->m_percentageLabel,
                    (CCNode*)pl->m_attemptLabel,
                    (CCNode*)pl->m_progressBar}) {
        if (!n) continue;
        n->setRotation(0.f);
        if (s_originalHUDPositions.count(n))
            n->setPosition(s_originalHUDPositions[n]);
    }
    clearStoredHUDPositions();
}

// ─────────────────────────────────────────────
//  Hook PlayerObject
// ─────────────────────────────────────────────
class $modify(MyPlayerObject, PlayerObject) {
    void flipGravity(bool isFlipped, bool noEffects) {
        PlayerObject::flipGravity(isFlipped, noEffects);
        if (!s_modEnabled) return;
        if (m_isSecondPlayer) return;

        auto* pl = PlayLayer::get();
        if (!pl) return;

        float speed = static_cast<float>(Mod::get()->getSettingValue<double>("rotation-speed"));
        s_targetAngle = isFlipped ? 180.f : 0.f;

        // Rotate the game wrapper (objects, ground, bg — everything in it)
        animateRotation(getGameWrapper(pl), s_targetAngle, speed);

        // If UI flip on, rotate HUD nodes directly (no reparenting)
        if (s_flipUI) {
            setHUDNodeFlipped(pl->m_percentageLabel, isFlipped);
            setHUDNodeFlipped(pl->m_attemptLabel, isFlipped);
            setHUDNodeFlipped(pl->m_progressBar, isFlipped);
        }
    }
};

// ─────────────────────────────────────────────
//  Hook PlayLayer
// ─────────────────────────────────────────────
class $modify(MyPlayLayer, PlayLayer) {

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        s_modEnabled  = Mod::get()->getSettingValue<bool>("enabled");
        s_flipUI      = Mod::get()->getSettingValue<bool>("rotate-ui");
        s_targetAngle = 0.f;

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        // Single game wrapper — reparent m_objectLayer into it exactly
        // as the original working code did
        auto* wrapper = CCNode::create();
        wrapper->setID("gravity-game-wrapper"_spr);
        wrapper->setAnchorPoint({0.5f, 0.5f});
        wrapper->setPosition(winSize / 2);
        wrapper->setContentSize(winSize);

        if (m_objectLayer) {
            CCPoint origPos = m_objectLayer->getPosition();
            m_objectLayer->retain();
            m_objectLayer->removeFromParentAndCleanup(false);
            m_objectLayer->setPosition(origPos - CCPoint(winSize / 2));
            wrapper->addChild(m_objectLayer);
            m_objectLayer->release();
        }

        this->addChild(wrapper, 1);
        return true;
    }

    void resetLevel() {
        // Snap wrapper back to 0
        if (auto* w = getGameWrapper(this)) {
            w->stopAllActions();
            w->setRotation(0.f);
        }
        resetHUDNodes(this);
        s_targetAngle = 0.f;
        PlayLayer::resetLevel();
    }

    void onQuit() {
        s_targetAngle = 0.f;
        PlayLayer::onQuit();
    }
};

// ─────────────────────────────────────────────
//  Hook PauseLayer
// ─────────────────────────────────────────────
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
            if (auto* w = getGameWrapper(pl)) {
                w->stopAllActions(); w->setRotation(0.f);
            }
            resetHUDNodes(pl);
        }
    }

    void onToggleUI(CCObject* sender) {
        s_flipUI = !s_flipUI;
        Mod::get()->setSettingValue("rotate-ui", s_flipUI);
        auto* spr = static_cast<ButtonSprite*>(
            static_cast<CCMenuItemSpriteExtra*>(sender)->getNormalImage());
        spr->setString(s_flipUI ? "UI Flip ON" : "UI Flip OFF");

        auto* pl = PlayLayer::get();
        if (!pl) return;

        bool isFlipped = (s_targetAngle == 180.f);

        if (s_flipUI) {
            // Sync HUD to current gravity state
            setHUDNodeFlipped(pl->m_percentageLabel, isFlipped);
            setHUDNodeFlipped(pl->m_attemptLabel, isFlipped);
            setHUDNodeFlipped(pl->m_progressBar, isFlipped);
        } else {
            // Restore HUD to normal
            resetHUDNodes(pl);
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
        menu->setPosition({ 60.f, 40.f });
        menu->setID("gravity-camera-menu"_spr);
        this->addChild(menu, 10);
    }
};
