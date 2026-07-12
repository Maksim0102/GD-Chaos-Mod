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

// ==========================================
// GLOBALS / CROSS-CLASS SHARED STATUS
// ==========================================
namespace ChaosState {
    inline bool  s_isOldTVActive   = false;

    // Mirror event states
    inline bool  s_isMirrorActive  = false;
    inline bool  s_naturalFlipped  = false;
    inline bool  s_isApplyingMirrorEvent = false;
}

// Chaos Event enumeration
enum class ChaosEvent {
    None     = -1,
    SpeedhackFast = 0, // 1.5x speed
    SpeedhackSlow = 1, // 0.75x speed
    Mirror   = 2,      // horizontally mirror gameplay
    OldTV    = 3,      // black & white filter
    Flashbang= 4,      // blinding flashes
    Stop     = 6,      // freeze player X
    Reverse  = 7,      // rewind 10 seconds
    Noclip   = 9,      // invulnerable ghost
    Normal     = 10,   // no effects (pool event)
    Spin       = 11,   // 1080° rotation over 9.9s
    Invisible  = 12    // player hidden, level 90% transparent
};

// Standard safe random generation namespace
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

// ==========================================
// HELPERS
// ==========================================

// Standard solid rect helper via drawPolygon (safe across all Cocos versions)
static void drawSolidRect(CCDrawNode* drawNode, CCPoint origin, CCPoint destination, ccColor4F color) {
    CCPoint verts[] = {
        origin,
        ccp(destination.x, origin.y),
        destination,
        ccp(origin.x, destination.y)
    };
    drawNode->drawPolygon(verts, 4, color, 0, ccc4f(0,0,0,0));
}

// Change opacity of a player node and its child sprites
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

// Generate a random splash explosion of small ice blocks
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

// Applies a gamemode strictly to a player object
static void applyGamemode(PlayerObject* player, int mode) {
    if (!player) return;
    
    // Deactivate all modes first
    player->toggleFlyMode(false, true);
    player->toggleRollMode(false, true);
    player->toggleBirdMode(false, true);
    player->toggleDartMode(false, true);
    player->toggleRobotMode(false, true);
    player->toggleSpiderMode(false, true);
    player->toggleSwingMode(false, true);
    
    // Activate target
    switch (mode) {
        case 0: // Cube
            break;
        case 1: // Ship
            player->toggleFlyMode(true, true);
            break;
        case 2: // Ball
            player->toggleRollMode(true, true);
            break;
        case 3: // UFO
            player->toggleBirdMode(true, true);
            break;
        case 4: // Wave
            player->toggleDartMode(true, true);
            break;
        case 5: // Robot
            player->toggleRobotMode(true, true);
            break;
        case 6: // Spider
            player->toggleSpiderMode(true, true);
            break;
        case 7: // Swing
            player->toggleSwingMode(true, true);
            break;
    }
}

// Crosses out a gamemode portal visually with a red X
static void crossOutPortal(GameObject* portal) {
    if (!portal || portal->getChildByTag(9999)) return;
    
    auto drawNode = CCDrawNode::create();
    float w = portal->getContentSize().width;
    float h = portal->getContentSize().height;
    
    // Draw red 'X' relative to portal center
    drawNode->drawSegment(ccp(0, 0), ccp(w, h), 2.0f, ccc4f(1.f, 0.f, 0.f, 1.f));
    drawNode->drawSegment(ccp(w, 0), ccp(0, h), 2.0f, ccc4f(1.f, 0.f, 0.f, 1.f));
    
    drawNode->setTag(9999);
    portal->addChild(drawNode, 100);
}

// Removes visual crossed-out indicators from portals
static void removeCrossedOutPortals(CCArray* objects) {
    if (!objects) return;
    for (int i = 0; i < objects->count(); ++i) {
        auto obj = static_cast<GameObject*>(objects->objectAtIndex(i));
        if (obj && obj->getChildByTag(9999)) {
            obj->removeChildByTag(9999);
        }
    }
}

