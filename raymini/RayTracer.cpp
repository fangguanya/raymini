// *********************************************************
// Ray Tracer Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2012 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#include <QImage>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <omp.h>
#include <chrono>

#include "Controller.h"
#include "RayTracer.h"
#include "Ray.h"
#include "Scene.h"
#include "Color.h"
#include "Brdf.h"

using namespace std;

inline int clamp (float f) {
    int v = static_cast<int> (255*f);
    return min(max(v, 0), 255);
}

class ProgressBar {
private:
    unsigned max;
    unsigned current;
    omp_lock_t lck;
    chrono::time_point<chrono::system_clock> start;

    void lock() { omp_set_lock(&lck); }
    void unlock() { omp_unset_lock(&lck); }

public:
    ProgressBar(unsigned nbIter) :
        max(nbIter), current(0), start(chrono::system_clock::now()) {
        omp_init_lock(&lck);
        cerr << endl
             << setw (5) << 0
             << "% >";
        for(unsigned i = 0 ; i < 100 ; i++)
            cerr << ' ';
        cerr << '<';
    }

    void operator()() {
        lock();
        float percent = 100.f*float(current)/float(max);
        cerr << '\r'
             << fixed << setprecision(2) << setw (5) << percent
             << "% >";
        for(unsigned i = 0 ; i < unsigned(percent)+1 ; i++)
            cerr << '*';
        current++;
        if(current==max) {
            auto now = chrono::system_clock::now();
            chrono::microseconds u = now -start;
            cerr << "< " << u.count()/1000 << "ms "
                 << '\r' << setw (5) << 100.00;
        }
        unlock();
    }
};

QImage RayTracer::render (const Vec3Df & camPos,
                          const Vec3Df & direction,
                          const Vec3Df & upVector,
                          const Vec3Df & rightVector,
                          float fieldOfView,
                          float aspectRatio,
                          unsigned int screenWidth,
                          unsigned int screenHeight) {
    Scene *scene = controller->getScene();
    vector<Color> buffer;
    buffer.resize(screenHeight*screenWidth);

    unsigned int pix[screenWidth*screenHeight];
    cl.getImage(camPos, direction, upVector, rightVector, fieldOfView, aspectRatio, screenWidth, screenHeight, pix);

    const vector<pair<float, float>> offsets = AntiAliasing::generateOffsets(typeAntiAliasing, nbRayAntiAliasing);
    const vector<pair<float, float>> offsets_focus = Focus::generateOffsets(typeFocus, apertureFocus, nbRayFocus);

    const float tang = tan (fieldOfView);
    const Vec3Df rightVec = tang * aspectRatio * rightVector / screenWidth;
    const Vec3Df upVec = tang * upVector / screenHeight;

    const Vec3Df camToObject = controller->getWindowModel()->getFocusPoint().getPos() - camPos;
    const float focalDistance = Vec3Df::dotProduct(camToObject, direction) - distanceOrthogonalCameraScreen;

    const unsigned nbIterations = scene->hasMobile()?nbPictures:1;
    ProgressBar progressBar(nbIterations*screenWidth);

    // For each picture
    for (unsigned picNumber = 0 ; picNumber < nbIterations ; picNumber++) {

        // For each pixel
        for (unsigned int i = 0; i < screenWidth; i++) {
            progressBar();
            for (unsigned int j = 0; j < screenHeight; j++) {
                buffer[j*screenWidth+i] += computePixel(camPos,
                                                        direction,
                                                        upVec, rightVec,
                                                        screenWidth, screenHeight,
                                                        offsets, offsets_focus,
                                                        focalDistance,
                                                        i, j);
            }
        }
        scene->move(nbPictures);
    }

    QImage image (QSize (screenWidth, screenHeight), QImage::Format_RGB888);
    for (unsigned int i = 0; i < screenWidth; i++) {
        for (unsigned int j = 0; j < screenHeight; j++) {
            Vec3Df c (  (pix[j*screenWidth+i] >> 16)/128,
                        ((pix[j*screenWidth+i] << 16)>>24)/128, 
                        ((pix[j*screenWidth+i] << 24)>>24)/128 
                        );
            image.setPixel (i, j, qRgb (clamp (c[0]), clamp (c[1]), clamp (c[2])));
        }
    }

    scene->reset();

    return image;
}

Vec3Df RayTracer::computePixel(const Vec3Df & camPos,
                               const Vec3Df & direction,
                               const Vec3Df & upVec,
                               const Vec3Df & rightVec,
                               unsigned int screenWidth,
                               unsigned int screenHeight,
                               const vector<pair<float, float>> &offsets,
                               const vector<pair<float, float>> &offsets_focus,
                               float focalDistance,
                               unsigned i, unsigned j) {
    Color c;

    // For each ray in each pixel
    for (const pair<float, float> &offset : offsets) {
        Vec3Df stepX = (float(i)+offset.first - screenWidth/2.f) * rightVec;
        Vec3Df stepY = (float(j)+offset.second - screenHeight/2.f) * upVec;
        Vec3Df step = stepX + stepY;
        Vec3Df dir = direction + step;
        cout << "i: " << i << " j: " << j << " dir: " << dir << endl;
        if(i == 0 && j == 0)
            while(1);
        cout << rightVec << endl;
        cout << upVec << endl;
        cout << camPos << endl;
        dir.normalize();
        if (typeFocus != Focus::NONE) {
            float distanceCameraScreen = sqrt(step.getLength()*step.getLength() +
                                              distanceOrthogonalCameraScreen*distanceOrthogonalCameraScreen);
            dir.normalize ();
            Vec3Df customFocalPoint = camPos + (distanceCameraScreen*(distanceOrthogonalCameraScreen + focalDistance)/
                                                distanceOrthogonalCameraScreen)*dir;
            for (const pair<float, float> &offset_focus : offsets_focus) {
                Vec3Df focusMovedCamPos = camPos + Vec3Df(1,0,0)*offset_focus.first + Vec3Df(0,1,0)*offset_focus.second;
                dir = customFocalPoint - focusMovedCamPos;
                dir.normalize();
                c += getColor(dir, focusMovedCamPos);
            }
        }
        else {
            c += getColor(dir, camPos);
        }
    }
    return c();
}

