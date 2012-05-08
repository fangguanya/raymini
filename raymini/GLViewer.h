// *********************************************************
// OpenGL Viewer Class, based on LibQGLViewer, compatible
// with most hardware (OpenGL 1.2).
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2010 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#pragma once

#include <GL/glew.h>
#include <QGLViewer/qglviewer.h>
#include <string>

#include "Observer.h"
#include "Vertex.h"
#include "Vec3D.h"

class Controller;
class Octree;
class Object;

class GLViewer : public QGLViewer, public Observer  {
    Q_OBJECT
public:
    GLViewer(Controller *);
    virtual ~GLViewer ();

    class Exception  {
    public:
        Exception (const std::string & msg) : message ("[GLViewer]"+msg) {}
        virtual ~Exception () {}
        const std::string & getMessage () const { return message; }
    private:
        std::string message;
    };

    virtual void update(Observable *);
    static void draw_octree(const Octree * t);
    static void drawCube(const Vec3Df min, const Vec3Df max);

    void getCameraInformation(float &fov, float &aspectRatio, float &width, float &height, Vec3Df &camPos, Vec3Df &viewDirection, Vec3Df &upVector, Vec3Df &rightVector);

protected :
    void init();
    void draw ();
    QString helpString() const;

    virtual void keyPressEvent (QKeyEvent * event);
    virtual void keyReleaseEvent (QKeyEvent * event);
    virtual void mousePressEvent (QMouseEvent * event);
    virtual void mouseMoveEvent (QMouseEvent *event);
    virtual void mouseReleaseEvent (QMouseEvent *event);
    virtual void wheelEvent (QWheelEvent * e);
    virtual void animate();

private:
    Controller *controller;

    void updateLights();
    void updateWireframe();
    void updateBackground();
    void updateFocus();
    void updateBoundingBox();

    /** Ray trace from the camera to update local focus point */
    void changeFocusPoint();

    bool focusBlinkOn;
    QTime time;
    Vertex currentFocusPoint;

    static Vec3Df focusBlinkOnColor() {return Vec3Df(1, 0, 0);}
    static Vec3Df focusBlinkOffColor() {return Vec3Df(0, 0, 0);}
    static Vec3Df focusFixedColor() {return Vec3Df(1, 1, 1);}

    static int constexpr msBetweenAnimation = 500;
};

// Some Emacs-Hints -- please don't remove:
//
//  Local Variables:
//  mode:C++
//  tab-width:4
//  End:
