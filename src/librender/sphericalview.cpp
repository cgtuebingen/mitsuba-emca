#include <mitsuba/render/sphericalview.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/random.h>
#include <mitsuba/core/rfilter.h>
#include <mitsuba/core/sched.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/mstream.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/imageblock.h>
#include <mitsuba/render/renderjob.h>
#include <mitsuba/render/renderqueue.h>
#include <mitsuba/core/plugin.h>

MTS_NAMESPACE_BEGIN

using namespace mitsuba;

SphericalView::SphericalView(std::string name, short id) : emca::Plugin(name, id) { }

void SphericalView::run() {

    if (m_scene == nullptr)
        return;

    std::cout << "Render Point: " << m_p.toString() << std::endl;

    ref<PluginManager> pluginManager = PluginManager::getInstance();

    Properties rFilterProps{"box"};
    ref<ReconstructionFilter> rFilter = static_cast<ReconstructionFilter*>(pluginManager->createObject(MTS_CLASS(ReconstructionFilter), rFilterProps));
    rFilter->configure();

    Properties sensorProps{"spherical"};
    sensorProps.setTransform("toWorld", Transform::lookAt(m_p, Point{m_p+Vector{1,0,0}}, Vector{0,0,1}));
    ref<Sensor> sensor = static_cast<Sensor*>(pluginManager->createObject(MTS_CLASS(Sensor), sensorProps));

    Properties filmProps{"hdrfilm"};
    filmProps.setInteger("width",  m_renderSize.x);
    filmProps.setInteger("height", m_renderSize.y);
    filmProps.setBoolean("banner", false);
    ref<Film> film = static_cast<Film*>(pluginManager->createObject(MTS_CLASS(Film), filmProps));
    film->clear();
    film->addChild(rFilter);
    film->setDestinationFile("", 0);
    film->configure();

    Properties samplerProps{"independent"};
    samplerProps.setInteger("sampleCount", m_numSamples);
    ref<Sampler> sampler = static_cast<Sampler*>(pluginManager->createObject(MTS_CLASS(Sampler), samplerProps));
    sampler->configure();

    sensor->addChild(sampler);
    sensor->addChild(film);
    sensor->configure();

    Properties integratorProps{m_integratorName};
    //disable features not compatible with a spherical camera
    integratorProps.setBoolean("lensPerturbation", false);
    integratorProps.setBoolean("multiChainPerturbation", false);
    integratorProps.setBoolean("causticPerturbation", false);
    integratorProps.setBoolean("bidirectionalMutation", true);
    integratorProps.setBoolean("manifoldPerturbation", true);
    ref<Integrator> integrator = static_cast<Integrator*>(pluginManager->createObject(MTS_CLASS(Integrator), integratorProps));
    integrator->configure();

    ref<Scene> sceneCopy = new Scene{const_cast<Scene*>(m_scene)};
    sceneCopy->setSensor(sensor);
    sceneCopy->setIntegrator(integrator);
    sceneCopy->setSampler(sampler);
    sceneCopy->configure();
    sceneCopy->setDestinationFile("");
    Film *cpfilm = sceneCopy->getFilm();
    cpfilm->setDestinationFile("", 0);

    ref<RenderQueue> renderQueue = new RenderQueue{};

    ref<RenderJob> job = new RenderJob{"sphericalView", sceneCopy, renderQueue, -1, -1, -1, false};
    job->start();

    renderQueue->waitLeft(0);
    renderQueue->join();

    m_bitmap = new Bitmap(Bitmap::ESpectrum, Bitmap::EFloat32, m_renderSize);
    m_bitmap->clear();

    film->develop(Point2i{0, 0}, m_renderSize, Point2i{0,0}, m_bitmap);
}

void SphericalView::serialize(emca::Stream *stream) const {
    std::cout << "Serialize in: " << getName() << std::endl;
    stream->writeShort(getId());
    if (m_bitmap.get()) {
        ref<MemoryStream> memStream = new MemoryStream();
        m_bitmap->write(Bitmap::EOpenEXR, memStream);
        stream->writeInt(memStream->getPos());
        stream->writeArray(memStream->getData(), memStream->getSize());
    } else {
        stream->writeInt(0);
    }
}

void SphericalView::deserialize(emca::Stream *stream) {
    std::cout << "Deserialize in: " << getName() << std::endl;
    float x = stream->readFloat();
    float y = stream->readFloat();
    float z = stream->readFloat();
    m_p = Point3f(x, y, z);
    std::cout << "Received point: " << m_p.toString() << std::endl;
    m_numSamples = stream->readInt();
    std::cout << "Received numSamples: " << m_numSamples << std::endl;
    int width = stream->readInt();
    int height = stream->readInt();
    m_renderSize.x = width;
    m_renderSize.y = height;
    std::cout << "Image size: "<< "(" << width << "x" << height << ")" << std::endl;
    m_integratorName = stream->readString();
    std::cout << "Integrator: "<< m_integratorName << std::endl;
}

MTS_NAMESPACE_END
