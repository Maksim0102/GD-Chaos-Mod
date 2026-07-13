#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/binding/EnhancedGameObject.hpp>
#include <random>
#include <vector>
#include <string>
#include <utility>
#include <cmath>

using namespace geode::prelude;

namespace ChaosState {
    inline bool  s_isOldTVActive   = false;
    inline bool  s_isMirrorActive  = false;
    inline bool  s_naturalFlipped  = false;
    inline bool  s_isApplyingMirrorEvent = false;
}

enum class ChaosEvent {
    None     = -1,
    SpeedhackFast = 0,
    SpeedhackSlow = 1,
    Mirror   = 2,
    OldTV    = 3,
    Flashbang= 4,
    Stop     = 6,
    Reverse  = 7,
    Noclip   = 9,
    Normal     = 10,
    Spin       = 11,
    Invisible  = 12
};

namespace ChaosRandom {
    inline float randomFloat(float min, float max) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(min, max);
        return dis(gen);
    }
    inline int randomInt(int min, int max) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dis(min, max);
        return dis(gen);
    }
}

static void drawSolidRect(CCDrawNode* drawNode, CCPoint origin, CCPoint destination, ccColor4F color) {
    CCPoint verts[] = {
        origin,
        ccp(destination.x, origin.y),
        destination,
        ccp(origin.x, destination.y)
    };
    drawNode->drawPolygon(verts, 4, color, 0, ccc4f(0,0,0,0));
}

static void setPlayerOpacity(PlayerObject* player, GLubyte opacity) {
    if (!player) return;
    player->setOpacity(opacity);
    if (player->getChildren()) {
        for (int i = 0; i < player->getChildren()->count(); ++i) {
            auto child = static_cast<CCNode*>(player->getChildren()->objectAtIndex(i));
            if (auto sprite = dynamic_cast<CCSprite*>(child)) {
                sprite->setOpacity(opacity);
            }
        }
    }
}

static void createIceShatterEffect(CCNode* parent, CCPoint pos) {
    if (!parent) return;
    for (int i = 0; i < 12; ++i) {
        auto piece = CCSprite::createWithSpriteFrameName("square02_001.png");
        if (!piece) {
            piece = CCSprite::create();
        }
        piece->setColor(ccc3(180, 240, 255));
        piece->setOpacity(220);
        piece->setScale(ChaosRandom::randomFloat(0.1f, 0.3f));
        piece->setPosition(pos);
        parent->addChild(piece, 100);

        float angle = ChaosRandom::randomFloat(0.f, 360.f) * 3.14159f / 180.f;
        float dist = ChaosRandom::randomFloat(20.f, 60.f);
        CCPoint targetPos = ccp(cos(angle) * dist, sin(angle) * dist);

        auto move = CCMoveBy::create(0.5f, targetPos);
        auto rotate = CCRotateBy::create(0.5f, ChaosRandom::randomFloat(-360.f, 360.f));
        auto fade = CCFadeOut::create(0.5f);
        auto remove = CCRemoveSelf::create();

        piece->runAction(CCSequence::create(
            CCSpawn::create(move, rotate, fade, nullptr),
            remove,
            nullptr
        ));
    }
}

static void applyGamemode(PlayerObject* player, int mode) {
    if (!player) return;
    
    player->toggleFlyMode(false, true);
    player->toggleRollMode(false, true);
    player->toggleBirdMode(false, true);
    player->toggleDartMode(false, true);
    player->toggleRobotMode(false, true);
    player->toggleSpiderMode(false, true);
    player->toggleSwingMode(false, true);
    
    switch (mode) {
        case 0:
            break;
        case 1:
            player->toggleFlyMode(true, true);
            break;
        case 2:
            player->toggleRollMode(true, true);
            break;
        case 3:
            player->toggleBirdMode(true, true);
            break;
        case 4:
            player->toggleDartMode(true, true);
            break;
        case 5:
            player->toggleRobotMode(true, true);
            break;
        case 6:
            player->toggleSpiderMode(true, true);
            break;
        case 7:
            player->toggleSwingMode(true, true);
            break;
    }
}

static void crossOutPortal(GameObject* portal) {
    if (!portal || portal->getChildByTag(9999)) return;
    
    auto drawNode = CCDrawNode::create();
    float w = portal->getContentSize().width;
    float h = portal->getContentSize().height;
    
    drawNode->drawSegment(ccp(0, 0), ccp(w, h), 2.0f, ccc4f(1.f, 0.f, 0.f, 1.f));
    drawNode->drawSegment(ccp(w, 0), ccp(0, h), 2.0f, ccc4f(1.f, 0.f, 0.f, 1.f));
    
    drawNode->setTag(9999);
    portal->addChild(drawNode, 100);
}

