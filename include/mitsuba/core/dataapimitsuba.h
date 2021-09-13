#pragma once
#if !defined(INCLUDE_EMCA_DATAAPIMITSUBA_H)
#define INCLUDE_EMCA_DATAAPIMITSUBA_H

#include <mitsuba/mitsuba.h>
#include <emca/dataapi.h>

#include <atomic>
#include <type_traits>
#include <unordered_map>

MTS_NAMESPACE_BEGIN

/**
 * Mitsuba Type Interface
 */
class MTS_EXPORT_CORE DataApiMitsuba final : public emca::DataApi {
private:
    static std::atomic<DataApiMitsuba* > m_ptrInstance;
public:
    static DataApiMitsuba* getInstance();

    // overload functions with Mitsuba-specific data types
    void setPathOrigin(const mitsuba::Point3f& p);
    void setIntersectionPos(const mitsuba::Point3f& p);
    void setNextEventEstimationPos(const mitsuba::Point3f& p, bool visible);
    void setIntersectionEstimate(const mitsuba::Spectrum& c);
    void setIntersectionEmission(const mitsuba::Spectrum& c);
    void setFinalEstimate(const mitsuba::Spectrum& c);

    // provide the generic interface
    using emca::DataApi::addIntersectionData;

    // support for Mitsuba 2D types
    template <typename T, typename std::enable_if<std::is_same<T, mitsuba::Point2i>::value || std::is_same<T, mitsuba::Point2f>::value || std::is_same<T, mitsuba::Vector2i>::value || std::is_same<T, mitsuba::Vector2f>::value, int>::type = 0>
    void addIntersectionData(const std::string& s, const T& p) {
        emca::DataApi::addIntersectionData(s, p.x, p.y);
    }

    // support for Mitsuba 3D types
    template <typename T, typename std::enable_if<std::is_same<T, mitsuba::Point3i>::value || std::is_same<T, mitsuba::Point3f>::value || std::is_same<T, mitsuba::Vector3i>::value || std::is_same<T, mitsuba::Vector3f>::value, int>::type = 1>
    void addIntersectionData(const std::string& s, const T& p) {
        emca::DataApi::addIntersectionData(s, p.x, p.y, p.z);
    }

    // support for Mitsuba Spectrum
    void addIntersectionData(const std::string& s, const mitsuba::Spectrum& c) {
        //FIXME: colors are transferred as 4 floats
        emca::DataApi::addIntersectionData(s, static_cast<float>(c[0]), static_cast<float>(c[1]), static_cast<float>(c[2]), 0.0f);
    }

    // provide the generic interface
    using emca::DataApi::addPathData;

    // support for Mitsuba 2D types
    template <typename T, typename std::enable_if<std::is_same<T, mitsuba::Point2i>::value || std::is_same<T, mitsuba::Point2f>::value || std::is_same<T, mitsuba::Vector2i>::value || std::is_same<T, mitsuba::Vector2f>::value, int>::type = 0>
    void addPathData(const std::string& s, const T& p) {
        emca::DataApi::addPathData(s, p.x, p.y);
    }

    // support for Mitsuba 3D types
    template <typename T, typename std::enable_if<std::is_same<T, mitsuba::Point3i>::value || std::is_same<T, mitsuba::Point3f>::value || std::is_same<T, mitsuba::Vector3i>::value || std::is_same<T, mitsuba::Vector3f>::value, int>::type = 1>
    void addPathData(const std::string& s, const T& p) {
        emca::DataApi::addPathData(s, p.x, p.y, p.z);
    }

    // support for Mitsuba Spectrum
    void addPathData(const std::string& s, const mitsuba::Spectrum& c) {
        //FIXME: colors are transferred as 4 floats
        emca::DataApi::addPathData(s, static_cast<float>(c[0]), static_cast<float>(c[1]), static_cast<float>(c[2]), 0.0f);
    }

    /**
     * @brief addHeatmapData records information to be displayed in 3D heatmaps
     * @param shape     intersected shape
     * @param primIndex intersected triangle
     * @param value     value to record at the location
     */
    void addHeatmapData(const Shape *shape, uint32_t primIndex, const Point3f& p, const Spectrum& value, float weight=1.0f);

    /**
     * @brief initHeatmapData initializes the data structures necessary to record 3D heatmap data
     * @param shapes list of shapes
     * @param subdivision_budget controls the fixed memory usage by the heatmap data
     *        TODO: compute how many bytes per subdivision face are needed - then one could set a memory budget instead
     */
    void initHeatmapMitsuba(const ref_vector<Shape>& shapes, uint32_t subdivision_budget=(1<<23));

private:
    DataApiMitsuba() = default;
	DataApiMitsuba(DataApiMitsuba const&) = delete;
    ~DataApiMitsuba() = default;
	void operator=(const DataApiMitsuba &) = delete;

    std::unordered_map<const Shape*, uint32_t> shape_to_id;
};

MTS_NAMESPACE_END

#endif // INCLUDE_EMCA_DATAAPIMITSUBA_H
