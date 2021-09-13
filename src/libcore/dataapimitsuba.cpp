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
    emca::DataApi::setIntersectionEstimate(emca::Color3f(c[0], c[1], c[2]));
}

void DataApiMitsuba::setIntersectionEmission(const mitsuba::Spectrum& c) {
    emca::DataApi::setIntersectionEmission(emca::Color3f(c[0], c[1], c[2]));
}

void DataApiMitsuba::setFinalEstimate(const mitsuba::Spectrum& c) {
    emca::DataApi::setFinalEstimate(emca::Color3f(c[0], c[1], c[2]));
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
        SLog(EWarn, "discarding sample with unknown shape");
        return;
    }
    const uint32_t mesh_id = it->second;

    // discard samples on shapes that do not provide trianlge ids (for now)
    if (primIndex == -1U || shape->getPrimitiveCount() != m_meshes.at(mesh_id).triangles.size()) {
        // TODO: perform ray intersection on the proxy mesh
        return;
    }

    heatmap.addSample(mesh_id, emca::Point3f{p.x, p.y, p.z}, primIndex, emca::Color3f(value[0], value[1], value[2]), weight);
}

void DataApiMitsuba::initHeatmapMitsuba(const ref_vector<Shape> &shapes, uint32_t subdivision_budget)
{
    float global_surface_area = 0.0f;
    for (uint32_t i=0; i<shapes.size(); ++i)
        global_surface_area += shapes.at(i).get()->getSurfaceArea();

    const float inv_global_surface_area = 1.0f/global_surface_area;

    std::vector<uint32_t> subdivision_budgets(shapes.size());

    for (uint32_t i=0; i<shapes.size(); ++i) {
        shape_to_id.emplace(shapes.at(i).get(), i);
        // evenly distribute the available subdivision faces based on area
        subdivision_budgets.at(i) = static_cast<uint32_t>(subdivision_budget*(shapes.at(i).get()->getSurfaceArea()*inv_global_surface_area));
    }

    heatmap.initialize(m_meshes, subdivision_budgets);
}

MTS_NAMESPACE_END
