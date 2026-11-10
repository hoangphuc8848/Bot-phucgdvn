#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/GameObject.hpp>
#include <algorithm>
#include <cmath>

using namespace geode::prelude;

// Standalone, in-game utility menu. Every toggle is local to this mod/session.
// This first pass deliberately uses stable Geode APIs and avoids claiming
// compatibility with unrelated mods or online leaderboard bypasses.
class $modify(UltimateHackMenu, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        auto menu = CCMenu::create();
        if (!menu) return true;
        auto open = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("ULTIMATE", 92, 0, 0.55f, true, "goldFont.fnt", "GJ_button_05.png", 30.f),
            this, menu_selector(UltimateHackMenu::onOpen));
        if (open) {
            open->setPosition({-150.f, -90.f});
            menu->addChild(open);
        }
        menu->setPosition({0.f, 0.f});
        this->addChild(menu, 1000);
        return true;
    }

    void onOpen(CCObject*) {
        FLAlertLayer::create(
            "ULTIMATE HACK MENU",
            "V1 menu shell is installed.\n\n"
            "Tabs planned: Player, Gameplay, Visual, Level, Practice and Bot.\n"
            "This build adds the menu entry first; individual options are being implemented and tested in stages.",
            "OK"
        )->show();
    }
};

class $modify(AIAutoPlayLayer, PlayLayer) {
    struct Fields {
        bool enabled = false;
        bool holdingJump = false;
        float releaseTimer = 0.f;
        float previousPlayerX = 0.f;
        float speedEstimate = 0.f;
        float reactionFrames = 4.0f;
        bool initializedPosition = false;
        float jumpCooldown = 0.f;
        GameObject* lastTriggeredHazard = nullptr;
        ButtonSprite* botSprite = nullptr;
        bool deathRecorded = false;
        int deathCount = 0;
        float lastDeathX = -1.f;
    };

    void onEnter() {
        PlayLayer::onEnter();
        m_fields->deathCount = Mod::get()->getSavedValue<int>("learning.deaths", 0);
        m_fields->reactionFrames = Mod::get()->getSavedValue<float>("learning.reaction", 4.f);
        m_fields->lastDeathX = Mod::get()->getSavedValue<float>("learning.last-death-x", -1.f);
        auto win = CCDirector::sharedDirector()->getWinSize();
        auto menu = CCMenu::create();
        if (!menu) return;
        auto make = [&](char const* label, SEL_MenuHandler handler) {
            auto sprite = ButtonSprite::create(label, 64, 0, 0.55f, true, "goldFont.fnt", "GJ_button_04.png", 24.f);
            return CCMenuItemSpriteExtra::create(sprite, this, handler);
        };
        auto bot = make("BOT OFF", menu_selector(AIAutoPlayLayer::onBotToggle));
        auto jump = make("JUMP", menu_selector(AIAutoPlayLayer::onManualJump));
        auto release = make("RELEASE", menu_selector(AIAutoPlayLayer::onManualRelease));
        auto slower = make("R-", menu_selector(AIAutoPlayLayer::onReactionDown));
        auto faster = make("R+", menu_selector(AIAutoPlayLayer::onReactionUp));
        auto reset = make("R0", menu_selector(AIAutoPlayLayer::onReactionReset));
        CCMenuItemSpriteExtra* items[] = {bot, jump, release, slower, faster, reset};
        for (int i = 0; i < 6; ++i) {
            if (items[i]) { items[i]->setPosition({(i - 2.5f) * 70.f, 0.f}); menu->addChild(items[i]); }
        }
        menu->setPosition({win.width / 2.f, 76.f});
        auto scene = CCDirector::sharedDirector()->getRunningScene();
        if (scene) scene->addChild(menu, 100000);
    }

