#include "Shadow.h"

#include "RayTracer.h"
#include "Material.h"

using namespace std;

bool Shadow::hard(const Vec3Df & pos, const Vec3Df& light) const {
    Ray riShadow;

    Vec3Df dir = light - pos;
    float dist = dir.normalize();

    bool inter = rt->intersect(dir, pos, riShadow);
    if(inter && dynamic_cast<const Glass *>(&riShadow.getIntersectedObject()->getMaterial()))
        return true;

    return !inter || (inter && riShadow.getIntersectionDistance() > dist);
}

float Shadow::soft(const Vec3Df & pos, const Light & light) const {
    unsigned int nb_impact = 0;
    vector<Vec3Df> pulse_light = generateImpulsion(light);

    for(const Vec3Df & impulse_l : pulse_light)
        nb_impact += int(!hard(pos, impulse_l));

    return float(nbImpulse - nb_impact) / float(nbImpulse);
}

std::vector<Vec3Df> Shadow::generateImpulsion(const Light & light) const{
    std::vector<Vec3Df> impulsion;
    impulsion.resize(nbImpulse);
    auto random = []() {
        return float(rand())/float(RAND_MAX);
    };//rand in [0,1[

    for(unsigned int i = 0 ; i < nbImpulse ; i++) {
        Vec3Df r(random(), random(), random());
        r = r.projectOn(light.getNormal());
        r.normalize();
        float norm = 2*random()-1.f;
        r = light.getRadius()*norm*r;
        impulsion[i] = light.getPos() + r;
    }
    return impulsion;
}

float Shadow::operator()(const Vec3Df & pos, const Light & light) const {
    bool noSoft = rt->getQuality() != RayTracer::Quality::OPTIMAL;
    if(mode == HARD || ((mode==SOFT) && noSoft))
        return float(hard(pos, light.getPos()));
    else if(mode == SOFT) {
        return soft(pos, light);
    }
    return 1.0;
}
