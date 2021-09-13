#ifndef INCLUDE_EMCA_SPHERICALVIEW_H_
#define INCLUDE_EMCA_SPHERICALVIEW_H_

#include <mitsuba/render/scene.h>
#include <emca/plugin.h>
#include <emca/stream.h>

MTS_NAMESPACE_BEGIN

class MTS_EXPORT_RENDER SphericalView : public emca::Plugin {
public:

    SphericalView(std::string name, short id);
    ~SphericalView() { };

    void setScene(const Scene *scene) { m_scene = scene; }

    void run();
    void serialize(emca::Stream *stream) const;
    void deserialize(emca::Stream *stream);

private:
    const Scene *m_scene = nullptr;
    Vector2i m_renderSize{256, 128};
    Point3f m_p{0.0, 0.0, 0.0};
    int m_numSamples = 16;
    std::string m_integratorName;
    ref<Bitmap> m_bitmap;
};

EMCA_NAMESPACE_END

#endif /* INCLUDE_EMCA_SPHERICALVIEW_H_ */
