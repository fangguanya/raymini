#pragma once

#include "Material.h"
#include "Texture.h"
#include "Vertex.h"
#include "Vec3D.h"
#include "Object.h"

class Controller;

enum CubeSide {
    TOP,
    BOTTOM,
    ONE,
    TWO,
    THREE,
    FOUR
};

/** Special Material for the skybox */
class SkyBoxMaterial : public Material {

public:
    SkyBoxMaterial(Controller *c, const char* textureFileName);
    virtual ~SkyBoxMaterial();

    virtual Vec3Df genColor (const Vec3Df & camPos, const Vertex & closestIntersection,
                             const std::vector<Light> & lights, Brdf::Type type = Brdf::All) const;
protected:
    Texture texture;

    Vec3Df getColor(CubeSide side, int u, int v) const;
};