static void removeCrossedOutPortals(CCArray* objects) {
    if (!objects) return;
    for (int i = 0; i < objects->count(); ++i) {
        auto obj = static_cast<GameObject*>(objects->objectAtIndex(i));
        if (obj && obj->getChildByTag(9999)) {
            obj->removeChildByTag(9999);
        }
    }
}

class ColorVignette : public CCNode {
public:
    CCLayerGradient* m_top;
    CCLayerGradient* m_bottom;
    CCLayerGradient* m_left;
    CCLayerGradient* m_right;

    static ColorVignette* create() {
        auto ret = new ColorVignette();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool init() {
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        float thickness = 45.0f;

        m_top = CCLayerGradient::create(ccc4(0, 0, 0, 160), ccc4(0, 0, 0, 0), ccp(0, -1));
        m_top->setContentSize(ccp(winSize.width, thickness));
        m_top->setPosition(ccp(0, winSize.height - thickness));
        addChild(m_top);

        m_bottom = CCLayerGradient::create(ccc4(0, 0, 0, 160), ccc4(0, 0, 0, 0), ccp(0, 1));
        m_bottom->setContentSize(ccp(winSize.width, thickness));
        m_bottom->setPosition(ccp(0, 0));
        addChild(m_bottom);

        m_left = CCLayerGradient::create(ccc4(0, 0, 0, 160), ccc4(0, 0, 0, 0), ccp(1, 0));
        m_left->setContentSize(ccp(thickness, winSize.height));
        m_left->setPosition(ccp(0, 0));
        addChild(m_left);

        m_right = CCLayerGradient::create(ccc4(0, 0, 0, 160), ccc4(0, 0, 0, 0), ccp(-1, 0));
        m_right->setContentSize(ccp(thickness, winSize.height));
        m_right->setPosition(ccp(winSize.width - thickness, 0));
        addChild(m_right);

        setVisible(false);
        return true;
    }

    void setColor(ccColor3B color, GLubyte maxOpacity = 160) {
        m_top->setStartColor(color);
        m_top->setStartOpacity(maxOpacity);
        m_top->setEndColor(color);
        m_top->setEndOpacity(0);

        m_bottom->setStartColor(color);
        m_bottom->setStartOpacity(maxOpacity);
        m_bottom->setEndColor(color);
        m_bottom->setEndOpacity(0);

        m_left->setStartColor(color);
        m_left->setStartOpacity(maxOpacity);
        m_left->setEndColor(color);
        m_left->setEndOpacity(0);

        m_right->setStartColor(color);
        m_right->setStartOpacity(maxOpacity);
        m_right->setEndColor(color);
        m_right->setEndOpacity(0);
    }
};

class OldTVOverlay : public CCNode {
public:
    CCDrawNode* m_lines;

    static OldTVOverlay* create() {
        auto ret = new OldTVOverlay();
        if (ret && ret->init()) { ret->autorelease(); return ret; }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool init() {
        m_lines = CCDrawNode::create();
        addChild(m_lines);
        scheduleUpdate();
        return true;
    }

    void update(float dt) override {
        if (!isVisible()) return;
        m_lines->clear();
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        for (float y = 4.f; y < winSize.height; y += 8.f) {
            m_lines->drawSegment(ccp(0.f, y), ccp(winSize.width, y), 1.1f, ccc4f(0.f, 0.f, 0.f, 0.13f));
        }
    }
};

class VHSEffectNode : public CCNode {
public:
    CCDrawNode* m_drawNode;
    CCLabelBMFont* m_rewLabel;

    static VHSEffectNode* create() {
        auto ret = new VHSEffectNode();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool init() {
        m_drawNode = CCDrawNode::create();
        addChild(m_drawNode);

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        m_rewLabel = CCLabelBMFont::create("<< REW", "bigFont.fnt");
        m_rewLabel->setPosition(ccp(winSize.width - 65, winSize.height - 35));
        m_rewLabel->setScale(0.55f);
        m_rewLabel->setColor(ccc3(255, 0, 50));
        addChild(m_rewLabel);

        m_rewLabel->runAction(CCRepeatForever::create(CCSequence::create(
            CCFadeTo::create(0.18f, 40),
            CCFadeTo::create(0.18f, 255),
            nullptr
        )));

        scheduleUpdate();
        return true;
    }

    void update(float dt) override {
        if (!isVisible()) return;

        m_drawNode->clear();
        auto winSize = CCDirector::sharedDirector()->getWinSize();

        for (int i = 0; i < 4; ++i) {
            float y = ChaosRandom::randomFloat(0.f, winSize.height);
            float h = ChaosRandom::randomFloat(6.f, 35.f);
            drawSolidRect(m_drawNode, ccp(0, y), ccp(winSize.width, y + h), ccc4f(1.f, 1.f, 1.f, ChaosRandom::randomFloat(0.06f, 0.22f)));
        }

        for (int i = 0; i < 3; ++i) {
            float x = ChaosRandom::randomFloat(0.f, winSize.width);
            float y = ChaosRandom::randomFloat(0.f, winSize.height);
            float w = ChaosRandom::randomFloat(30.f, 180.f);
            float h = ChaosRandom::randomFloat(12.f, 45.f);
            drawSolidRect(m_drawNode, ccp(x, y), ccp(x + w, y + h), ccc4f(1.f, 1.f, 1.f, ChaosRandom::randomFloat(0.12f, 0.35f)));
        }

        float step = 8.0f;
        for (float y = 0; y < winSize.height; y += step) {
            m_drawNode->drawSegment(ccp(0, y), ccp(winSize.width, y), 0.9f, ccc4f(0.f, 0.f, 0.f, 0.18f));
        }
    }
};

class ChaosHUD : public CCNode {
public:
    CCLabelBMFont* m_eventLabel;
    CCLabelBMFont* m_counterLabel;
    CCLabelBMFont* m_timerLabel;
    CCLabelBMFont* m_nextLabel;
    CCScale9Sprite* m_eventBg;
    CCScale9Sprite* m_nextBg;

