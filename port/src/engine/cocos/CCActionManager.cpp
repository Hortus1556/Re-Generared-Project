// cocos2d-x 2.1.1's CCActionManager.cpp, ported. MIT: LICENSE-cocos2d-x.txt.
#include "engine/cocos/CCActionManager.h"

#include <algorithm>

#include "engine/cocos/CCAction.h"

namespace cocos2d {

CCActionManager::~CCActionManager() { removeAllActions(); }

CCActionManager::Element* CCActionManager::elementOf(const CCObject* target) {
    for (Element* e : m_targets)
        if (e->target == target) return e;
    return nullptr;
}

void CCActionManager::deleteElement(Element* e) {
    auto it = std::find(m_targets.begin(), m_targets.end(), e);
    if (it == m_targets.end()) return;
    // Fixed: an element at or before the one stepping in update moves the
    // rest down; without this the target after it lost a step (an action
    // stopping an earlier target's actions: a CCCallFuncN removing a node)
    if (static_cast<int>(it - m_targets.begin()) <= m_nCurrentTargetIndex) --m_nCurrentTargetIndex;
    m_targets.erase(it);
    for (CCAction* a : e->actions) a->release();
    e->target->release();
    delete e;
}

void CCActionManager::pauseTarget(CCObject* target) {
    if (Element* e = elementOf(target)) e->paused = true;
}

void CCActionManager::resumeTarget(CCObject* target) {
    if (Element* e = elementOf(target)) e->paused = false;
}

void CCActionManager::addAction(CCAction* action, CCNode* target, bool paused) {
    if (!action || !target) return;
    Element* e = elementOf(target);
    if (!e) {
        e = new Element;
        e->target = target;
        e->paused = paused;
        target->retain();
        m_targets.push_back(e);
    }
    action->retain();
    e->actions.push_back(action);
    action->startWithTarget(target);
}

void CCActionManager::removeAllActions() {
    std::vector<Element*> all = m_targets;
    for (Element* e : all) removeAllActionsFromTarget(e->target);
}

void CCActionManager::removeAllActionsFromTarget(CCObject* target) {
    if (!target) return;
    Element* e = elementOf(target);
    if (!e) return;
    if (std::find(e->actions.begin(), e->actions.end(), e->currentAction) != e->actions.end() &&
        !e->currentActionSalvaged) {
        e->currentAction->retain();  // it is stepping: released when the step is done
        e->currentActionSalvaged = true;
    }
    for (CCAction* a : e->actions) a->release();
    e->actions.clear();
    if (m_pCurrentTarget == e) m_bCurrentTargetSalvaged = true;
    else deleteElement(e);
}

void CCActionManager::removeAction(CCAction* action) {
    if (!action) return;
    Element* e = elementOf(action->getOriginalTarget());
    if (!e) return;
    auto it = std::find(e->actions.begin(), e->actions.end(), action);
    if (it != e->actions.end()) removeActionAtIndex(static_cast<unsigned int>(it - e->actions.begin()), e);
}

void CCActionManager::removeActionAtIndex(unsigned int index, Element* e) {
    CCAction* action = e->actions[index];
    if (action == e->currentAction && !e->currentActionSalvaged) {
        e->currentAction->retain();
        e->currentActionSalvaged = true;
    }
    e->actions.erase(e->actions.begin() + index);
    action->release();
    // the index of what is stepping moves down with it
    if (e->actionIndex >= static_cast<int>(index)) e->actionIndex--;
    if (e->actions.empty()) {
        if (m_pCurrentTarget == e) m_bCurrentTargetSalvaged = true;
        else deleteElement(e);
    }
}

void CCActionManager::removeActionByTag(unsigned int tag, CCObject* target) {
    Element* e = elementOf(target);
    if (!e) return;
    for (size_t i = 0; i < e->actions.size(); ++i)
        if (e->actions[i]->getTag() == (int)tag && e->actions[i]->getOriginalTarget() == target) {
            removeActionAtIndex(static_cast<unsigned int>(i), e);
            break;
        }
}

CCAction* CCActionManager::getActionByTag(unsigned int tag, CCObject* target) {
    if (Element* e = elementOf(target))
        for (CCAction* a : e->actions)
            if (a->getTag() == (int)tag) return a;
    return nullptr;
}

unsigned int CCActionManager::numberOfRunningActionsInTarget(CCObject* target) {
    Element* e = elementOf(target);
    return e ? static_cast<unsigned int>(e->actions.size()) : 0;
}

void CCActionManager::update(float dt) {
    for (m_nCurrentTargetIndex = 0; m_nCurrentTargetIndex < static_cast<int>(m_targets.size());
         ++m_nCurrentTargetIndex) {
        Element* e = m_targets[m_nCurrentTargetIndex];
        m_pCurrentTarget = e;
        m_bCurrentTargetSalvaged = false;
        if (!e->paused) {
            // the actions may change while they run
            for (e->actionIndex = 0; e->actionIndex < static_cast<int>(e->actions.size()); ++e->actionIndex) {
                e->currentAction = e->actions[e->actionIndex];
                if (!e->currentAction) continue;
                e->currentActionSalvaged = false;
                e->currentAction->step(dt);
                if (e->currentActionSalvaged) {
                    // it was removed while stepping: let it go now
                    e->currentAction->release();
                } else if (e->currentAction->isDone()) {
                    e->currentAction->stop();
                    CCAction* action = e->currentAction;
                    // removeAction must not find it current (2.1.1 clears it first)
                    e->currentAction = nullptr;
                    removeAction(action);
                }
                e->currentAction = nullptr;
            }
        }
        m_pCurrentTarget = nullptr;
        // the element goes once no actions were added to it during the step
        if (m_bCurrentTargetSalvaged && e->actions.empty()) deleteElement(e);
    }
    m_nCurrentTargetIndex = -1;
}

}  // namespace cocos2d