    void onBotToggle(CCObject*) {
        m_fields->enabled = !m_fields->enabled;
        if (!m_fields->enabled) releaseJump();
        if (m_fields->botSprite) m_fields->botSprite->setString(m_fields->enabled ? "BOT ON" : "BOT OFF");
    }
    void onManualJump(CCObject*) {
        if (m_player1) { m_player1->pushButton(PlayerButton::Jump); m_fields->holdingJump = true; m_fields->releaseTimer = 0.09f; }
    }
    void onManualRelease(CCObject*) { releaseJump(); }
    void onReactionDown(CCObject*) { m_fields->reactionFrames = std::max(1.f, m_fields->reactionFrames - .25f); }
    void onReactionUp(CCObject*) { m_fields->reactionFrames = std::min(10.f, m_fields->reactionFrames + .25f); }
    void onReactionReset(CCObject*) {
        m_fields->reactionFrames = 4.f;
        m_fields->lastTriggeredHazard = nullptr;
        Mod::get()->setSavedValue<float>("learning.reaction", 4.f);
    }
    void onShowMemory(CCObject*) {
        auto message = fmt::format(
            "Deaths learned: {}\nReaction timing: {:.2f} frames\nLast death X: {:.1f}\n\nThe bot stores these values between attempts and shifts its next jump earlier after a death.",
            m_fields->deathCount, m_fields->reactionFrames, m_fields->lastDeathX
        );
        FLAlertLayer::create("BOT MEMORY", message, "OK")->show();
    }
    void learnFromDeath() {
        if (m_fields->deathRecorded) return;
        m_fields->deathRecorded = true;
        ++m_fields->deathCount;
        m_fields->lastDeathX = m_player1 ? m_player1->getPositionX() : -1.f;
        // A death is treated as a late reaction. Move the next attempt slightly earlier.
        m_fields->reactionFrames = std::min(10.f, m_fields->reactionFrames + 0.5f);
        Mod::get()->setSavedValue<int>("learning.deaths", m_fields->deathCount);
        Mod::get()->setSavedValue<float>("learning.reaction", m_fields->reactionFrames);
        Mod::get()->setSavedValue<float>("learning.last-death-x", m_fields->lastDeathX);
    }

    void releaseJump() {
        if (m_player1 && m_fields->holdingJump) m_player1->releaseButton(PlayerButton::Jump);
        m_fields->holdingJump = false;
        m_fields->releaseTimer = 0.f;
    }
    GameObject* getNextHazard(float x) {
        if (!m_objects) return nullptr;
        GameObject* best = nullptr;
        float bestDistance = 100000.f;
        for (auto object : geode::cocos::CCArrayExt<GameObject, false>(m_objects)) {
            if (!object || object->m_objectType != GameObjectType::Hazard) continue;
            float d = object->getPositionX() - x;
            if (d > 0.f && d < bestDistance) { bestDistance = d; best = object; }
        }
        return best;
    }
    void update(float dt) {
        PlayLayer::update(dt);
        if (!m_player1) return;
        float x = m_player1->getPositionX();
        if (!m_fields->initializedPosition) { m_fields->previousPlayerX = x; m_fields->initializedPosition = true; }
        if (dt > 0.f) {
            float v = std::abs(x - m_fields->previousPlayerX) / dt;
            if (v > 0.f && v < 5000.f) m_fields->speedEstimate = m_fields->speedEstimate == 0.f ? v : m_fields->speedEstimate * .75f + v * .25f;
        }
        m_fields->previousPlayerX = x;
        if (m_fields->holdingJump && (m_fields->releaseTimer -= dt) <= 0.f) releaseJump();
        if (m_fields->jumpCooldown > 0.f) m_fields->jumpCooldown -= dt;
        if (m_player1->m_isDead) { learnFromDeath(); releaseJump(); return; }
        if (!m_fields->enabled || !m_player1->m_isOnGround || m_fields->jumpCooldown > 0.f) return;
        auto hazard = getNextHazard(x);
        if (!hazard || hazard == m_fields->lastTriggeredHazard) return;
        float hw = hazard->getContentSize().width * std::abs(hazard->getScaleX()) * .5f;
        float pw = m_player1->getContentSize().width * std::abs(m_player1->getScaleX()) * .5f;
        float distance = hazard->getPositionX() - hw - (x + pw);
        float reactionDistance = std::max(24.f, m_fields->speedEstimate * (m_fields->reactionFrames / 60.f)) + 10.f;
        if (distance <= reactionDistance && distance > -4.f && !m_fields->holdingJump) {
            m_player1->pushButton(PlayerButton::Jump);
            m_fields->holdingJump = true;
            m_fields->releaseTimer = .065f;
            m_fields->jumpCooldown = .10f;
            m_fields->lastTriggeredHazard = hazard;
        }
    }
};

$on_mod(Loaded) { log::info("[Ultimate Hack Menu] Loaded"); }