    static ChaosHUD* create() {
        auto ret = new ChaosHUD();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool init() {
        auto winSize = CCDirector::sharedDirector()->getWinSize();

        float panelW = 150.f;
        float panelH = 45.f;
        float panelCX = panelW / 2.f + 6.f;
        float panelCY = winSize.height - panelH / 2.f - 6.f;

        m_eventBg = CCScale9Sprite::create("GJ_square02.png");
        m_eventBg->setContentSize(ccp(panelW, panelH));
        m_eventBg->setPosition(ccp(panelCX, panelCY));
        m_eventBg->setOpacity(255);
        m_eventBg->setColor(ccc3(255, 255, 255));
        addChild(m_eventBg);

        m_eventLabel = CCLabelBMFont::create("NORMAL", "bigFont.fnt");
        m_eventLabel->setPosition(ccp(panelCX, panelCY + 9.f));
        m_eventLabel->setScale(0.34f);
        m_eventLabel->setColor(ccc3(255, 255, 255));
        addChild(m_eventLabel);

        m_timerLabel = CCLabelBMFont::create("10s", "bigFont.fnt");
        m_timerLabel->setAnchorPoint(ccp(0.f, 0.5f));
        m_timerLabel->setPosition(ccp(panelCX - 67.f, panelCY - 11.f));
        m_timerLabel->setScale(0.29f);
        m_timerLabel->setColor(ccc3(0, 240, 255));
        addChild(m_timerLabel);

        m_counterLabel = CCLabelBMFont::create("Survived: 0", "bigFont.fnt");
        m_counterLabel->setAnchorPoint(ccp(1.f, 0.5f));
        m_counterLabel->setPosition(ccp(panelCX + 67.f, panelCY - 11.f));
        m_counterLabel->setScale(0.26f);
        m_counterLabel->setColor(ccc3(255, 220, 0));
        addChild(m_counterLabel);

        m_nextBg = CCScale9Sprite::create("GJ_square01.png");
        m_nextBg->setContentSize(ccp(260, 36));
        m_nextBg->setPosition(ccp(winSize.width / 2, 45));
        m_nextBg->setOpacity(255);
        m_nextBg->setColor(ccc3(255, 255, 255));
        m_nextBg->setVisible(false);
        addChild(m_nextBg);

        m_nextLabel = CCLabelBMFont::create("NEXT: SPEEDUP", "bigFont.fnt");
        m_nextLabel->setPosition(m_nextBg->getPosition());
        m_nextLabel->setScale(0.44f);
        m_nextLabel->setColor(ccc3(255, 190, 0));
        m_nextLabel->setVisible(false);
        addChild(m_nextLabel);

        return true;
    }

    void updateEvent(const std::string& name, ccColor3B color) {
        m_eventLabel->setString(name.c_str());
        m_eventLabel->setColor(color);
        m_eventLabel->stopAllActions();
        m_eventLabel->setScale(0.34f);
        m_eventLabel->runAction(CCSequence::create(
            CCScaleTo::create(0.1f, 0.44f),
            CCScaleTo::create(0.1f, 0.34f),
            nullptr
        ));
    }

    void updateCounter(int count) {
        std::string s = "Survived: " + std::to_string(count);
        m_counterLabel->setString(s.c_str());
    }

    void updateTimer(float timeLeft) {
        int secs = (int)ceilf(fmaxf(timeLeft, 0.f));
        m_timerLabel->setString((std::to_string(secs) + "s").c_str());
        m_timerLabel->setColor(secs <= 3 ? ccc3(255, 60, 60) : ccc3(200, 200, 200));
    }

    void showNextPreview(const std::string& name) {
        m_nextLabel->setString(("NEXT: " + name).c_str());
        m_nextBg->setVisible(true);
        m_nextLabel->setVisible(true);
        m_nextLabel->stopAllActions();
        m_nextLabel->setScale(0.44f);
        m_nextLabel->runAction(CCRepeatForever::create(CCSequence::create(
            CCScaleTo::create(0.45f, 0.50f),
            CCScaleTo::create(0.45f, 0.44f),
            nullptr
        )));
    }

    void hideNextPreview() {
        m_nextBg->setVisible(false);
        m_nextLabel->setVisible(false);
        m_nextLabel->stopAllActions();
    }
};

static std::string getEventName(ChaosEvent ev) {
    switch (ev) {
        case ChaosEvent::SpeedhackFast: return "SPEEDUP";
        case ChaosEvent::SpeedhackSlow: return "SLOWDOWN";
        case ChaosEvent::Mirror:        return "MIRROR";
        case ChaosEvent::OldTV:         return "OLD";
        case ChaosEvent::Flashbang:     return "FLASHBANG";
        case ChaosEvent::Noclip:        return "NOCLIP";
        case ChaosEvent::Normal:        return "NORMAL";
        case ChaosEvent::Spin:          return "SPIN";
        case ChaosEvent::Invisible:     return "INVISIBLE";
        default:                        return "NORMAL";
    }
}

static ccColor3B getEventColor(ChaosEvent ev) {
    switch (ev) {
        case ChaosEvent::SpeedhackFast: return ccc3(255, 128, 0);
        case ChaosEvent::SpeedhackSlow: return ccc3(0, 160, 255);
        case ChaosEvent::Mirror:        return ccc3(180, 0, 255);
        case ChaosEvent::OldTV:         return ccc3(150, 150, 150);
        case ChaosEvent::Flashbang:     return ccc3(255, 255, 255);
        case ChaosEvent::Noclip:        return ccc3(255, 0, 200);
        case ChaosEvent::Normal:        return ccc3(255, 255, 255);
        case ChaosEvent::Spin:          return ccc3(100, 255, 140);
        case ChaosEvent::Invisible:     return ccc3(80, 0, 160);
        default:                        return ccc3(255, 255, 255);
    }
}

static bool getEventSetting(const char* key) {
    auto* mod = geode::Mod::get();
    if (!mod) return true;
    return mod->getSettingValue<bool>(key);
}

static ChaosEvent getWeightedRandomEvent(ChaosEvent current, bool isFirst) {
    std::vector<std::pair<ChaosEvent, int>> pool;
    if (getEventSetting("event_speedup"))   pool.push_back({ ChaosEvent::SpeedhackFast, 10 });
    if (getEventSetting("event_slowdown"))  pool.push_back({ ChaosEvent::SpeedhackSlow, 10 });
    if (getEventSetting("event_mirror"))    pool.push_back({ ChaosEvent::Mirror,        10 });
    if (getEventSetting("event_oldtv"))     pool.push_back({ ChaosEvent::OldTV,         10 });
    if (getEventSetting("event_flashbang")) pool.push_back({ ChaosEvent::Flashbang,     10 });
    if (getEventSetting("event_spin"))      pool.push_back({ ChaosEvent::Spin,          10 });
    if (getEventSetting("event_noclip"))    pool.push_back({ ChaosEvent::Noclip,        10 });
    if (getEventSetting("event_normal"))    pool.push_back({ ChaosEvent::Normal,        10 });
    if (getEventSetting("event_invisible")) pool.push_back({ ChaosEvent::Invisible,     10 });

    std::vector<std::pair<ChaosEvent, int>> filtered;
    int totalWeight = 0;
    for (const auto& entry : pool) {
        if (entry.first == current) continue;
        filtered.push_back(entry);
        totalWeight += entry.second;
    }

    if (totalWeight <= 0) {
        filtered = pool;
        for (const auto& e : pool) totalWeight += e.second;
    }

    if (totalWeight <= 0) return ChaosEvent::None;

    int rVal = ChaosRandom::randomInt(0, totalWeight - 1);
    int currentSum = 0;
    for (const auto& entry : filtered) {
        currentSum += entry.second;
        if (rVal < currentSum) {
            return entry.first;
        }
    }
    return ChaosEvent::SpeedhackFast;
}

class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        ChaosEvent m_event = ChaosEvent::None;
        ChaosEvent m_next = ChaosEvent::None;
        float m_timer = 0.f;
        int m_survived = 0;
        bool m_first = true;
        float m_speed = 1.0f;
        bool m_noclip = false;
        bool m_hasMoved = false;
        bool m_hudScene = false;
        CCLayerColor* m_invisLayer = nullptr;
        ChaosHUD* m_hud = nullptr;
        ColorVignette* m_vignette = nullptr;
        OldTVOverlay* m_oldTV = nullptr;
        VHSEffectNode* m_vhsNode = nullptr;
        CCLayerColor* m_flashLayer = nullptr;
        bool m_flashTriggered = false;
        float m_freq = 0.f;
        bool m_firstTry = true;
        float m_introTimer = 0.f;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        m_fields->m_hud = ChaosHUD::create();
        m_fields->m_vignette = ColorVignette::create();

