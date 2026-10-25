#include "as3/Scene.h"

#include "engine/cocos/CCDirector.h"
#include "platform/Platform.h"

namespace mt {

Scene::Scene() {}

Scene::~Scene() {}

void Scene::onShown() { m_shownAt = platform::passTime(); }

bool Scene::shownForLessThanMinimum() { return platform::passTime() - m_shownAt < m_minimumShown; }

bool Scene::isTouchRegistered() {
    return cocos2d::CCDirector::sharedDirector()->getTouchDispatcher()->findHandler(this) != nullptr;
}

void Scene::setTouchRegistered(bool on) {
    cocos2d::CCTouchDispatcher* d = cocos2d::CCDirector::sharedDirector()->getTouchDispatcher();
    if (on) d->addTargetedDelegate(this, 0, true);
    else d->removeDelegate(this);
}

}  // namespace mt
