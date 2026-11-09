#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <algorithm>
#include <cmath>

using namespace geode::prelude;

class $modify(AIAutoPlayLayer, PlayLayer) {
    struct Fields {
        bool enabled = false;
        bool holdingJump = false;
        float releaseTimer = 0.f;
        float previousPlayerX = 0.f;
        float speedEstimate = 0.f;
        float reactionFrames = 2.0f;
        bool initializedPosition = false;
    };

    void onEnter() {
        PlayLayer::onEnter();
        auto win = CCDirector::sharedDirector()->getWinSize();
        auto menu = CCMenu::create();
        if (!menu) return;

        auto botSprite = ButtonSprite::create("BOT OFF", 64, 0, 0.55f, true, "goldFont.fnt", "GJ_button_04.png", 24.f);
        auto jumpSprite = ButtonSprite::create("JUMP", 64, 0, 0.55f, true, "goldFont.fnt", "GJ_button_04.png", 24.f);
        auto releaseSprite = ButtonSprite::create("RELEASE", 64, 0, 0.55f, true, "goldFont.fnt", "GJ_button_04.png", 24.f);
        auto downSprite = ButtonSprite::create("R-", 64, 0, 0.55f, true, "goldFont.fnt", "GJ_button_04.png", 24.f);
        auto upSprite = ButtonSprite::create("R+", 64, 0, 0.55f, true, "goldFont.fnt", "GJ_button_04.png", 24.f);

        auto bot = CCMenuItemSpriteExtra::create(botSprite, this, menu_selector(AIAutoPlayLayer::onBotToggle));
        auto jump = CCMenuItemSpriteExtra::create(jumpSprite, this, menu_selector(AIAutoPlayLayer::onManualJump));
        auto release = CCMenuItemSpriteExtra::create(releaseSprite, this, menu_selector(AIAutoPlayLayer::onManualRelease));
        auto slower = CCMenuItemSpriteExtra::create(downSprite, this, menu_selector(AIAutoPlayLayer::onReactionDown));
        auto faster = CCMenuItemSpriteExtra::create(upSprite, this, menu_selector(AIAutoPlayLayer::onReactionUp));

        if (bot) { bot->setPosition({-150.f, 0.f}); menu->addChild(bot); }
        if (jump) { jump->setPosition({-75.f, 0.f}); menu->addChild(jump); }
        if (release) { release->setPosition({0.f, 0.f}); menu->addChild(release); }
        if (slower) { slower->setPosition({75.f, 0.f}); menu->addChild(slower); }
        if (faster) { faster->setPosition({150.f, 0.f}); menu->addChild(faster); }

        menu->setPosition({win.width / 2.f, 42.f});
        this->addChild(menu, 10000);
        log::info("[AI Auto Play] Controls ready");
    }

    void onBotToggle(CCObject*) {
        m_fields->enabled = !m_fields->enabled;
        if (!m_fields->enabled) releaseJump();
        log::info("[AI Auto Play] Bot {}", m_fields->enabled ? "ON" : "OFF");
    }

    void onManualJump(CCObject*) {
        if (m_player1) {
            m_player1->pushButton(PlayerButton::Jump);
            m_fields->holdingJump = true;
            m_fields->releaseTimer = 0.09f;
        }
    }

    void onManualRelease(CCObject*) { releaseJump(); }

    void onReactionDown(CCObject*) {
        m_fields->reactionFrames = std::max(0.5f, m_fields->reactionFrames - 0.25f);
        log::info("[AI Auto Play] Reaction: {:.2f}", m_fields->reactionFrames);
    }

    void onReactionUp(CCObject*) {
        m_fields->reactionFrames = std::min(6.0f, m_fields->reactionFrames + 0.25f);
        log::info("[AI Auto Play] Reaction: {:.2f}", m_fields->reactionFrames);
    }

    void releaseJump() {
        if (m_player1 && m_fields->holdingJump)
            m_player1->releaseButton(PlayerButton::Jump);
        m_fields->holdingJump = false;
        m_fields->releaseTimer = 0.f;
    }

    GameObject* getNextHazard(float playerX) {
        if (!m_objects) return nullptr;
        GameObject* best = nullptr;
        float bestDistance = 100000.f;
        for (auto object : geode::cocos::CCArrayExt<GameObject, false>(m_objects)) {
            if (!object || object->m_objectType != 2) continue;
            float distance = object->getPositionX() - playerX;
            if (distance > 0.f && distance < bestDistance) {
                bestDistance = distance;
                best = object;
            }
        }
        return best;
    }

    void update(float dt) {
        PlayLayer::update(dt);
        if (!m_player1) return;

        float playerX = m_player1->getPositionX();
        if (!m_fields->initializedPosition) {
            m_fields->previousPlayerX = playerX;
            m_fields->initializedPosition = true;
        }

        if (dt > 0.f) {
            float measured = std::abs(playerX - m_fields->previousPlayerX) / dt;
            if (measured > 0.f && measured < 5000.f) {
                m_fields->speedEstimate = m_fields->speedEstimate == 0.f
                    ? measured : m_fields->speedEstimate * 0.75f + measured * 0.25f;
            }
        }
        m_fields->previousPlayerX = playerX;

        if (m_fields->holdingJump) {
            m_fields->releaseTimer -= dt;
            if (m_fields->releaseTimer <= 0.f) releaseJump();
        }

        if (!m_fields->enabled || m_player1->m_isDead || !m_player1->m_isOnGround) return;
        auto hazard = getNextHazard(playerX);
        if (!hazard) return;

        float distance = hazard->getPositionX() - playerX;
        float halfWidth = m_player1->getContentSize().width * 0.5f;
        float reactionDistance = std::max(18.f, m_fields->speedEstimate *
            (m_fields->reactionFrames / 60.f)) + halfWidth + 8.f;

        if (distance <= reactionDistance && distance > 0.f && !m_fields->holdingJump) {
            m_player1->pushButton(PlayerButton::Jump);
            m_fields->holdingJump = true;
            m_fields->releaseTimer = 0.075f;
        }
    }
};

$on_mod(Loaded) {
    log::info("[AI Auto Play] Loaded - offline Cube controller ready");
}
