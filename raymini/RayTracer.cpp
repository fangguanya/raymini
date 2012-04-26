// *********************************************************
// Ray Tracer Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2012 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#include <QProgressDialog>
#include <algorithm>

#include "RayTracer.h"
#include "Ray.h"
#include "Scene.h"
#include "Color.h"
#include "Brdf.h"

using namespace std;

static RayTracer * instance = NULL;

RayTracer * RayTracer::getInstance () {
    if (instance == NULL)
        instance = new RayTracer ();
    return instance;
}

void RayTracer::destroyInstance () {
    if (instance != NULL) {
        delete instance;
        instance = NULL;
    }
}

inline int clamp (float f) {
    int v = static_cast<int> (255*f);
    return min(max(v, 0), 255);
}

QImage RayTracer::render (const Vec3Df & camPos,
                          const Vec3Df & direction,
                          const Vec3Df & upVector,
                          const Vec3Df & rightVector,
                          float fieldOfView,
                          float aspectRatio,
                          unsigned int screenWidth,
                          unsigned int screenHeight) {
    QImage image (QSize (screenWidth, screenHeight), QImage::Format_RGB888);

    QProgressDialog progressDialog ("Raytracing...", "Cancel", 0, 100);
    progressDialog.show ();

    vector<pair<float, float>> offsets = AntiAliasing::generateOffsets(typeAntiAliasing, nbRayAntiAliasing);
    float tang = tan (fieldOfView);
    Vec3Df rightVec = tang * aspectRatio * rightVector / screenWidth;
    Vec3Df upVec = tang * upVector / screenHeight;

    vector<Color> buffer;
    buffer.resize(screenHeight*screenWidth);
    Scene *scene = Scene::getInstance();

    // For each picture
    for (unsigned int pictureNumber = 0; pictureNumber<5; pictureNumber++) {

        for (Object &o : scene->getObjects()) {
            if (o.isMobile()) {
                o.setTrans(o.getTrans()+Vec3Df(0, 1, 0)*0.2);
            }
        }

        // For each pixel
        for (unsigned int i = 0; i < screenWidth; i++) {
            progressDialog.setValue ((100*i)/screenWidth);
            for (unsigned int j = 0; j < screenHeight; j++) {

                Color c (backgroundColor);



                // For each ray in each pixel
                for (const pair<float, float> &offset : offsets) {
                    Vec3Df stepX = (float(i)+offset.first - screenWidth/2.f) * rightVec;
                    Vec3Df stepY = (float(j)+offset.second - screenHeight/2.f) * upVec;
                    Vec3Df step = stepX + stepY;
                    Vec3Df dir = direction + step;
                    dir.normalize ();

                    c += getColor(dir, camPos);
                }
                buffer[j*screenWidth+i] += c;
            }
        }
    }
    progressDialog.setValue (100);
    for (unsigned int i = 0; i < screenWidth; i++) {
        for (unsigned int j = 0; j < screenHeight; j++) {
            Color c = buffer[j*screenWidth+i];
            image.setPixel (i, j, qRgb (clamp (c[0]), clamp (c[1]), clamp (c[2])));
        }
    }
    return image;
}

bool RayTracer::intersect(const Vec3Df & dir,
                          const Vec3Df & camPos,
                          Ray & bestRay,
                          Object* & intersectedObject,
                          bool stopAtFirst) const {
    Scene * scene = Scene::getInstance ();
    bestRay = Ray();

    for (Object & o : scene->getObjects()) {
        Ray ray (camPos-o.getTrans ()+ DISTANCE_MIN_INTERSECT*dir, dir);

        if (o.getKDtree().intersect(ray) &&
            ray.getIntersectionDistance() < bestRay.getIntersectionDistance() &&
            ray.getIntersectionDistance() > DISTANCE_MIN_INTERSECT) {
            intersectedObject = &o;
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
    return getColor(dir, camPos, bestRay, rayTracing?0:depthPathTracing);
}

Vec3Df RayTracer::getColor(const Vec3Df & dir, const Vec3Df & camPos, Ray & bestRay, unsigned depth, Brdf::Type type) const {
    Object *intersectedObject;

    if(intersect(dir, camPos, bestRay, intersectedObject)) {
        const Material & mat = intersectedObject->getMaterial();
        Vec3Df color = mat.genColor(camPos, bestRay.getIntersection(),
                                    getLights(bestRay.getIntersection()),
                                    type);
        if(depth < depthPathTracing)
            color += mat.genColor(camPos, bestRay.getIntersection(),
                                  getLightsPT(bestRay.getIntersection(), depth),
                                  Brdf::Diffuse);
        return color;
    }

    return backgroundColor;
}

vector<Light> RayTracer::getLights(const Vertex & closestIntersection) const {
    vector<Light> lights = Scene::getInstance ()->getLights();

    for(Light &light : lights) {
        float visibilite = shadow(closestIntersection.getPos(), light);
        light.setIntensity(light.getIntensity() * visibilite);
    }

    return lights;
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
            float intensity = 1.f/(pow(1+d,3)*nbRayPathTracing);

            lights.push_back(Light(bestRay.getIntersection().getPos(),
                                   color, intensity));
        }
    }
    return lights;
}

float RayTracer::getAmbientOcclusion(Vertex intersection) const {
    if (!nbRayAmbientOcclusion) {
        return 0.1f;
    }

    int occlusion = 0;
    vector<Vec3Df> directions = intersection.getNormal().randRotate(maxAngleAmbientOcclusion,
                                                                    nbRayAmbientOcclusion);
    for (Vec3Df & direction : directions) {
        const Vec3Df & pos = intersection.getPos();

        Object *intersectedObject;
        Ray bestRay;
        if (intersect(direction, pos, bestRay, intersectedObject)) {
            if (bestRay.getIntersectionDistance() < radiusAmbientOcclusion) {
                occlusion++;
            }
        }
    }

    return (1.f-float(occlusion)/float(nbRayAmbientOcclusion))/5;
}
