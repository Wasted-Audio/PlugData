class ValueTreeViewerComponent;
class ValueTreeNodeComponent;
struct ValueTreeOwnerView : public Component
{
    SafePointer<ValueTreeNodeComponent> selectedNode;
    
    std::function<void()> updateView = [](){};
    std::function<void(ValueTree&)> onClick = [](ValueTree&){};
    std::function<void(ValueTree&)> onSelect = [](ValueTree&){};
    std::function<void(ValueTree&)> onDragStart = [](ValueTree&){};
};

#include "ValueTreeNodeBranchLine.h"
#include "Fonts.h"
#include "../Components/BouncingViewport.h"

class ValueTreeNodeComponent : public Component
{
public:
    ValueTreeNodeComponent(const ValueTree& node, ValueTreeNodeComponent* parentNode, String const& prepend = String()) : valueTreeNode(node), parent(parentNode)
    {
        nodeBranchLine = std::make_unique<ValueTreeNodeBranchLine>(this);
        addAndMakeVisible(nodeBranchLine.get());
        nodeBranchLine->setAlwaysOnTop(true);
        
        if(valueTreeNode.hasProperty("Name")) {
            auto tooltipPrepend = prepend;
            if (tooltipPrepend.isEmpty())
                tooltipPrepend = "(Parent)";
            nodeBranchLine->setTooltip(tooltipPrepend + " " + valueTreeNode.getProperty("Name").toString());
        }

        // Create subcomponents for each child node
        for (int i = 0; i < valueTreeNode.getNumChildren(); ++i)
        {
            auto* childComponent = nodes.add(new ValueTreeNodeComponent(valueTreeNode.getChild(i), this, prepend));
            addAndMakeVisible(childComponent);
        }
    }

    void update()
    {
        // Compare existing child nodes with current children
        for (int i = 0; i < valueTreeNode.getNumChildren(); ++i)
        {
            ValueTree childNode = valueTreeNode.getChild(i);
            
            // Check if an existing node exists for this child
            ValueTreeNodeComponent* existingNode = nullptr;
            for (auto* node : nodes)
            {
                if (ValueTreeNodeComponent::compareProperties(childNode, node->valueTreeNode))
                {
                    existingNode = node;
                    node->valueTreeNode = childNode;
                    break;
                }
            }
            
            // If an existing node is found, update it; otherwise, create a new node
            if (existingNode != nullptr)
            {
                existingNode->update();
            }
            else
            {
                auto* childComponent = new ValueTreeNodeComponent(childNode, this);
                nodes.add(childComponent);
                addAndMakeVisible(childComponent);
            }
        }

        // Remove any existing nodes that no longer exist in the current children
        for (int i = nodes.size(); --i >= 0;)
        {
            if(!nodes.getUnchecked(i)->valueTreeNode.isAChildOf(valueTreeNode))
            {
                nodes.remove(i);
            }
        }
    }

    void paintOpenCloseButton(Graphics& g, Rectangle<float> const& area)
    {
        auto arrowArea = area.reduced(5, 9).translated(4, 0).toFloat();
        Path p;
        
        p.startNewSubPath(0.0f, 0.0f);
        p.lineTo(0.5f, 0.5f);
        p.lineTo(isOpen() ? 1.0f : 0.0f, isOpen() ? 0.0f : 1.0f);
        
        g.setColour(isSelected() ? findColour(PlugDataColour::sidebarActiveTextColourId) : getOwnerView()->findColour(PlugDataColour::sidebarTextColourId));
        g.strokePath(p, PathStrokeType(1.5f, PathStrokeType::curved, PathStrokeType::rounded), p.getTransformToScaleToFit(arrowArea, true));
    }
    
    bool isSelected()
    {
        return getOwnerView()->selectedNode.getComponent() == this;
    }
    
    ValueTreeOwnerView* getOwnerView()
    {
        return findParentComponentOfClass<ValueTreeOwnerView>();
    }
    
    void mouseDrag(const MouseEvent& e) override
    {
        if(!isDragging && e.getDistanceFromDragStart() > 10)
        {
            isDragging = true;
            getOwnerView()->onDragStart(valueTreeNode);
        }
    }
    
    bool isOpen() const
    {
        return isOpened || isOpenedBySearch;
    }
    
    void mouseUp(const MouseEvent& e) override
    {
        isDragging = false;
        
        if (e.eventComponent == this && e.mods.isLeftButtonDown())
        {
            if(nodes.size() && e.x < 22) {
                closeNode();
            }
            else {
                getOwnerView()->selectedNode = this;
                getOwnerView()->repaint();
                
                if(e.getNumberOfClicks() == 1)
                {
                    getOwnerView()->onSelect(valueTreeNode);
                }
                else {
                    getOwnerView()->onClick(valueTreeNode);
                }
                
            }
        }
    }

