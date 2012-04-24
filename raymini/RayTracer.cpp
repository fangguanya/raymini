// *********************************************************
// Ray Tracer Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2012 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#include <QProgressDialog>

#include "RayTracer.h"
#include "Ray.h"
#include "Scene.h"
#include "AntiAliasing.h"
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

inline int clamp (float f, int inf, int sup) {
    int v = static_cast<int> (f);
    return (v < inf ? inf : (v > sup ? sup : v));
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

    vector<pair<float, float>> offsets = AntiAliasing::generateOffsets();

    // For each pixel
    for (unsigned int i = 0; i < screenWidth; i++) {
        progressDialog.setValue ((100*i)/screenWidth);
        for (unsigned int j = 0; j < screenHeight; j++) {

            Color c (backgroundColor);

            // For each ray in each pixel
            for (const pair<float, float> &offset : offsets) {
                float tanX = tan (fieldOfView)*aspectRatio;
                float tanY = tan (fieldOfView);
                Vec3Df stepX = (float(i)+offset.first - screenWidth/2.f)/screenWidth * tanX * rightVector;
                Vec3Df stepY = (float(j)+offset.second - screenHeight/2.f)/screenHeight * tanY * upVector;
                Vec3Df step = stepX + stepY;
                Vec3Df dir = direction + step;
                dir.normalize ();

                c += getColor(dir, camPos);
            }

            image.setPixel (i, j, qRgb (clamp (c[0], 0, 255), clamp (c[1], 0, 255), clamp (c[2], 0, 255)));
        }
    }
    progressDialog.setValue (100);
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
    return bestRay.intersect();
}

Vec3Df RayTracer::getColor(const Vec3Df & dir, const Vec3Df & camPos) const {
    Ray bestRay;
    return getColor(dir, camPos, bestRay);
}

Vec3Df RayTracer::getColor(const Vec3Df & dir, const Vec3Df & camPos, Ray & bestRay, unsigned depth, Brdf::Type type) const {
    Object *intersectedObject;

    if(intersect(dir, camPos, bestRay, intersectedObject)) {
        Vec3Df color = intersectedObject->genColor(camPos, bestRay.getIntersection(),
                                                   getLights(intersectedObject, bestRay.getIntersection()),
                                                   type);
        if(depth < depthPathTracing)
            color += intersectedObject->genColor(camPos, bestRay.getIntersection(),
                                                 getLightsPT(intersectedObject, bestRay.getIntersection(), depth),
                                                 Brdf::Diffuse);
        return color;
    }

    return backgroundColor;
}

vector<Light> RayTracer::getLights(Object *intersectedObject, const Vertex & closestIntersection) const {
    vector<Light> lights = Scene::getInstance ()->getLights();

    for(Light &light : lights) {
        float visibilite = shadow(closestIntersection.getPos() + intersectedObject->getTrans(), light);
        light.setIntensity(light.getIntensity() * visibilite);
    }

    return lights;
}

vector<Light> RayTracer::getLightsPT(Object *intersectedObject,
                                     const Vertex & closestIntersection, unsigned depth) const {
    vector<Light> lights;

    Vec3Df pos = closestIntersection.getPos() + intersectedObject->getTrans();
    vector<Vec3Df> dirs = getPathTracingDirection(closestIntersection.getNormal());

    for (const Vec3Df & dir : dirs) {
        Ray bestRay;
        Vec3Df color = getColor(dir, pos, bestRay, depth+1, Brdf::Diffuse);

        if(bestRay.intersect()) {
            float d = bestRay.getIntersectionDistance();
            float intensity = 1.f/(pow(1+d,3)*nbRayonPathTracing);

            lights.push_back(Light(bestRay.getIntersection().getPos(),
                                   color, intensity));
        }
    }
    return lights;
}

vector<Vec3Df> RayTracer::getPathTracingDirection(const Vec3Df & normal) const {
    vector<Vec3Df> directions;
    directions.resize(nbRayonPathTracing);

    for (unsigned int i=0 ; i < nbRayonPathTracing ; i++) {
        Vec3Df randomDirection;
        for (int j=0; j<3; j++) {
            randomDirection[j] += (float)(rand()) / (float)(RAND_MAX) - 0.5; // in [-0.5,0.5]
        }
        randomDirection.normalize();
        randomDirection *= 0.4;
        randomDirection += normal;
        randomDirection.normalize();
        directions[i] = randomDirection;
    }

    return directions;
}