        m_fields->m_oldTV = OldTVOverlay::create();
        m_fields->m_oldTV->setVisible(false);

        m_fields->m_vhsNode = VHSEffectNode::create();
        m_fields->m_vhsNode->setVisible(false);

        m_fields->m_flashLayer = CCLayerColor::create(ccc4(255, 255, 255, 0));
        m_fields->m_invisLayer = CCLayerColor::create(ccc4(0, 0, 0, 0));

        if (m_uiLayer) {
            m_uiLayer->addChild(m_fields->m_invisLayer, 2);
            m_uiLayer->addChild(m_fields->m_oldTV,   5);
            m_uiLayer->addChild(m_fields->m_hud,     200);
            m_uiLayer->addChild(m_fields->m_vignette, 180);
            m_uiLayer->addChild(m_fields->m_vhsNode,  195);
            m_uiLayer->addChild(m_fields->m_flashLayer, 1000);
        } else {
            addChild(m_fields->m_invisLayer, 2);
            addChild(m_fields->m_oldTV,   5);
            addChild(m_fields->m_hud,     200);
            addChild(m_fields->m_vignette, 180);
            addChild(m_fields->m_vhsNode,  195);
            addChild(m_fields->m_flashLayer, 1000);
        }

        m_fields->m_event = ChaosEvent::None;
        m_fields->m_next = ChaosEvent::None;
        m_fields->m_timer = 0.f;
        m_fields->m_survived = 0;
        m_fields->m_first = true;
        m_fields->m_firstTry = true;
        m_fields->m_introTimer = 0.f;