    void closeNode()
    {
        isOpened = !isOpened;
        for (auto* child : nodes)
            child->setVisible(isOpen());

        getOwnerView()->updateView();
        resized();
    }
    
    void paint(Graphics& g) override
    {
        if(getOwnerView()->selectedNode.getComponent() == this) {
            g.setColour(findColour(PlugDataColour::sidebarActiveBackgroundColourId));
            g.fillRoundedRectangle(getLocalBounds().withHeight(25).reduced(2).toFloat(), Corners::defaultCornerRadius);
        }
        
        auto hasChildren = !nodes.isEmpty();
        auto itemBounds = getLocalBounds().removeFromTop(25);
        auto arrowBounds = itemBounds.removeFromLeft(20).toFloat().reduced(1.0f);
        if (isOpen()) {
            arrowBounds = arrowBounds.reduced(1.0f);
        }
        
        if(hasChildren)
        {
            paintOpenCloseButton(g, arrowBounds);
        }
        
        auto& owner = *getOwnerView();
        auto colour = isSelected() ? owner.findColour(PlugDataColour::sidebarActiveTextColourId) : getOwnerView()->findColour(PlugDataColour::sidebarTextColourId);
        
        if(valueTreeNode.hasProperty("Icon"))
        {
            Fonts::drawIcon(g, valueTreeNode.getProperty("Icon"), itemBounds.removeFromLeft(22).reduced(2), colour, 12, false);
        }
        if(valueTreeNode.hasProperty("RightText"))
        {
            auto text = valueTreeNode.getProperty("Name").toString();
            auto rightText = valueTreeNode.getProperty("RightText").toString();
            if(Font(15).getStringWidth(text + rightText) < itemBounds.getWidth() - 16) {
                Fonts::drawFittedText(g, valueTreeNode.getProperty("RightText"), itemBounds.removeFromRight(Font(15).getStringWidth(rightText) + 4), colour.withAlpha(0.5f));
            }
        }
        
        Fonts::drawFittedText(g, valueTreeNode.getProperty("Name"), itemBounds, colour);
    }

    void resized() override
    {
        // Set the bounds of the subcomponents within the current component
        if(isOpen()) {
            nodeBranchLine->setVisible(true);
            nodeBranchLine->setBounds(getLocalBounds().withLeft(10).withRight(18).withTrimmedBottom(10).withTop(20));

            auto bounds = getLocalBounds().withTrimmedLeft(8).withTrimmedTop(25);

            for (auto* node : nodes)
            {
                if(node->isVisible()) {
                    auto childBounds = bounds.removeFromTop(node->getTotalContentHeight());
                    node->setBounds(childBounds);
                }
            }
        }
        else {
            nodeBranchLine->setVisible(false);
        }
    }

    int getTotalContentHeight() const
    {
        int totalHeight = isVisible() ? 25 : 0;
        
        if(isOpen()) {
            for (auto* node : nodes)
            {
                totalHeight += node->isVisible() ? node->getTotalContentHeight() : 0;
            }
        }
        
        return totalHeight;
    }
    
    static bool compareProperties(ValueTree oldTree, ValueTree newTree)
    {
        for(int i = 0; i < oldTree.getNumProperties(); i++)
        {
            auto name = oldTree.getPropertyName(i);
            if(!newTree.hasProperty(name) || newTree.getProperty(name) != oldTree.getProperty(name)) return false;
        }
        
        return true;
    }

    int getPositionInViewport()
    {
        auto* node = this;
        int posInParent = 0;
        while (node) {
            posInParent += node->getPosition().getY();
            node = node->parent;
        }
        return posInParent;
    }

    ValueTree valueTreeNode;

private:
    ValueTreeNodeComponent* parent;
    SafePointer<ValueTreeNodeComponent> previous, next;
    OwnedArray<ValueTreeNodeComponent> nodes;
    bool isOpened = false;
    bool isOpenedBySearch = false;
    bool isDragging = false;

    std::unique_ptr<ValueTreeNodeBranchLine> nodeBranchLine;

    friend class ValueTreeViewerComponent;
};

#include "SettingsFile.h"

class ValueTreeViewerComponent : public Component, public KeyListener, public SettingsFileListener
{
public:
    explicit ValueTreeViewerComponent(String prepend = String()) : tooltipPrepend(std::move(prepend))
    {
        if (tooltipPrepend == "(Subpatch)") // FIXME: this is horrible
            sortLayerOrder = SettingsFile::getInstance()->getProperty<bool>("search_order");

        // Add a Viewport to handle scrolling
        viewport.setViewedComponent(&contentComponent, false);
        viewport.setScrollBarsShown(true, false, false, false);
        viewport.addKeyListener(this);
        
        contentComponent.setVisible(true);
        contentComponent.updateView = [this](){
            resized();
        };
        
        contentComponent.onClick =   [this](ValueTree& tree){ onClick(tree); };
        contentComponent.onSelect =   [this](ValueTree& tree){ onSelect(tree); };
        contentComponent.onDragStart = [this](ValueTree& tree){ onDragStart(tree); };
        
        addAndMakeVisible(viewport);
    }