bool RayTracer::intersect(const Vec3Df & dir,
                          const Vec3Df & camPos,
                          Ray & bestRay,
                          const Object* & intersectedObject,
                          bool stopAtFirst) const {
    Scene * scene = controller->getScene();
    bestRay = Ray();


    for (const Object * o : scene->getObjects()) {
        if (!o->isEnabled()) {
            continue;
        }
        Ray ray (camPos - o->getTrans ()+ DISTANCE_MIN_INTERSECT*dir, dir);

        if (o->getKDtree().intersect(ray) &&
            ray.getIntersectionDistance() < bestRay.getIntersectionDistance() &&
            ray.getIntersectionDistance() > DISTANCE_MIN_INTERSECT) {
            intersectedObject = o;
            bestRay = ray;
            if(stopAtFirst) return true;
        }
    }

    if(bestRay.intersect()) {
        bestRay.translate(intersectedObject->getTrans());
    }

    return bestRay.intersect();
}

Vec3Df RayTracer::getColor(const Vec3Df & dir, const Vec3Df & camPos, bool rayTracing) const {
    Ray bestRay;
    Brdf::Type type = onlyAmbientOcclusion?Brdf::Ambient:Brdf::All;
    return getColor(dir, camPos, bestRay, rayTracing?0:depthPathTracing, type);
}

Vec3Df RayTracer::getColor(const Vec3Df & dir, const Vec3Df & camPos, Ray & bestRay, unsigned depth, Brdf::Type type) const {
    const Object *intersectedObject;

    if(intersect(dir, camPos, bestRay, intersectedObject)) {
        const Material & mat = intersectedObject->getMaterial();
        const vector<Light> & light = getLights(bestRay.getIntersection());

        Color color = mat.genColor(camPos, bestRay.getIntersection(),
                                   light,
                                   type);

        if((depth < depthPathTracing) || (mode == PBGI_MODE)) {

            vector<Light> lights;
            switch(mode) {
                case RAY_TRACING_MODE:
                    lights =  getLightsPT(bestRay.getIntersection(), depth);
                    break;
                case PBGI_MODE:
                    lights = controller->getPBGI()->getLights(bestRay);
                    break;
            }

            Color ptColor = mat.genColor(camPos, bestRay.getIntersection(),
                                          lights,
                                          Brdf::Diffuse);

            if(onlyPathTracing && depth == 0)
                color = ptColor;
            else
                color += ptColor;
        }
        return color();
    }

    return backgroundColor;
}

vector<Light> RayTracer::getLights(const Vertex & closestIntersection) const {
    vector<Light *> lights = controller->getScene()->getLights();
    vector<Light> enabledLights;

    for(Light * light : lights) {
        if (!light->isEnabled()) {
            continue;
        }
        float visibilite = shadow(closestIntersection.getPos(), *light);
        Light l = *light;
        l.setIntensity(light->getIntensity()*visibilite);
        enabledLights.push_back(l);
    }

    return enabledLights;
}

vector<Light> RayTracer::getLightsPT(const Vertex & closestIntersection, unsigned depth) const {
    vector<Light> lights;

    Vec3Df pos = closestIntersection.getPos();
    vector<Vec3Df> dirs = closestIntersection.getNormal().randRotate(maxAnglePathTracing,
                                                                     nbRayPathTracing);

    for (const Vec3Df & dir : dirs) {
        Ray bestRay;
        Vec3Df color = getColor(dir, pos, bestRay, depth+1, Brdf::Diffuse);

        if(bestRay.intersect()) {
            float d = bestRay.getIntersectionDistance();
            float intensity = intensityPathTracing / pow(1+d,3);

            lights.push_back(Light(bestRay.getIntersection().getPos(),
                                   color, intensity));
        }
    }
    return lights;
}

float RayTracer::getAmbientOcclusion(Vertex intersection) const {
    if (!nbRayAmbientOcclusion) return intensityAmbientOcclusion;

    int occlusion = 0;
    vector<Vec3Df> directions = intersection.getNormal().randRotate(maxAngleAmbientOcclusion,
                                                                    nbRayAmbientOcclusion);
    for (Vec3Df & direction : directions) {
        const Vec3Df & pos = intersection.getPos();

        const Object *intersectedObject;
        Ray bestRay;
        if (intersect(direction, pos, bestRay, intersectedObject)) {
            if (bestRay.getIntersectionDistance() < radiusAmbientOcclusion) {
                occlusion++;
            }
        }
    }

    return intensityAmbientOcclusion * (1.f-float(occlusion)/float(nbRayAmbientOcclusion));
}