        ChaosState::s_isMirrorActive = false;
        ChaosState::s_naturalFlipped = false;
        ChaosState::s_isApplyingMirrorEvent = false;

        return true;
    }

    void restoreHudToUILayer() {
        if (!m_fields->m_hudScene) return;
        auto scene = CCDirector::sharedDirector()->getRunningScene();
        if (scene && m_fields->m_hud && m_uiLayer) {
            m_fields->m_hud->retain();
            scene->removeChild(m_fields->m_hud, false);
            m_uiLayer->addChild(m_fields->m_hud, 200);
            m_fields->m_hud->release();
        }
        m_fields->m_hudScene = false;
    }

    void applyLevelOpacity(GLubyte opacity) {
        if (!m_objects) return;
        for (int i = 0; i < m_objects->count(); i++) {
            auto obj = static_cast<CCSprite*>(m_objects->objectAtIndex(i));
            if (obj) obj->setOpacity(opacity);
        }
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        if (m_fields->m_event != ChaosEvent::None) {
            m_fields->m_timer += dt;

            if (m_fields->m_event == ChaosEvent::SpeedhackFast) {
                CCDirector::sharedDirector()->getScheduler()->setTimeScale(1.5f);
            } else if (m_fields->m_event == ChaosEvent::SpeedhackSlow) {
                CCDirector::sharedDirector()->getScheduler()->setTimeScale(0.75f);
            }

            if (m_fields->m_event == ChaosEvent::Invisible) {
                applyLevelOpacity(13);
            }

            if (m_fields->m_event == ChaosEvent::OldTV) {
                if (m_shaderLayer) {
                    m_shaderLayer->triggerGrayscale(0.f, 1.f, false, 0, 0, 0.f);
                }
            }

            if (m_fields->m_event == ChaosEvent::Flashbang && m_fields->m_timer >= 5.0f && !m_fields->m_flashTriggered) {
                m_fields->m_flashTriggered = true;
                m_fields->m_flashLayer->stopAllActions();
                m_fields->m_flashLayer->setOpacity(255);
                m_fields->m_flashLayer->runAction(CCSequence::create(
                    CCDelayTime::create(0.5f),
                    CCFadeOut::create(1.3f),
                    nullptr
                ));
                FMODAudioEngine::sharedEngine()->playEffect("unlockedPath.ogg");
            }

            if (m_fields->m_hud) {
                m_fields->m_hud->updateTimer(10.f - m_fields->m_timer);
            }

            if (m_fields->m_timer >= 8.5f && m_fields->m_next == ChaosEvent::None) {
                m_fields->m_next = getWeightedRandomEvent(m_fields->m_event, false);
                if (m_fields->m_next != ChaosEvent::None) {
                    m_fields->m_hud->showNextPreview(getEventName(m_fields->m_next));
                    FMODAudioEngine::sharedEngine()->playEffect("clickBtn.ogg");
                } else {
                    m_fields->m_next = ChaosEvent::None;
                }
            }

            if (m_fields->m_timer >= 10.0f) {
                if (m_fields->m_event == ChaosEvent::Flashbang) {
                    m_fields->m_flashLayer->stopAllActions();
                    m_fields->m_flashLayer->setOpacity(255);
                    m_fields->m_flashLayer->runAction(CCSequence::create(
                        CCDelayTime::create(0.5f),
                        CCFadeOut::create(2.5f),
                        nullptr
                    ));
                    FMODAudioEngine::sharedEngine()->playEffect("unlockedPath.ogg");
                }

                deactivateCurrentEvent();

                m_fields->m_survived++;
                m_fields->m_hud->updateCounter(m_fields->m_survived);

                auto nextEv = m_fields->m_next;
                m_fields->m_next = ChaosEvent::None;
                if (nextEv != ChaosEvent::None) {
                    activateEvent(nextEv);
                } else {
                    m_fields->m_event = ChaosEvent::None;
                    m_fields->m_timer = 0.f;
                }
            }

        } else {
            if (!m_fields->m_hasMoved) {
                if (m_fields->m_firstTry) {
                    m_fields->m_introTimer += dt;
                    if (m_fields->m_introTimer < 1.0f) {
                        return;
                    }
                }
                if (m_player1 && !m_player1->m_isDead && m_player1->getCurrentXVelocity() > 0.01) {
                    m_fields->m_hasMoved = true;
                }
                return;
            }

            m_fields->m_timer += dt;

            if (m_fields->m_hud)
                m_fields->m_hud->updateTimer(10.f - m_fields->m_timer);

            if (m_fields->m_timer >= 8.5f && m_fields->m_next == ChaosEvent::None) {
                m_fields->m_next = getWeightedRandomEvent(ChaosEvent::None, true);
                if (m_fields->m_next != ChaosEvent::None) {
                    m_fields->m_hud->showNextPreview(getEventName(m_fields->m_next));
                    FMODAudioEngine::sharedEngine()->playEffect("clickBtn.ogg");
                }
            }

            if (m_fields->m_timer >= 10.0f) {
                auto firstEv = m_fields->m_next;
                m_fields->m_next = ChaosEvent::None;
                m_fields->m_first = false;
                if (firstEv != ChaosEvent::None) {
                    activateEvent(firstEv);
                } else {
                    m_fields->m_timer = 0.f;
                }
            }
        }
    }