    void propertyChanged(String const& name, var const& value) override
    {
        if (tooltipPrepend != "(Subpatch)") // FIXME: again this is a horrible way to find out what we are!
            return;

        if (name == "search_order") {
            setSortDir(static_cast<bool>(value));
        }
    }

    void setValueTree(const ValueTree& tree)
    {
        valueTree = tree;
        auto originalViewPos = viewport.getViewPosition();
        
        // Compare existing child nodes with current children
        for (int i = 0; i < valueTree.getNumChildren(); ++i)
        {
            ValueTree childNode = valueTree.getChild(i);
  
            // Check if an existing node exists for this child
            ValueTreeNodeComponent* existingNode = nullptr;
            for (auto* node : nodes)
            {
                if (childNode.isValid() && ValueTreeNodeComponent::compareProperties(childNode, node->valueTreeNode))
                {
                    existingNode = node;
                    node->valueTreeNode = childNode;
                    break;
                }
            }
            
            // If an existing node is found, update it; otherwise, create a new node
            if (existingNode != nullptr)
            {
                existingNode->update();
            }
            else if(childNode.isValid())
            {
                auto* childComponent = new ValueTreeNodeComponent(childNode, nullptr, tooltipPrepend);
                nodes.add(childComponent);
                contentComponent.addAndMakeVisible(childComponent);
            }
        }
        
        // Remove any existing nodes that no longer exist in the current children
        for (int i = nodes.size(); --i >= 0;)
        {
            if(!nodes.getUnchecked(i)->valueTreeNode.isAChildOf(valueTree))
            {
                nodes.remove(i);
            }
        }

        sortNodes(nodes, sortLayerOrder);
        
        ValueTreeNodeComponent* previous = nullptr;
        linkNodes(nodes, previous);
        
        contentComponent.resized();
        resized();
        
        viewport.setViewPosition(originalViewPos);
    }

    void clearValueTree()
    {
        ValueTree emptyTree;
        setValueTree(emptyTree);
    }

    ValueTree& getValueTree()
    {
        return valueTree;
    }

    void setSortDir(bool sortDir)
    {
        sortLayerOrder = sortDir;
        sortNodes(nodes, sortLayerOrder);
        resizeAllNodes(nodes);
    }

    void resizeAllNodes(OwnedArray<ValueTreeNodeComponent>& nodes)
    {
        resized();

        for (auto* node : nodes) {
            node->resized();
            resizeAllNodes(node->nodes);
        }
    }

    void paint(Graphics& g) override
    {
        g.fillAll(getLookAndFeel().findColour(PlugDataColour::sidebarBackgroundColourId));
    }
    
    int getTotalContentHeight() const
    {
        int totalHeight = 0;
        for (auto* node : nodes)
        {
            totalHeight += node->isVisible() ? node->getTotalContentHeight() : 0;
        }
        return totalHeight;
    }
    
    void resized() override
    {
        auto originalViewPos = viewport.getViewPosition();
        
        // Set the bounds of the Viewport within the main component
        auto bounds = getLocalBounds();
        viewport.setBounds(bounds);
        
        contentComponent.setBounds(0, 0, bounds.getWidth(), std::max(getTotalContentHeight(), bounds.getHeight()));
        
        auto scrollbarIndent = viewport.canScrollVertically() ? 8 : 0;
        bounds = bounds.reduced(2, 0).withTrimmedRight(scrollbarIndent).withHeight(getTotalContentHeight() + 4).withTrimmedTop(4);


        auto resizeNodes = [bounds]( ValueTreeNodeComponent* node) mutable {
            if(node->isVisible()) {
                auto childBounds = bounds.removeFromTop(node->getTotalContentHeight());
                node->setBounds(childBounds);
            }
        };

        for (auto* node : nodes) {
            resizeNodes(node);
        }

        viewport.setViewPosition(originalViewPos);
    }
    
