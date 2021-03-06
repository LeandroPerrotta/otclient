-- @docclass
UIMiniWindow = extends(UIWindow)

function UIMiniWindow.create()
  local miniwindow = UIMiniWindow.internalCreate()
  return miniwindow
end

function UIMiniWindow:getClassName()
  return 'UIMiniWindow'
end

function UIMiniWindow:open(dontSave)
  self:setVisible(true)

  if not dontSave then
    self:setSettings({closed = false})
  end

  signalcall(self.onOpen, self)
end

function UIMiniWindow:close(dontSave)
  self:setVisible(false)

  if not dontSave then
    self:setSettings({closed = true})
  end

  signalcall(self.onClose, self)
end

function UIMiniWindow:minimize(dontSave)
  self:setOn(true)
  self:getChildById('contentsPanel'):hide()
  self:getChildById('miniwindowScrollBar'):hide()
  self:getChildById('bottomResizeBorder'):hide()
  self:getChildById('minimizeButton'):setOn(true)
  self.savedHeight = self:getHeight()
  self:setHeight(self.minimizedHeight)

  if not dontSave then
    self:setSettings({minimized = true})
  end

  signalcall(self.onMinimize, self)
end

function UIMiniWindow:maximize(dontSave)
  self:setOn(false)
  self:getChildById('contentsPanel'):show()
  self:getChildById('miniwindowScrollBar'):show()
  self:getChildById('bottomResizeBorder'):show()
  self:getChildById('minimizeButton'):setOn(false)
  self:setHeight(self.savedHeight)

  if not dontSave then
    self:setSettings({minimized = false})
  end

  signalcall(self.onMaximize, self)
end

function UIMiniWindow:onSetup()
  self:getChildById('closeButton').onClick =
    function()
      self:close()
    end

  self:getChildById('minimizeButton').onClick =
    function()
      if self:isOn() then
        self:maximize()
      else
        self:minimize()
      end
    end

  local oldParent = self:getParent()

  local settings = g_settings.getNode('MiniWindows')
  if settings then
    local selfSettings = settings[self:getId()]
    if selfSettings then
      if selfSettings.parentId then
        local parent = rootWidget:recursiveGetChildById(selfSettings.parentId)
        if parent then
          if parent:getClassName() == 'UIMiniWindowContainer' and selfSettings.index and parent:isOn() then
            self.miniIndex = selfSettings.index
            parent:scheduleInsert(self, selfSettings.index)
          elseif selfSettings.position then
            self:setParent(parent)
            self:setPosition(topoint(selfSettings.position))
            self:bindRectToParent()
          end
        end
      end

      if selfSettings.minimized then
        self:minimize(true)
      end

      if selfSettings.closed then
        self:close(true)
      end
    end
  end

  local newParent = self:getParent()

  self.miniLoaded = true

  if oldParent and oldParent:getClassName() == 'UIMiniWindowContainer' then
    oldParent:order()
  end
  if newParent and newParent:getClassName() == 'UIMiniWindowContainer' and newParent ~= oldParent then
    newParent:order()
  end
end

function UIMiniWindow:onDragEnter(mousePos)
  local parent = self:getParent()
  if not parent then return false end

  if parent:getClassName() == 'UIMiniWindowContainer' then
    local containerParent = parent:getParent()
    parent:removeChild(self)
    containerParent:addChild(self)
    parent:saveChildren()
  end

  local oldPos = self:getPosition()
  self.movingReference = { x = mousePos.x - oldPos.x, y = mousePos.y - oldPos.y }
  self:setPosition(oldPos)
  self.free = true
  return true
end

function UIMiniWindow:onDragMove(mousePos, mouseMoved)
  local oldMousePosY = mousePos.y - mouseMoved.y
  local children = rootWidget:recursiveGetChildrenByMarginPos(mousePos)
  local overAnyWidget = false
  for i=1,#children do
    local child = children[i]
    if child:getParent():getClassName() == 'UIMiniWindowContainer' then
      overAnyWidget = true

      local childCenterY = child:getY() + child:getHeight() / 2
      if child == self.movedWidget and mousePos.y < childCenterY and oldMousePosY < childCenterY then
        break
      end

      if self.movedWidget then
        self.setMovedChildMargin(0)
        self.setMovedChildMargin = nil
      end

      if mousePos.y < childCenterY then
        self.setMovedChildMargin = function(v) child:setMarginTop(v) end
        self.movedIndex = 0
      else
        self.setMovedChildMargin = function(v) child:setMarginBottom(v) end
        self.movedIndex = 1
      end

      self.movedWidget = child
      self.setMovedChildMargin(self:getHeight())
      break
    end
  end

  if not overAnyWidget and self.movedWidget then
    self.setMovedChildMargin(0)
    self.movedWidget = nil
  end

  return UIWindow.onDragMove(self, mousePos, mouseMoved)
end

function UIMiniWindow:onMousePress()
  local parent = self:getParent()
  if not parent then return false end
  if parent:getClassName() ~= 'UIMiniWindowContainer' then
    self:raise()
    return true
  end
end

function UIMiniWindow:onDragLeave(droppedWidget, mousePos)
  if self.movedWidget then
    self.setMovedChildMargin(0)
    self.movedWidget = nil
    self.setMovedChildMargin = nil
    self.movedIndex = nil
  end

  local parent = self:getParent()
  if parent then
    if parent:getClassName() == 'UIMiniWindowContainer' then
      parent:saveChildren()
    else
      self:saveParentPosition(parent:getId(), self:getPosition())
    end
  end
end

function UIMiniWindow:onFocusChange(focused)
  -- miniwindows only raises when its outside MiniWindowContainers
  if not focused then return end
  local parent = self:getParent()
  if parent and parent:getClassName() ~= 'UIMiniWindowContainer' then
    self:raise()
  end
end

function UIMiniWindow:setSettings(data)
  if not self.save then return end

  local settings = g_settings.getNode('MiniWindows')
  if not settings then
    settings = {}
  end

  local id = self:getId()
  if not settings[id] then
    settings[id] = {}
  end

  for key,value in pairs(data) do
    settings[id][key] = value
  end

  g_settings.setNode('MiniWindows', settings)
end

function UIMiniWindow:saveParentPosition(parentId, position)
  local selfSettings = {}
  selfSettings.parentId = parentId
  selfSettings.position = pointtostring(position)
  self:setSettings(selfSettings)
end

function UIMiniWindow:saveParentIndex(parentId, index)
  local selfSettings = {}
  selfSettings.parentId = parentId
  selfSettings.index = index
  self:setSettings(selfSettings)
end