    void resetLevel() {
        ChaosState::s_isMirrorActive = false;
        ChaosState::s_naturalFlipped = false;
        ChaosState::s_isApplyingMirrorEvent = false;

        PlayLayer::resetLevel();

        if (m_fields->m_vignette) m_fields->m_vignette->setVisible(false);
        if (m_fields->m_oldTV) m_fields->m_oldTV->setVisible(false);
        if (m_fields->m_vhsNode) m_fields->m_vhsNode->setVisible(false);
        if (m_fields->m_flashLayer) {
            m_fields->m_flashLayer->stopAllActions();
            m_fields->m_flashLayer->setOpacity(0);
        }
        if (m_fields->m_hud) {
            m_fields->m_hud->updateEvent("NORMAL", ccc3(255, 255, 255));
            m_fields->m_hud->updateCounter(0);
            m_fields->m_hud->hideNextPreview();
        }

        if (m_player1) {
            m_player1->stopActionByTag(7777);
            setPlayerOpacity(m_player1, 255);
            m_player1->removeChildByTag(8888);
        }
        if (m_player2) {
            m_player2->stopActionByTag(7777);
            setPlayerOpacity(m_player2, 255);
            m_player2->removeChildByTag(8888);
        }

        toggleFlipped(false, true);
        if (m_shaderLayer) m_shaderLayer->triggerGrayscale(0.f, 0.f, false, 0, 0, 0.f);

        this->updateScreenRotation(0.f, false, false, 0.f, 0, 0.f, 0, 0);

        if (m_player1) {
            m_player1->setOpacity(255);
            m_player1->setVisible(true);
            auto ch = m_player1->getChildren();
            if (ch) for (int i = 0; i < ch->count(); i++)
                static_cast<CCNode*>(ch->objectAtIndex(i))->setVisible(true);
        }
        if (m_objectLayer) {
            auto ch = m_objectLayer->getChildren();
            if (ch) for (int i = 0; i < ch->count(); i++) {
                auto node = static_cast<CCNode*>(ch->objectAtIndex(i));
                if (!dynamic_cast<CCSpriteBatchNode*>(node))
                    node->setVisible(true);
            }
        }
        applyLevelOpacity(255);

        if (m_objects) {
            for (int i = 0; i < m_objects->count(); ++i) {
                auto node = static_cast<CCNode*>(m_objects->objectAtIndex(i));
                if (node) node->resumeSchedulerAndActions();
            }
        }

        if (auto ch = FMODAudioEngine::sharedEngine()->getActiveMusicChannel(0)) {
            if (m_fields->m_freq > 0.f)
                ch->setFrequency(m_fields->m_freq);
        }
        m_fields->m_freq = 0.f;

        removeCrossedOutPortals(m_objects);

        CCDirector::sharedDirector()->getScheduler()->setTimeScale(1.0f);

        ChaosState::s_isOldTVActive  = false;

        m_fields->m_event = ChaosEvent::None;
        m_fields->m_next = ChaosEvent::None;
        m_fields->m_timer = 0.f;
        m_fields->m_survived = 0;
        m_fields->m_first = true;
        m_fields->m_hasMoved = false;
        m_fields->m_firstTry = false;
        m_fields->m_introTimer = 0.f;
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (m_fields->m_noclip) {
            return;
        }
        PlayLayer::destroyPlayer(player, object);
    }