// ==========================================
// ==========================================
// CUSTOM VISUAL NODES
// ==========================================

// Color Vignette Overlay
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

// Old TV scanlines overlay — subtle CRT horizontal lines drawn every frame
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

// VHS Glitch Rewind Overlay
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

        // 1. VHS noise/glitch bands
        for (int i = 0; i < 4; ++i) {
            float y = ChaosRandom::randomFloat(0.f, winSize.height);
            float h = ChaosRandom::randomFloat(6.f, 35.f);
            drawSolidRect(m_drawNode, ccp(0, y), ccp(winSize.width, y + h), ccc4f(1.f, 1.f, 1.f, ChaosRandom::randomFloat(0.06f, 0.22f)));
        }

        // 2. Screen tracking distortion blocks
        for (int i = 0; i < 3; ++i) {
            float x = ChaosRandom::randomFloat(0.f, winSize.width);
            float y = ChaosRandom::randomFloat(0.f, winSize.height);
            float w = ChaosRandom::randomFloat(30.f, 180.f);
            float h = ChaosRandom::randomFloat(12.f, 45.f);
            drawSolidRect(m_drawNode, ccp(x, y), ccp(x + w, y + h), ccc4f(1.f, 1.f, 1.f, ChaosRandom::randomFloat(0.12f, 0.35f)));
        }

        // 3. Static horizontal line mesh
        float step = 8.0f;
        for (float y = 0; y < winSize.height; y += step) {
            m_drawNode->drawSegment(ccp(0, y), ccp(winSize.width, y), 0.9f, ccc4f(0.f, 0.f, 0.f, 0.18f));
        }
    }
};



// UI Overlay HUD
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

        // 1. Active Event panel — two rows:
        //    Row 1: EVENT NAME (center)
        //    Row 2: [Xs timer]  ...  [Survived: X]
        // GJ_square02.png = textured background with highlighted border
        float panelW = 150.f; // slightly wider than 135
        float panelH = 45.f;  // slightly taller than 40
        float panelCX = panelW / 2.f + 6.f; // top-left corner
        float panelCY = winSize.height - panelH / 2.f - 6.f;

        m_eventBg = CCScale9Sprite::create("GJ_square02.png");
        m_eventBg->setContentSize(ccp(panelW, panelH));
        m_eventBg->setPosition(ccp(panelCX, panelCY));
        m_eventBg->setOpacity(255);
        m_eventBg->setColor(ccc3(255, 255, 255)); // Keep native bright borders!
        addChild(m_eventBg);

        // Row 1 (top): event name centered (narrower scale)
        m_eventLabel = CCLabelBMFont::create("NORMAL", "bigFont.fnt");
        m_eventLabel->setPosition(ccp(panelCX, panelCY + 9.f));
        m_eventLabel->setScale(0.34f);
        m_eventLabel->setColor(ccc3(255, 255, 255));
        addChild(m_eventLabel);

        // Row 2 LEFT: countdown timer
        m_timerLabel = CCLabelBMFont::create("10s", "bigFont.fnt");
        m_timerLabel->setAnchorPoint(ccp(0.f, 0.5f));     // left-anchored
        m_timerLabel->setPosition(ccp(panelCX - 67.f, panelCY - 11.f));
        m_timerLabel->setScale(0.29f);
        m_timerLabel->setColor(ccc3(0, 240, 255)); // Neon bright cyan-blue!
        addChild(m_timerLabel);

        // Row 2 RIGHT: survived counter
        m_counterLabel = CCLabelBMFont::create("Survived: 0", "bigFont.fnt");
        m_counterLabel->setAnchorPoint(ccp(1.f, 0.5f));   // right-anchored
        m_counterLabel->setPosition(ccp(panelCX + 67.f, panelCY - 11.f));
        m_counterLabel->setScale(0.26f);
        m_counterLabel->setColor(ccc3(255, 220, 0)); // Neon bright gold-yellow!
        addChild(m_counterLabel);

        // 2. Next Event preview warning (Bottom Center)
        m_nextBg = CCScale9Sprite::create("GJ_square01.png");
        m_nextBg->setContentSize(ccp(260, 36));
        m_nextBg->setPosition(ccp(winSize.width / 2, 45));
        m_nextBg->setOpacity(255);
        m_nextBg->setColor(ccc3(255, 255, 255)); // Keep native bright details!
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

