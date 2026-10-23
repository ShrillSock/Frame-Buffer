#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace {
    struct Pending {
        bool active = false;
        int framesLeft = 0;
        bool releaseQueued = false;  // player let go while still buffered
        bool releaseNextStep = false; // fired already, release on next step
    };
    Pending g_p1, g_p2;
    bool g_replaying = false;

    int bufferFrames() {
        return std::stoi(Mod::get()->getSettingValue<std::string>("buffer-frames"));
    }
    bool isEnabled() { return Mod::get()->getSettingValue<bool>("enabled"); }

    // Is a jump click "valid" right now?
    bool canActNow(PlayerObject* p) {
        if (!p) return false;
        // hold-based modes don't need buffering
        if (p->m_isShip || p->m_isBird || p->m_isDart || p->m_isSwing) return true;
        if (p->m_isOnGround) return true;
        if (p->m_touchingRings && p->m_touchingRings->count() > 0) return true;
        return false;
    }
}

class $modify(BufferGJBGL, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        if (g_replaying || !isEnabled() || button != 1) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        auto& pend = isPlayer1 ? g_p1 : g_p2;
        auto* player = isPlayer1 ? m_player1 : m_player2;

        if (down) {
            if (canActNow(player)) {
                return GJBaseGameLayer::handleButton(down, button, isPlayer1);
            }
            // not valid yet: hold the click back
            pend.active = true;
            pend.framesLeft = bufferFrames();
            pend.releaseQueued = false;
            return;
        }

        // release
        if (pend.active) {
            pend.releaseQueued = true; // deliver after the buffered press fires
            return;
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt) {
        if (isEnabled()) {
            step(g_p1, true);
            step(g_p2, false);
        }
        GJBaseGameLayer::processCommands(dt);
    }

    void step(Pending& pend, bool isPlayer1) {
        auto* player = isPlayer1 ? m_player1 : m_player2;

        if (pend.releaseNextStep) {
            pend.releaseNextStep = false;
            g_replaying = true;
            GJBaseGameLayer::handleButton(false, 1, isPlayer1);
            g_replaying = false;
            return;
        }
        if (!pend.active) return;

        if (canActNow(player)) {
            g_replaying = true;
            GJBaseGameLayer::handleButton(true, 1, isPlayer1);
            g_replaying = false;
            pend.active = false;
            pend.releaseNextStep = pend.releaseQueued;
            pend.releaseQueued = false;
        } else if (--pend.framesLeft <= 0) {
            pend = Pending{}; // window expired, drop the click
        }
    }
};

class $modify(PlayLayer) {
    void resetLevel() {
        g_p1 = Pending{};
        g_p2 = Pending{};
        PlayLayer::resetLevel();
    }
};