    void deactivateCurrentEvent() {
        auto ev = m_fields->m_event;
        if (ev == ChaosEvent::None) return;

        m_fields->m_vignette->setVisible(false);

        switch (ev) {
            case ChaosEvent::SpeedhackFast:
            case ChaosEvent::SpeedhackSlow: {
                CCDirector::sharedDirector()->getScheduler()->setTimeScale(1.0f);
                m_fields->m_speed = 1.0f;
                if (auto ch = FMODAudioEngine::sharedEngine()->getActiveMusicChannel(0)) {
                    if (m_fields->m_freq > 0.f)
                        ch->setFrequency(m_fields->m_freq);
                }
                m_fields->m_freq = 0.f;
                break;
            }
            case ChaosEvent::Mirror: {
                ChaosState::s_isMirrorActive = false;
                ChaosState::s_isApplyingMirrorEvent = true;
                toggleFlipped(ChaosState::s_naturalFlipped, true);
                ChaosState::s_isApplyingMirrorEvent = false;
                break;
            }
            case ChaosEvent::OldTV: {
                m_fields->m_oldTV->setVisible(false);
                ChaosState::s_isOldTVActive = false;
                if (m_shaderLayer) m_shaderLayer->triggerGrayscale(0.f, 0.f, false, 0, 0, 0.f);
                break;
            }
            case ChaosEvent::Flashbang: {
                break;
            }
            case ChaosEvent::Normal: {
                break;
            }
            case ChaosEvent::Invisible: {
                if (m_player1) {
                    m_player1->setVisible(true);
                    m_player1->setOpacity(255);
                    auto ch = m_player1->getChildren();
                    if (ch) for (int i = 0; i < ch->count(); i++)
                        static_cast<CCNode*>(ch->objectAtIndex(i))->setVisible(true);
                }
                if (m_player2) {
                    m_player2->setVisible(true);
                    m_player2->setOpacity(255);
                    auto ch = m_player2->getChildren();
                    if (ch) for (int i = 0; i < ch->count(); i++)
                        static_cast<CCNode*>(ch->objectAtIndex(i))->setVisible(true);
                }
                if (m_objectLayer) {
                    auto ch = m_objectLayer->getChildren();
                    if (ch) for (int i = 0; i < ch->count(); i++) {
                        auto node = static_cast<CCNode*>(ch->objectAtIndex(i));
                        node->setVisible(true);
                    }
                }
                applyLevelOpacity(255);
                break;
            }
            case ChaosEvent::Spin: {
                this->updateScreenRotation(0.f, false, false, 0.f, 0, 0.f, 0, 0);
                break;
            }
            case ChaosEvent::Noclip: {
                m_fields->m_noclip = false;
                if (m_player1) {
                    m_player1->stopActionByTag(7777);
                    setPlayerOpacity(m_player1, 255);
                }
                if (m_player2) {
                    m_player2->stopActionByTag(7777);
                    setPlayerOpacity(m_player2, 255);
                }
                break;
            }
        }

        if (ev != ChaosEvent::Flashbang) {
            FMODAudioEngine::sharedEngine()->playEffect("backBtn.ogg");
        }
    }

