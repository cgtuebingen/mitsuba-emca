/********************************************** MITSUBA TYPES **********************************************/

#include <mitsuba/core/dataapimitsuba.h>
#include <mitsuba/render/shape.h>

MTS_NAMESPACE_BEGIN

std::atomic<DataApiMitsuba* > DataApiMitsuba::m_ptrInstance {nullptr};

DataApiMitsuba* DataApiMitsuba::getInstance() {
    DataApiMitsuba *singleton = m_ptrInstance.load(std::memory_order_relaxed);
    // in most cases, the instance already exists so skip this part
    if (!singleton) {
        DataApiMitsuba *instance = new DataApiMitsuba();

        bool swapped = false;
        while (!swapped && !singleton)
            swapped = m_ptrInstance.compare_exchange_weak(singleton, instance);

        if (swapped)
            return instance;
        else // some other thread was faster - delete the instance
            delete instance;
    }
    return singleton;
}

void DataApiMitsuba::setPathOrigin(const mitsuba::Point3f& p) {
    emca::DataApi::setPathOrigin(emca::Point3f(p.x, p.y, p.z));
}

void DataApiMitsuba::setIntersectionPos(const mitsuba::Point3f& p) {
    emca::DataApi::setIntersectionPos(emca::Point3f(p.x, p.y, p.z));
}

void DataApiMitsuba::setNextEventEstimationPos(const mitsuba::Point3f& p, bool visible) {
    emca::DataApi::setNextEventEstimationPos(emca::Point3f(p.x, p.y, p.z), visible);
}

void DataApiMitsuba::setIntersectionEstimate(const mitsuba::Spectrum& c) {
    emca::DataApi::setIntersectionEstimate(emca::Color4f(c[0], c[1], c[2]));
}

void DataApiMitsuba::setIntersectionEmission(const mitsuba::Spectrum& c) {
    emca::DataApi::setIntersectionEmission(emca::Color4f(c[0], c[1], c[2]));
}

void DataApiMitsuba::setFinalEstimate(const mitsuba::Spectrum& c) {
    emca::DataApi::setFinalEstimate(emca::Color4f(c[0], c[1], c[2]));
}

void DataApiMitsuba::addHeatmapData(const Shape* shape, uint32_t primIndex, const Point3f &p, const Spectrum &value, float weight)
{
    if (!heatmap.isCollecting())
        return;

    // discard samples which are not associated to a shape
    if (shape == nullptr) {
        SLog(EWarn, "discarding sample without shape");
        return;
    }

    auto it = shape_to_id.find(shape);
    // discard samples on unknown shapes (this should never happen!)
    if (it == shape_to_id.end()) {
        if (shape_to_id.empty())
            SLog(EError, "mapping from shapes to object ids not configured");
        SLog(EWarn, "discarding sample from unknown shape");
        return;
    }
    const uint32_t mesh_id = it->second;

    // discard samples on shapes that do not provide trianlge ids (for now)
    if (primIndex == -1U) {
        // TODO: perform ray intersection on the proxy mesh
        return;
    }

    heatmap.addSample(mesh_id, emca::Point3f{p.x, p.y, p.z}, primIndex, emca::Color4f(value[0], value[1], value[2]), weight);
}

void DataApiMitsuba::configureShapeMapping(const ref_vector<Shape>& shapes)
{
    for (uint32_t i=0; i<shapes.size(); ++i)
        shape_to_id.emplace(shapes.at(i).get(), i);
}

MTS_NAMESPACE_END