// Map events to names & colors
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
        case ChaosEvent::SpeedhackFast: return ccc3(255, 128, 0);   // Orange
        case ChaosEvent::SpeedhackSlow: return ccc3(0, 160, 255);   // Blue
        case ChaosEvent::Mirror:        return ccc3(180, 0, 255);   // Purple
        case ChaosEvent::OldTV:         return ccc3(150, 150, 150); // Gray
        case ChaosEvent::Flashbang:     return ccc3(255, 255, 255); // White
        case ChaosEvent::Noclip:        return ccc3(255, 0, 200);   // Pink/Magenta
        case ChaosEvent::Normal:        return ccc3(255, 255, 255); // White
        case ChaosEvent::Spin:          return ccc3(100, 255, 140); // Lime
        case ChaosEvent::Invisible:     return ccc3(80, 0, 160);    // Dark purple
        default:                        return ccc3(255, 255, 255);
    }
}

// Helper — safe bool setting read (returns default if key not found)
static bool getEventSetting(const char* key) {
    auto* mod = geode::Mod::get();
    if (!mod) return true;
    return mod->getSettingValue<bool>(key);
}

// Selects a random event considering weight, duplicate bans and mod settings
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
        if (entry.first == current) continue;                     // prefer no immediate repeat
        filtered.push_back(entry);
        totalWeight += entry.second;
    }

    // If pool is empty after exclusion (e.g. only one event enabled), allow consecutive
    if (totalWeight <= 0) {
        filtered = pool;
        for (const auto& e : pool) totalWeight += e.second;
    }

    // If still empty (all events disabled in settings), return None
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

