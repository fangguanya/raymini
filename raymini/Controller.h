/**
 * Handle any model modification, via the SLOTS, to ensure coherent behaviour
 */

#include <QApplication>

#pragma once

class Controller: public QApplication {
public:
    Controller(int argc, char *argv[]);
    virtual ~Controller();

    /** Start the whole procedure */
    void initAll();

    inline Window *getWindow() {return window;}
    inline GLViewer *getViewer() {return viewer;}

    inline Scene *getScene() {return scene;}
    inline RayTracer *getRayTracer() {return rayTracer;}
    inline WindowModel *getWindowModel() {return windowModel;}

public slots :
    void windowRenderRayImage();
    void windowSetShadowMode(int);
    void windowSetShadowNbRays(int);
    void windowSetBGColor();
    void windowShowRayImage();
    void windowExportGLImage();
    void windowExportRayImage();
    void windowAbout();
    void windowChangeAntiAliasingType(int index);
    void windowSetNbRayAntiAliasing(int);
    void windowChangeAmbientOcclusionNbRays(int index);
    void windowSetAmbientOcclusionMaxAngle(int);
    void windowSetAmbientOcclusionRadius(double);
    void windowSetAmbientOcclusionIntensity(int);
    void windowSetOnlyAO(bool);
    void windowEnableFocal(bool);
    void windowSetFocal();
    void windowSetDepthPathTracing(int);
    void windowSetNbRayPathTracing(int);
    void windowSetMaxAnglePathTracing(int);
    void windowSetIntensityPathTracing(int);
    void windowSetOnlyPT(bool);
    void windowSetNbImagesSpinBox(int);
    void windowSelectLight(int);
    void windowEnableLight(bool);
    void windowSetLightRadius(double);
    void windowSetLightIntensity(double);
    void windowSetLightPos();

private:
    // Views
    Window *window;
    GLViewer *viewer;

    // Models
    Scene *scene;
    RayTracer *rayTracer;
    WindowModel *windowModel;
};