    void activateEvent(ChaosEvent ev) {
        m_fields->m_event = ev;
        m_fields->m_timer = 0.f;
        m_fields->m_flashTriggered = false;

        m_fields->m_hud->updateEvent(getEventName(ev), getEventColor(ev));
        m_fields->m_hud->hideNextPreview();

        m_fields->m_vignette->setColor(getEventColor(ev));
        m_fields->m_vignette->setVisible(true);

        switch (ev) {
            case ChaosEvent::SpeedhackFast: {
                m_fields->m_speed = 1.5f;
                CCDirector::sharedDirector()->getScheduler()->setTimeScale(1.5f);
                if (auto ch = FMODAudioEngine::sharedEngine()->getActiveMusicChannel(0)) {
                    if (m_fields->m_freq == 0.f)
                        ch->getFrequency(&m_fields->m_freq);
                    ch->setFrequency(m_fields->m_freq * 1.5f);
                }
                FMODAudioEngine::sharedEngine()->playEffect("achievement.ogg");
                break;
            }
            case ChaosEvent::SpeedhackSlow: {
                m_fields->m_speed = 0.75f;
                CCDirector::sharedDirector()->getScheduler()->setTimeScale(0.75f);
                if (auto ch = FMODAudioEngine::sharedEngine()->getActiveMusicChannel(0)) {
                    if (m_fields->m_freq == 0.f)
                        ch->getFrequency(&m_fields->m_freq);
                    ch->setFrequency(m_fields->m_freq * 0.75f);
                }
                FMODAudioEngine::sharedEngine()->playEffect("achievement.ogg");
                break;
            }
            case ChaosEvent::Mirror: {
                ChaosState::s_isMirrorActive = true;
                ChaosState::s_isApplyingMirrorEvent = true;
                toggleFlipped(!ChaosState::s_naturalFlipped, true);
                ChaosState::s_isApplyingMirrorEvent = false;
                FMODAudioEngine::sharedEngine()->playEffect("achievement.ogg");
                break;
            }
            case ChaosEvent::OldTV: {
                m_fields->m_oldTV->setVisible(true);
                m_fields->m_vignette->setVisible(false);
                ChaosState::s_isOldTVActive = true;
                if (m_shaderLayer)
                    m_shaderLayer->triggerGrayscale(0.f, 1.f, false, 0, 0, 0.f);
                FMODAudioEngine::sharedEngine()->playEffect("achievement.ogg");
                break;
            }
            case ChaosEvent::Flashbang: {
                m_fields->m_vignette->setVisible(false);
                m_fields->m_flashLayer->stopAllActions();
                m_fields->m_flashLayer->setOpacity(255);
                m_fields->m_flashLayer->runAction(CCSequence::create(
                    CCDelayTime::create(0.5f),
                    CCFadeOut::create(2.5f),
                    nullptr
                ));
                FMODAudioEngine::sharedEngine()->playEffect("unlockedPath.ogg");
                break;
            }
            case ChaosEvent::Normal: {
                m_fields->m_vignette->setVisible(false);
                FMODAudioEngine::sharedEngine()->playEffect("clickBtn.ogg");
                break;
            }
            case ChaosEvent::Spin: {
                this->updateScreenRotation(1080.f, false, false, 9.9f, 0, 0.f, 0, 0);
                FMODAudioEngine::sharedEngine()->playEffect("achievement.ogg");
                break;
            }
            case ChaosEvent::Invisible: {
                if (m_player1) {
                    m_player1->setVisible(false);
                    m_player1->setOpacity(0);
                    auto ch = m_player1->getChildren();
                    if (ch) for (int i = 0; i < ch->count(); i++)
                        static_cast<CCNode*>(ch->objectAtIndex(i))->setVisible(false);
                }
                if (m_objectLayer) {
                    auto ch = m_objectLayer->getChildren();
                    if (ch) for (int i = 0; i < ch->count(); i++) {
                        auto node = static_cast<CCNode*>(ch->objectAtIndex(i));
                        if (!dynamic_cast<CCSpriteBatchNode*>(node))
                            node->setVisible(false);
                    }
                }
                applyLevelOpacity(13);
                FMODAudioEngine::sharedEngine()->playEffect("achievement.ogg");
                break;
            }
            case ChaosEvent::Noclip: {
                m_fields->m_noclip = true;
                
                if (m_player1) {
                    auto blink = CCRepeatForever::create(CCSequence::create(
                        CCFadeTo::create(0.15f, 75),
                        CCFadeTo::create(0.15f, 195),
                        nullptr
                    ));
                    blink->setTag(7777);
                    m_player1->runAction(blink);
                }
                if (m_player2 && m_player2->isVisible()) {
                    auto blink = CCRepeatForever::create(CCSequence::create(
                        CCFadeTo::create(0.15f, 75),
                        CCFadeTo::create(0.15f, 195),
                        nullptr
                    ));
                    blink->setTag(7777);
                    m_player2->runAction(blink);
                }
                FMODAudioEngine::sharedEngine()->playEffect("unlockedPath.ogg");
                break;
            }
        }
    }
};

class $modify(MyGJBaseGameLayer, GJBaseGameLayer) {
    void toggleFlipped(bool flipped, bool instant) {
        if (ChaosState::s_isApplyingMirrorEvent) {
            GJBaseGameLayer::toggleFlipped(flipped, instant);
        } else {
            ChaosState::s_naturalFlipped = flipped;
            bool targetVisualFlipped = ChaosState::s_naturalFlipped ^ ChaosState::s_isMirrorActive;
            
            ChaosState::s_isApplyingMirrorEvent = true;
            GJBaseGameLayer::toggleFlipped(targetVisualFlipped, instant);
            ChaosState::s_isApplyingMirrorEvent = false;
        }
    }
};