// ==========================================
// PLAYLAYER HOOK IMPLEMENTATION
// ==========================================
class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        ChaosEvent m_currentEvent = ChaosEvent::None;
        ChaosEvent m_nextEvent = ChaosEvent::None;
        float m_eventTimer = 0.f;
        int m_survivedEvents = 0;
        bool m_isFirstEvent = true;

        // Speedhack properties
        float m_targetTimeScale = 1.0f;

        // Noclip properties
        bool m_isNoclipActive = false;

        // Delay event timer until player starts moving
        bool m_playerHasMoved = false;

        // Spin: track whether m_hud was moved to the CCScene
        bool m_hudInScene = false;

        // Invisible event overlay
        CCLayerColor* m_invisLayer = nullptr;

        // UI nodes
        ChaosHUD* m_hud = nullptr;
        ColorVignette* m_vignette = nullptr;
        OldTVOverlay* m_oldTV = nullptr;
        VHSEffectNode* m_vhsNode = nullptr;
        CCLayerColor* m_flashLayer = nullptr;

        // Timing flag for Flashbang mid-flash
        bool m_flash5sTriggered = false;

        // Base FMOD music frequency (for speed events)
        float m_baseMusicFrequency = 0.f;

        // Custom state flags
        bool m_isFirstAttempt = true;
        float m_introDelayTimer = 0.f;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        // Instantiate HUD
        m_fields->m_hud = ChaosHUD::create();

        // Instantiate Vignette overlay
        m_fields->m_vignette = ColorVignette::create();

        // OldTV scanlines overlay — added to uiLayer so it covers full screen above game
        m_fields->m_oldTV = OldTVOverlay::create();
        m_fields->m_oldTV->setVisible(false);

        // Instantiate VHS Rewind overlay
        m_fields->m_vhsNode = VHSEffectNode::create();
        m_fields->m_vhsNode->setVisible(false);

        // Instantiate Flashbang overlay
        m_fields->m_flashLayer = CCLayerColor::create(ccc4(255, 255, 255, 0));

        // Invisible event overlay: near-black, covers game content but not HUD
        m_fields->m_invisLayer = CCLayerColor::create(ccc4(0, 0, 0, 0));

        if (m_uiLayer) {
            m_uiLayer->addChild(m_fields->m_invisLayer, 2);   // below scanlines (z=5) and HUD (z=200)
            m_uiLayer->addChild(m_fields->m_oldTV,   5);   // scanlines above game, below HUD
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

        // Variables setup
        m_fields->m_currentEvent = ChaosEvent::None;
        m_fields->m_nextEvent = ChaosEvent::None;
        m_fields->m_eventTimer = 0.f;
        m_fields->m_survivedEvents = 0;
        m_fields->m_isFirstEvent = true;
        m_fields->m_isFirstAttempt = true;
        m_fields->m_introDelayTimer = 0.f;

        ChaosState::s_isMirrorActive = false;
        ChaosState::s_naturalFlipped = false;
        ChaosState::s_isApplyingMirrorEvent = false;

        return true;
    }

    // Restore m_hud from CCScene back to m_uiLayer (called after Spin ends)
    void restoreHudToUILayer() {
        if (!m_fields->m_hudInScene) return;
        auto scene = CCDirector::sharedDirector()->getRunningScene();
        if (scene && m_fields->m_hud && m_uiLayer) {
            m_fields->m_hud->retain();
            scene->removeChild(m_fields->m_hud, false);
            m_uiLayer->addChild(m_fields->m_hud, 200);
            m_fields->m_hud->release();
        }
        m_fields->m_hudInScene = false;
    }

    // Set opacity on all currently loaded objects (equivalent to GD Alpha trigger, Opacity: 0.1)
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

        // 3. CONTINUOUS ACTIVE EVENT OVERRIDE LOGIC
        if (m_fields->m_currentEvent != ChaosEvent::None) {
            m_fields->m_eventTimer += dt;

            // Speedhack Event Speed Enforcement
            if (m_fields->m_currentEvent == ChaosEvent::SpeedhackFast) {
                CCDirector::sharedDirector()->getScheduler()->setTimeScale(1.5f);
            } else if (m_fields->m_currentEvent == ChaosEvent::SpeedhackSlow) {
                CCDirector::sharedDirector()->getScheduler()->setTimeScale(0.75f);
            }

                    // Invisible: re-apply 5% opacity every frame so GD's color system can't reset it
            if (m_fields->m_currentEvent == ChaosEvent::Invisible) {
                applyLevelOpacity(13);
            }

            // OldTV: enforce 100% grayscale every frame so GD's own shaders can't turn it off
            if (m_fields->m_currentEvent == ChaosEvent::OldTV) {
                if (m_shaderLayer) {
                    m_shaderLayer->triggerGrayscale(0.f, 1.f, false, 0, 0, 0.f);
                }
            }





            // Flashbang 5.0s second flash (longer)
            if (m_fields->m_currentEvent == ChaosEvent::Flashbang && m_fields->m_eventTimer >= 5.0f && !m_fields->m_flash5sTriggered) {
                m_fields->m_flash5sTriggered = true;
                m_fields->m_flashLayer->stopAllActions();
                m_fields->m_flashLayer->setOpacity(255);
                m_fields->m_flashLayer->runAction(CCSequence::create(
                    CCDelayTime::create(0.5f),
                    CCFadeOut::create(1.3f),
                    nullptr
                ));
                FMODAudioEngine::sharedEngine()->playEffect("unlockedPath.ogg");
            }

            // Update countdown timer in HUD
            if (m_fields->m_hud) {
                m_fields->m_hud->updateTimer(10.f - m_fields->m_eventTimer);
            }

            // 4. NEXT EVENT WARNING PREVIEW AT 8.5s
            if (m_fields->m_eventTimer >= 8.5f && m_fields->m_nextEvent == ChaosEvent::None) {
                m_fields->m_nextEvent = getWeightedRandomEvent(m_fields->m_currentEvent, false);
                if (m_fields->m_nextEvent != ChaosEvent::None) {
                    m_fields->m_hud->showNextPreview(getEventName(m_fields->m_nextEvent));
                    FMODAudioEngine::sharedEngine()->playEffect("clickBtn.ogg");
                } else {
                    // All events disabled — mark as handled
                    m_fields->m_nextEvent = ChaosEvent::None;
                }
            }

            // 5. EVENT CYCLE OVER / ROTATION AT 10.0s
            if (m_fields->m_eventTimer >= 10.0f) {
                // Flashbang final blinding flash (overlaps onto next event, 3s)
                if (m_fields->m_currentEvent == ChaosEvent::Flashbang) {
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

                m_fields->m_survivedEvents++;
                m_fields->m_hud->updateCounter(m_fields->m_survivedEvents);

                // Start the next event (or stay NORMAL if all events are disabled)
                auto nextEv = m_fields->m_nextEvent;
                m_fields->m_nextEvent = ChaosEvent::None;
                if (nextEv != ChaosEvent::None) {
                    activateEvent(nextEv);
                } else {
                    m_fields->m_currentEvent = ChaosEvent::None;
                    m_fields->m_eventTimer = 0.f;
                }
            }

        } else {
            // Level start timeline — wait until player starts moving
            if (!m_fields->m_playerHasMoved) {
                if (m_fields->m_isFirstAttempt) {
                    m_fields->m_introDelayTimer += dt;
                    if (m_fields->m_introDelayTimer < 1.0f) {
                        return;
                    }
                }
                if (m_player1 && !m_player1->m_isDead && m_player1->getCurrentXVelocity() > 0.01) {
                    m_fields->m_playerHasMoved = true;
                }
                return;
            }

            m_fields->m_eventTimer += dt;

            if (m_fields->m_hud)
                m_fields->m_hud->updateTimer(10.f - m_fields->m_eventTimer);

            // 8.5s preview — first-time pick (isFirst=true excludes Reverse)
            if (m_fields->m_eventTimer >= 8.5f && m_fields->m_nextEvent == ChaosEvent::None) {
                m_fields->m_nextEvent = getWeightedRandomEvent(ChaosEvent::None, true);
                if (m_fields->m_nextEvent != ChaosEvent::None) {
                    m_fields->m_hud->showNextPreview(getEventName(m_fields->m_nextEvent));
                    FMODAudioEngine::sharedEngine()->playEffect("clickBtn.ogg");
                }
            }

            if (m_fields->m_eventTimer >= 10.0f) {
                auto firstEv = m_fields->m_nextEvent;
                m_fields->m_nextEvent = ChaosEvent::None;
                m_fields->m_isFirstEvent = false;
                if (firstEv != ChaosEvent::None) {
                    activateEvent(firstEv);
                } else {
                    m_fields->m_eventTimer = 0.f; // no events enabled, reset
                }
            }
        }
    }

    // Level resets/spawns (death handling)
    void resetLevel() {
        ChaosState::s_isMirrorActive = false;
        ChaosState::s_naturalFlipped = false;
        ChaosState::s_isApplyingMirrorEvent = false;

        PlayLayer::resetLevel();

        // Wipe all dynamic HUD and active overlays
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

        // Restore player opacity, scaling and modes
        if (m_player1) {
            m_player1->stopActionByTag(7777); // stop blinking
            setPlayerOpacity(m_player1, 255);
            m_player1->removeChildByTag(8888); // clear ice crust
        }
        if (m_player2) {
            m_player2->stopActionByTag(7777);
            setPlayerOpacity(m_player2, 255);
            m_player2->removeChildByTag(8888);
        }

        // Restore native mirror and grayscale on death/reset
        toggleFlipped(false, true);
        if (m_shaderLayer) m_shaderLayer->triggerGrayscale(0.f, 0.f, false, 0, 0, 0.f);

        // Reset Spin (restore camera rotation to normal)
        this->updateScreenRotation(0.f, false, false, 0.f, 0, 0.f, 0, 0);

        // Restore player visibility, streak nodes and level opacity (Invisible event)
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

        // Resume actions/schedulers on all level objects upon death/restart
        if (m_objects) {
            for (int i = 0; i < m_objects->count(); ++i) {
                auto node = static_cast<CCNode*>(m_objects->objectAtIndex(i));
                if (node) node->resumeSchedulerAndActions();
            }
        }

        // Restore music speed if Speedhack was active
        if (auto ch = FMODAudioEngine::sharedEngine()->getActiveMusicChannel(0)) {
            if (m_fields->m_baseMusicFrequency > 0.f)
                ch->setFrequency(m_fields->m_baseMusicFrequency);
        }
        m_fields->m_baseMusicFrequency = 0.f;

        // Clean portals crossed-out signs
        removeCrossedOutPortals(m_objects);

        // Reset speedhack timescales
        CCDirector::sharedDirector()->getScheduler()->setTimeScale(1.0f);

        // Clear global and fields states
        ChaosState::s_isOldTVActive  = false;

        m_fields->m_currentEvent = ChaosEvent::None;
        m_fields->m_nextEvent = ChaosEvent::None;
        m_fields->m_eventTimer = 0.f;
        m_fields->m_survivedEvents = 0;
        m_fields->m_isFirstEvent = true;
        m_fields->m_playerHasMoved = false;
        m_fields->m_isFirstAttempt = false;
        m_fields->m_introDelayTimer = 0.f;
    }

    // Noclip invulnerability bypass
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (m_fields->m_isNoclipActive) {
            return;
        }
        PlayLayer::destroyPlayer(player, object);
    }

    // Helper functions within PlayLayer
    void deactivateCurrentEvent() {
        auto ev = m_fields->m_currentEvent;
        if (ev == ChaosEvent::None) return;

        m_fields->m_vignette->setVisible(false);

        switch (ev) {
            case ChaosEvent::SpeedhackFast:
            case ChaosEvent::SpeedhackSlow: {
                CCDirector::sharedDirector()->getScheduler()->setTimeScale(1.0f);
                m_fields->m_targetTimeScale = 1.0f;
                // Restore music speed
                if (auto ch = FMODAudioEngine::sharedEngine()->getActiveMusicChannel(0)) {
                    if (m_fields->m_baseMusicFrequency > 0.f)
                        ch->setFrequency(m_fields->m_baseMusicFrequency);
                }
                m_fields->m_baseMusicFrequency = 0.f;
                break;
            }
            case ChaosEvent::Mirror: {
                ChaosState::s_isMirrorActive = false;
                ChaosState::s_isApplyingMirrorEvent = true;
                toggleFlipped(ChaosState::s_naturalFlipped, true); // return to natural state
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
                break; // nothing to undo
            }
            case ChaosEvent::Invisible: {
                // Restore player visibility, streak nodes and level opacity (set to 255)
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
                m_fields->m_isNoclipActive = false;
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

        // Deactivation sound (not for Flashbang as it overlaps)
        if (ev != ChaosEvent::Flashbang) {
            FMODAudioEngine::sharedEngine()->playEffect("backBtn.ogg");
        }
    }

    void activateEvent(ChaosEvent ev) {
        m_fields->m_currentEvent = ev;
        m_fields->m_eventTimer = 0.f;
        m_fields->m_flash5sTriggered = false;

        // Sync visual indicator and next event HUDs
        m_fields->m_hud->updateEvent(getEventName(ev), getEventColor(ev));
        m_fields->m_hud->hideNextPreview();

        // Enable default edge vignettes
        m_fields->m_vignette->setColor(getEventColor(ev));
        m_fields->m_vignette->setVisible(true);

        switch (ev) {
            case ChaosEvent::SpeedhackFast: {
                m_fields->m_targetTimeScale = 1.5f;
                CCDirector::sharedDirector()->getScheduler()->setTimeScale(1.5f);
                // Speed up music
                if (auto ch = FMODAudioEngine::sharedEngine()->getActiveMusicChannel(0)) {
                    if (m_fields->m_baseMusicFrequency == 0.f)
                        ch->getFrequency(&m_fields->m_baseMusicFrequency);
                    ch->setFrequency(m_fields->m_baseMusicFrequency * 1.5f);
                }
                FMODAudioEngine::sharedEngine()->playEffect("achievement.ogg");
                break;
            }
            case ChaosEvent::SpeedhackSlow: {
                m_fields->m_targetTimeScale = 0.75f;
                CCDirector::sharedDirector()->getScheduler()->setTimeScale(0.75f);
                // Slow down music to 0.75x
                if (auto ch = FMODAudioEngine::sharedEngine()->getActiveMusicChannel(0)) {
                    if (m_fields->m_baseMusicFrequency == 0.f)
                        ch->getFrequency(&m_fields->m_baseMusicFrequency);
                    ch->setFrequency(m_fields->m_baseMusicFrequency * 0.75f);
                }
                FMODAudioEngine::sharedEngine()->playEffect("achievement.ogg");
                break;
            }
            case ChaosEvent::Mirror: {
                ChaosState::s_isMirrorActive = true;
                ChaosState::s_isApplyingMirrorEvent = true;
                toggleFlipped(!ChaosState::s_naturalFlipped, true); // apply XOR visual flipped state
                ChaosState::s_isApplyingMirrorEvent = false;
                FMODAudioEngine::sharedEngine()->playEffect("achievement.ogg");
                break;
            }
            case ChaosEvent::OldTV: {
                // Show scanlines overlay
                m_fields->m_oldTV->setVisible(true);
                // Hide vignette (no colour border for B&W event)
                m_fields->m_vignette->setVisible(false);
                ChaosState::s_isOldTVActive = true;
                // Use GD's native grayscale shader trigger: fadeTime=0, target=1.0 (full B&W)
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
                // No effects — just normal gameplay
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
                // Hide player (opacity=0 kills the motion trail at its source)
                if (m_player1) {
                    m_player1->setVisible(false);
                    m_player1->setOpacity(0);
                    auto ch = m_player1->getChildren();
                    if (ch) for (int i = 0; i < ch->count(); i++)
                        static_cast<CCNode*>(ch->objectAtIndex(i))->setVisible(false);
                }
                // Hide streak nodes that are siblings of batch nodes in objectLayer
                // (CCMotionStreak / particle nodes — NOT CCSpriteBatchNode)
                if (m_objectLayer) {
                    auto ch = m_objectLayer->getChildren();
                    if (ch) for (int i = 0; i < ch->count(); i++) {
                        auto node = static_cast<CCNode*>(ch->objectAtIndex(i));
                        if (!dynamic_cast<CCSpriteBatchNode*>(node))
                            node->setVisible(false);
                    }
                }
                // Fade all game objects to 5% opacity (Opacity: 0.05)
                applyLevelOpacity(13);
                FMODAudioEngine::sharedEngine()->playEffect("achievement.ogg");
                break;
            }
            case ChaosEvent::Noclip: {
                m_fields->m_isNoclipActive = true;
                
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

// ==========================================
// GJBASEGAMELAYER HOOK IMPLEMENTATION
// ==========================================
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