    bool keyPressed(KeyPress const& key, Component* component) override
    {
        if(key.getKeyCode() == KeyPress::upKey)
        {
            if(contentComponent.selectedNode && contentComponent.selectedNode->previous) {
                
                contentComponent.selectedNode = contentComponent.selectedNode->previous;
                
                // Keep iterating until we find a node that is visible
                while(contentComponent.selectedNode != nullptr && contentComponent.selectedNode->parent != nullptr && !contentComponent.selectedNode->parent->isOpen() && contentComponent.selectedNode->isShowing())
                {
                    contentComponent.selectedNode = contentComponent.selectedNode->previous;
                }
                if(contentComponent.selectedNode) onSelect(contentComponent.selectedNode->valueTreeNode);
                
                contentComponent.repaint();
                resized();
                scrollToShowSelection();
            }
            
            return true;
        }
        if(key.getKeyCode() == KeyPress::downKey)
        {
            if(contentComponent.selectedNode && contentComponent.selectedNode->next) {
                contentComponent.selectedNode = contentComponent.selectedNode->next;
                
                while(contentComponent.selectedNode != nullptr && contentComponent.selectedNode->parent != nullptr && !(contentComponent.selectedNode->parent->isOpen() && contentComponent.selectedNode->isShowing()))
                {
                    contentComponent.selectedNode = contentComponent.selectedNode->next;
                }
                if(contentComponent.selectedNode) onSelect(contentComponent.selectedNode->valueTreeNode);

                contentComponent.repaint();
                resized();
                scrollToShowSelection();
            }
            return true;
        }
        
        return false;
    }
    
    void scrollToShowSelection()
    {
        if(auto* selection = contentComponent.selectedNode.getComponent())
        {
            auto viewBounds = viewport.getViewArea();
            auto selectionBounds = contentComponent.getLocalArea(selection, selection->getLocalBounds());
                                                                 
            if (selectionBounds.getY() < viewBounds.getY())
            {
                viewport.setViewPosition(0, selectionBounds.getY());
            }
            else if (selectionBounds.getBottom() > viewBounds.getBottom())
            {
                viewport.setViewPosition(0, selectionBounds.getY() - (viewBounds.getHeight() - 25));
            }
        }
    }
    
    Viewport& getViewport()
    {
        return viewport;
    }
    
    void setFilterString(const String& toFilter)
    {
        filterString = toFilter;
        
        if(filterString.isEmpty())
        {
            for (auto* topLevelNode : nodes)
            {
                clearSearch(topLevelNode);
            }
            
            resized();
            return;
        }
        

        for (auto* topLevelNode : nodes)
        {
           searchInNode(topLevelNode);
        }

        resized();
    }
    
    std::function<void(ValueTree&)> onClick =   [](ValueTree&){};
    std::function<void(ValueTree&)> onSelect =   [](ValueTree&){};
    std::function<void(ValueTree&)> onDragStart = [](ValueTree&){};
    
private:
    
    static void linkNodes(OwnedArray<ValueTreeNodeComponent>& nodes, ValueTreeNodeComponent*& previous)
    {
        // Iterate over direct children
        for(auto* node : nodes)
        {
            if(previous)
            {
                node->previous = previous;
                previous->next = node;
            }

            // Recursively iterate over grandchildren
            linkNodes(node->nodes, node);

            // Set previous to the last child processed
            previous = node;
        }
    }
    
    bool searchInNode(ValueTreeNodeComponent* node)
    {
        // Check if the current node matches the filterString
        bool found = filterString.isEmpty() || node->valueTreeNode.getProperty("Name").toString().containsIgnoreCase(filterString);
        
        for (auto* child : node->nodes)
        {
            // We can't return early because searchInNode has side effects
            found = searchInNode(child) || found;
        }
        
        node->isOpenedBySearch = !node->nodes.isEmpty() && found;
        
        // Set the visibility of the node based on whether it matches the filterString
        node->setVisible(found);

        return found;
    }
    
    void clearSearch(ValueTreeNodeComponent* node)
    {
        node->setVisible(true);
        node->isOpenedBySearch = false;
        for (auto* child : node->nodes)
        {
            clearSearch(child);
        }
    }
    
    static void sortNodes(OwnedArray<ValueTreeNodeComponent>& nodes, bool sortDown)
    {
        struct {
            bool sortDirection = false;

            int compareElements (const ValueTreeNodeComponent* a, const ValueTreeNodeComponent* b)
            {
                auto firstIdx = a->valueTreeNode. getParent().indexOf(a->valueTreeNode);
                auto secondIdx = b->valueTreeNode.getParent().indexOf (b->valueTreeNode);
                if(firstIdx > secondIdx)  return sortDirection ? -1 : 1;
                if(firstIdx < secondIdx)  return sortDirection ? 1 : -1;
                return 0;
            }
        } elementComparator;

        elementComparator.sortDirection = sortDown;

        nodes.sort(elementComparator, false);

        for (auto* childNode : nodes) {
            sortNodes(childNode->nodes, sortDown);
        }
    }
    
    String filterString;
    String tooltipPrepend;
    ValueTreeOwnerView contentComponent;
    OwnedArray<ValueTreeNodeComponent> nodes;
    ValueTree valueTree = ValueTree("Folder");
    BouncingViewport viewport;
    bool sortLayerOrder = false;
};
