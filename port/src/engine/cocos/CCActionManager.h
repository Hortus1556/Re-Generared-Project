// cocos2d-x 2.1.1's CCActionManager, ported to Bleed Engine.
// MIT: LICENSE-cocos2d-x.txt.
// Steps every running action each frame (scheduled at the system's priority,
// before the games' updates), target by target in the order they were first
// given an action; a finished action is stopped and removed.
#pragma once

#include <vector>

#include "engine/cocos/CCObject.h"

namespace cocos2d {

class CCAction;
class CCNode;

class CCActionManager : public CCObject {
public:
    ~CCActionManager() override;
    void addAction(CCAction* action, CCNode* target, bool paused);
    void removeAllActions();
    void removeAllActionsFromTarget(CCObject* target);
    void removeAction(CCAction* action);
    void removeActionByTag(unsigned int tag, CCObject* target);
    CCAction* getActionByTag(unsigned int tag, CCObject* target);
    unsigned int numberOfRunningActionsInTarget(CCObject* target);
    void pauseTarget(CCObject* target);
    void resumeTarget(CCObject* target);
    void update(float dt) override;

private:
    struct Element {
        CCObject* target;  // retained
        std::vector<CCAction*> actions;  // retained
        int actionIndex = 0;
        CCAction* currentAction = nullptr;
        bool currentActionSalvaged = false;
        bool paused = false;
    };
    Element* elementOf(const CCObject* target);
    void removeActionAtIndex(unsigned int index, Element* e);
    void deleteElement(Element* e);
    std::vector<Element*> m_targets;
    Element* m_pCurrentTarget = nullptr;
    // the element stepping in update: moved back when an element at or
    // before it is deleted (2.1.1 walked a linked hash, where that skipped
    // none); -1 outside
    int m_nCurrentTargetIndex = -1;
    bool m_bCurrentTargetSalvaged = false;
};

}  // namespace cocos2d
