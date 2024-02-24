/*
 // Copyright (c) 2021-2022 Timothy Schoen
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 */

#pragma once

#include <m_pd.h>

#include <readerwriterqueue.h>
#include "Constants.h"
#include "Objects/AllGuis.h"
#include "Iolet.h"       // Move to impl
#include "Pd/Instance.h" // Move to impl
#include "Pd/MessageListener.h"
#include "Utility/RateReducer.h"
#include "Utility/ModifierKeyListener.h"

using PathPlan = std::vector<Point<float>>;

class Canvas;
class PathUpdater;

class Connection : public Component
    , public ComponentListener
    , public Value::Listener
    , public ChangeListener
    , public pd::MessageListener {
public:
    int inIdx;
    int outIdx;
    int numSignalChannels = 1;

    WeakReference<Iolet> inlet, outlet;
    WeakReference<Object> inobj, outobj;

    Path toDraw, toDrawLocalSpace;
    RectangleList<int> clipRegion;
    String lastId;

    std::atomic<int> messageActivity;

    Connection(Canvas* parent, Iolet* start, Iolet* end, t_outconnect* oc);
    ~Connection() override;

    void updateOverlays(int overlay);

    static void renderConnectionPath(Graphics& g,
        Canvas* cnv,
        Path const& connectionPath,
        bool isSignal,
        bool isGemState,
        bool isMouseOver = false,
        bool showDirection = false,
        bool showConnectionOrder = false,
        bool isSelected = false,
        Point<int> mousePos = { 0, 0 },
        bool isHovering = false,
        int connections = 0,
        int connectionNum = 0,
        int numSignalChannels = 0);

    static Path getNonSegmentedPath(Point<float> start, Point<float> end);

    void paint(Graphics&) override;

    bool isSegmented() const;
    void setSegmented(bool segmented);

    void updatePath();

    void forceUpdate();

    void lookAndFeelChanged() override;

    void changeListenerCallback(ChangeBroadcaster* source) override;

    bool hitTest(int x, int y) override;

    void mouseDown(MouseEvent const& e) override;
    void mouseMove(MouseEvent const& e) override;
    void mouseDrag(MouseEvent const& e) override;
    void mouseUp(MouseEvent const& e) override;
    void mouseEnter(MouseEvent const& e) override;
    void mouseExit(MouseEvent const& e) override;

    Point<float> getStartPoint() const;
    Point<float> getEndPoint() const;

    void reconnect(Iolet* target);

    bool intersects(Rectangle<float> toCheck, int accuracy = 4) const;
    int getClosestLineIdx(Point<float> const& position, PathPlan const& plan);

    void setPointer(t_outconnect* ptr);
    t_outconnect* getPointer();

    t_symbol* getPathState();
    void pushPathState();
    void popPathState();

    void componentMovedOrResized(Component& component, bool wasMoved, bool wasResized) override;

    // Pathfinding
    int findLatticePaths(PathPlan& bestPath, PathPlan& pathStack, Point<float> start, Point<float> end, Point<float> increment);

    void findPath();

    void applyBestPath();

    bool intersectsObject(Object* object) const;
    bool straightLineIntersectsObject(Line<float> toCheck, Array<Object*>& objects);

    void receiveMessage(t_symbol* symbol, pd::Atom const atoms[8], int numAtoms) override;

    bool isSelected() const;

    StringArray getMessageFormated();
    int getSignalData(t_float* output, int maxChannels);

private:
    void resizeToFit();

    int getMultiConnectNumber();
    int getNumSignalChannels();
    int getNumberOfConnections();

    void valueChanged(Value& v) override;

    void setSelected(bool shouldBeSelected);

    Array<SafePointer<Connection>> reconnecting;
    Rectangle<float> startReconnectHandle, endReconnectHandle, endCableOrderDisplay;

    bool selectedFlag = false;
    bool segmented = false;

    PathPlan currentPlan;

    Value locked;
    Value presentationMode;

    bool showDirection = false;
    bool showConnectionOrder = false;
    bool showActiveState = false;

    Canvas* cnv;

    Point<float> previousPStart = Point<float>();

    int dragIdx = -1;

    float mouseDownPosition = 0;
    bool isHovering = false;

    pd::WeakReference ptr;

    pd::Atom lastValue[8];
    int lastNumArgs = 0;
    t_symbol* lastSelector = nullptr;

    friend class ConnectionPathUpdater;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Connection)
};

class ConnectionBeingCreated : public Component {
    SafePointer<Iolet> iolet;
    Component* cnv;
    Path connectionPath;

public:
    ConnectionBeingCreated(Iolet* target, Component* canvas)
        : iolet(target)
        , cnv(canvas)
    {

        // Only listen for mouse-events on canvas and the original iolet
        setInterceptsMouseClicks(false, true);
        cnv->addMouseListener(this, true);
        iolet->addMouseListener(this, false);

        cnv->addAndMakeVisible(this);

        setAlwaysOnTop(true);
    }

    ~ConnectionBeingCreated() override
    {
        cnv->removeMouseListener(this);
        iolet->removeMouseListener(this);
    }

    void mouseDrag(MouseEvent const& e) override
    {
        mouseMove(e);
    }

    void mouseMove(MouseEvent const& e) override
    {
        if (rateReducer.tooFast())
            return;

        auto ioletPoint = cnv->getLocalPoint((Component*)iolet->object, iolet->getBounds().toFloat().getCentre());
        auto cursorPoint = e.getEventRelativeTo(cnv).position;

        auto& startPoint = iolet->isInlet ? cursorPoint : ioletPoint;
        auto& endPoint = iolet->isInlet ? ioletPoint : cursorPoint;

        connectionPath = Connection::getNonSegmentedPath(startPoint.toFloat(), endPoint.toFloat());

        auto bounds = connectionPath.getBounds().getSmallestIntegerContainer().expanded(3);

        // Make sure we have minimal bounds, expand slightly to take line thickness into account
        setBounds(bounds);

        // Remove bounds offset from path, because we've already set our origin by setting component bounds
        connectionPath.applyTransform(AffineTransform::translation(-bounds.getX(), -bounds.getY()));

        repaint();
        iolet->repaint();
    }

    void paint(Graphics& g) override
    {
        if (!iolet) {
            jassertfalse; // shouldn't happen
            return;
        }
        Connection::renderConnectionPath(g, (Canvas*)cnv, connectionPath, iolet->isSignal, iolet->isGemState, true);
    }

    Iolet* getIolet()
    {
        return iolet;
    }

    RateReducer rateReducer = RateReducer(90);
};

// Helper class to group connection path changes together into undoable/redoable actions
class ConnectionPathUpdater : public Timer {
    Canvas* canvas;

    moodycamel::ReaderWriterQueue<std::pair<Component::SafePointer<Connection>, t_symbol*>> connectionUpdateQueue = moodycamel::ReaderWriterQueue<std::pair<Component::SafePointer<Connection>, t_symbol*>>(4096);

    void timerCallback() override;

public:
    explicit ConnectionPathUpdater(Canvas* cnv)
        : canvas(cnv)
    {
    }

    void pushPathState(Connection* connection, t_symbol* newPathState)
    {
        connectionUpdateQueue.enqueue({ connection, newPathState });
        startTimer(50);
    }
};
