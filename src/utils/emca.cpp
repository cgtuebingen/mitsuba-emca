#include <mitsuba/core/platform.h>
// Mitsuba's "Assert" macro conflicts with Xerces' XSerializeEngine::Assert(...).
// This becomes a problem when using a PCH which contains mitsuba/core/logger.h
#if defined(Assert)
# undef Assert
#endif
#include <xercesc/parsers/SAXParser.hpp>
#include <mitsuba/core/sched_remote.h>
#include <mitsuba/core/sstream.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/appender.h>
#include <mitsuba/core/sshstream.h>
#include <mitsuba/core/shvector.h>
#include <mitsuba/core/statistics.h>
#include <mitsuba/render/renderjob.h>
#include <mitsuba/render/scenehandler.h>
#include <fstream>
#include <stdexcept>
#include <boost/algorithm/string.hpp>
#if defined(__WINDOWS__)
#include <mitsuba/core/getopt.h>
#include <winsock2.h>
#else
#include <signal.h>
#endif
#include <mitsuba/core/timer.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/render/util.h>
#include <mitsuba/core/dataapimitsuba.h>
#include <mitsuba/render/sphericalview.h>

#include <emca/renderinterface.h>
#include <emca/scenedata.h>
#include <emca/pathdata.h>
#include <emca/emcaserver.h>

MTS_NAMESPACE_BEGIN
using XERCES_CPP_NAMESPACE::SAXParser;
static ref<RenderQueue> renderQueue = nullptr;
static emca::EMCAServer *emcaServer = nullptr;
#if !defined(__WINDOWS__)
/* Handle the hang-up signal and write a partially rendered image to disk */
void signalHandler(int signal) {
    if (signal == SIGHUP && renderQueue.get()) {
        renderQueue->flush();
    } else if (signal == SIGFPE) {
        SLog(EWarn, "Caught a floating-point exception!");
#if defined(MTS_DEBUG_FP)
        /* Generate a core dump! */
        abort();
#endif
    } else if (signal == SIGINT) {
        if (renderQueue)
            renderQueue->join();
        if (emcaServer)
            emcaServer->stop();
        else
            abort();
    }
}
#endif

class MitsubaEMCAInterface final : public emca::RenderInterface {
public:
    MitsubaEMCAInterface(int argc, char **argv) : emca::RenderInterface() {
        int retval = mitsuba_init(argc, argv);
        m_preprocessed = false;
        SLog(EInfo, "Retval from init: %d", retval);
        if(retval < 0) {
            SLog(EInfo, "Init error.");
            exit(retval);
        }
        SLog(EInfo, "Starting Server for Explorer of Monte Carlo based Algorithms ...");
    }

    ~MitsubaEMCAInterface() override {
        Scheduler* sched = Scheduler::getInstance();
        if (m_samplerResID != -1)
            sched->unregisterResource(m_samplerResID);
    }


    void runPreprocess() {
        if (!m_preprocessed) {
            if(!m_scene->preprocess(renderQueue, m_renderJob, m_sceneResID, m_sensorResID, m_samplerResID)) {
                SLog(EWarn, "Preprocessing of scene \"%s\" did not complete successfully!",
                     m_scene->getSourceFile().filename().string().c_str());
            }
            m_preprocessed = true;
        }
    }

    void renderImage() override {
        runPreprocess();

        m_scene->getFilm()->setDestinationFile(m_scene->getDestinationFile(), m_scene->getBlockSize());

        if (!m_scene->render(renderQueue, m_renderJob, m_sceneResID, m_sensorResID, m_samplerResID)) {
            SLog(EWarn, "Rendering of scene \"%s\" did not complete successfully!",
                 m_scene->getSourceFile().filename().string().c_str());
        }
        SLog(EInfo, "Render time: %s", timeString(renderQueue->getRenderTime(m_renderJob), true).c_str());
        m_scene->postprocess(renderQueue, m_renderJob, m_sceneResID, m_sensorResID, m_samplerResID);

        // write output image
        renderQueue->flush();

        Statistics::getInstance()->printStats();
        Statistics::getInstance()->resetAll();

        // preprocess again if the image is rendered again
        m_preprocessed = false;
    }

    void renderPixel(uint32_t x, uint32_t y) override {
        ref<Timer> renderPixelTimer = new Timer();

        // basic render process copied from render/integrator.cpp
        const Point2i pixel(x, y);
        const size_t sampleCount = m_sampler->getSampleCount();
        MonteCarloIntegrator *integrator = dynamic_cast<MonteCarloIntegrator*>(m_scene->getIntegrator());
        // configure sampler
        integrator->configureSampler(m_scene, m_sampler);
        const Sensor *sensor = m_scene->getSensor();
        Float diffScaleFactor = 1.0f / std::sqrt((Float) sampleCount);
        bool needsApertureSample = sensor->needsApertureSample();
        bool needsTimeSample = sensor->needsTimeSample();
        Point2 apertureSample(0.5f);
        Float timeSample = 0.5f;
        RayDifferential sensorRay;
        uint32_t queryType = RadianceQueryRecord::ESensorRay;
        /* Don't compute an alpha channel if we don't have to */
        if (!sensor->getFilm()->hasAlpha())
            queryType &= ~RadianceQueryRecord::EOpacity;
        RadianceQueryRecord rRec(m_scene, m_sampler);
        m_sampler->generate(pixel); // this seeds the sampler with a pixel-dependent value
        Spectrum spec;
        Spectrum L;
        Spectrum mcL = Spectrum(0.0);
        // work with provided data API interface for mitsuba
        DataApiMitsuba *dataApiMitsuba = DataApiMitsuba::getInstance();
        for (int sampleIdx = 0; sampleIdx < sampleCount; sampleIdx++) {
            dataApiMitsuba->setPathIdx(sampleIdx);
            rRec.newQuery(queryType, sensor->getMedium());
            Point2 samplePos(Point2(pixel) + Vector2(rRec.nextSample2D()));
            if (needsApertureSample)
                apertureSample = rRec.nextSample2D();
            if (needsTimeSample)
                timeSample = rRec.nextSample1D();
            spec = sensor->sampleRayDifferential(sensorRay, samplePos, apertureSample, timeSample);
            sensorRay.scaleDifferential(diffScaleFactor);
            // take sensor ray origin as start ray instead of camera origin
            dataApiMitsuba->setPathOrigin(Point3f(sensorRay.o.x, sensorRay.o.y, sensorRay.o.z));
            L = integrator->Li(sensorRay, rRec);
            spec *= L;
            mcL += spec;
            m_sampler->advance();
        }
        // final estimated value
        mcL /= (Float)sampleCount;

        Float duration = renderPixelTimer->stop();
        SLog(EInfo, "renderPixel() took %s", timeString(duration, true).c_str());
    }

    std::string getRendererName() const override {
        return "Mitsuba EMCA Interface";
    }

    std::string getSceneName() const override {
        fs::path pathToFile = m_scene->getSourceFile();
        fs::path fileName = pathToFile.filename();
        return  fileName.string();
    }

    size_t getSampleCount() const override {
        return m_sampler->getSampleCount();
    }

    std::string getRenderedImagePath() const override {
        fs::path pathToOutputFile = m_scene->getDestinationFile();
        return pathToOutputFile.string()+".exr";
    }

    emca::Camera getCameraData() const override {
        // get camera settings from rendering system
        const Sensor *sensor = m_scene->getSensor();
        Properties props = sensor->getProperties();
        float nearClip = props.getFloat("nearClip", 0.1);
        float farClip = props.getFloat("farClip", 100);
        float focusDist = props.getFloat("focusDistance", 10);
        float fov = props.getFloat("fov", 40);
        Transform t = props.getTransform("toWorld", Transform());
        Matrix4x4 mat = t.getMatrix();
        emca::Point3f origin = emca::Point3f(mat(0, 3), mat(1, 3), mat(2, 3));
        emca::Vec3f up = emca::Vec3f(mat(0, 1), mat(1, 1), mat(2, 1));
        emca::Vec3f dir = emca::Vec3f(mat(0, 2), mat(1, 2), mat(2, 2));

        // Initialize and serialize EMCA camera
        return emca::Camera(nearClip, farClip, focusDist, fov, up, dir, origin);
    }

    std::vector<emca::Mesh> getMeshData() const override {
        const ref_vector<Shape>& shapes = m_scene->getShapes();
        std::vector<emca::Mesh> meshes;
        meshes.reserve(shapes.size());

        for(const auto& shape : shapes) {
            emca::Mesh emcaMesh;
            // get vertex positions
            ref<TriMesh> triMesh = const_cast<Shape*>(shape.get())->createTriMesh(); // evil const cast, but creating the tri mesh should not change the shape in any way.
            //SLog(EInfo, "transferring shape %s with %zu vertices...", shape->getName().c_str(), triMesh->getVertexCount());
            const Point *vertexList = triMesh->getVertexPositions();
#ifndef SINGLE_PRECISION
            for(uint32_t vert = 0; vert < triMesh->getVertexCount(); vert++) {
                emca::Point3f emcaPoint(
                            vertexList[vert].x,
                            vertexList[vert].y,
                            vertexList[vert].z);
                emcaMesh.vertices.push_back(emcaPoint);
            }
#else
            //mitsuba uses a compatible point type, no need to convert data one-by-one
            emcaMesh.vertices.assign(reinterpret_cast<const emca::Point3f*>(vertexList), reinterpret_cast<const emca::Point3f*>(vertexList)+triMesh->getVertexCount());
#endif
            // get triangles indices
            const Triangle *triangles = triMesh->getTriangles();
            //mitsuba uses a compatible point type, no need to convert data one-by-one
            emcaMesh.triangles.assign(reinterpret_cast<const emca::Vec3i*>(triangles), reinterpret_cast<const emca::Vec3i*>(triangles)+triMesh->getTriangleCount());

            // define color of object
            if(shape->isEmitter()) {
                // FIXME: evaluate the emitter
                emcaMesh.diffuseColor = emca::Color3f(1, 1, 1);
                emcaMesh.specularColor = emca::Color3f(0, 0, 0);
            } else if(shape->isSensor()) {
                emcaMesh.diffuseColor = emca::Color3f(1, 1, 1);
                emcaMesh.specularColor = emca::Color3f(0, 0, 0);
            } else if(shape->isMediumTransition()) {
                emcaMesh.diffuseColor = emca::Color3f(1, 1, 1);
                emcaMesh.specularColor = emca::Color3f(1, 1, 1);
            } else {
                if(shape->hasBSDF()) {
                    PositionSamplingRecord pRec;
                    Intersection its;
                    its.p = pRec.p;
                    its.wi = Vector3f(0.0f, 0.0f, 1.0f);
                    const BSDF* bsdf = shape->getBSDF();
                    Spectrum diffuse = bsdf->getDiffuseReflectance(its);
                    Spectrum specular = bsdf->getSpecularReflectance(its);
                    emcaMesh.diffuseColor = emca::Color3f(diffuse[0], diffuse[1], diffuse[2]);
                    emcaMesh.specularColor = emca::Color3f(specular[0], specular[1], specular[2]);
                } else {
                    SLog(EWarn, "mesh %s has no associated BSDF", shape->getName().c_str());
                    emcaMesh.diffuseColor = emca::Color3f(0, 0, 0);
                    emcaMesh.specularColor = emca::Color3f(0, 0, 0);
                }
            }
            meshes.emplace_back(std::move(emcaMesh));
        }

        return meshes;
    }

    void setSampleCount(size_t sampleCount) override {
        Properties samplerProps{"deterministic"};
        samplerProps.setSize("sampleCount", sampleCount);
        m_sampler = static_cast<Sampler*>(PluginManager::getInstance()->createObject(MTS_CLASS(Sampler), samplerProps));
        m_scene->setSampler(m_sampler);

        // update the per-thread samplers
        Scheduler* sched = Scheduler::getInstance();
        if (m_samplerResID != -1)
            sched->unregisterResource(m_samplerResID);

        m_perThreadSamplers.resize(sched->getCoreCount());
        for (size_t i=0; i<sched->getCoreCount(); ++i) {
            ref<Sampler> clonedSampler = m_sampler->clone();
            clonedSampler->incRef();
            m_perThreadSamplers[i] = clonedSampler.get();
        }
        m_samplerResID = sched->registerMultiResource(m_perThreadSamplers);
        for (size_t i=0; i<sched->getCoreCount(); ++i)
            m_perThreadSamplers[i]->decRef();
    }

    Scene* getScene() { return m_scene; }

    fs::path getDestinationFile() { return m_scene->getDestinationFile(); }

private:
    int mitsuba_init(int argc, char **argv) {
        try {
            /* Default settings */
            std::string nodeName = getHostName(),
                    networkHosts = "", destFile="";
            bool progressBars = true;
            ref<FileResolver> fileResolver = Thread::getThread()->getFileResolver();
            std::map<std::string, std::string, SimpleStringOrdering> parameters;
            const uint32_t blockSize = 16;
            ProgressReporter::setEnabled(progressBars);
#if !defined(__WINDOWS__)
            /* Initialize signal handlers */
            struct sigaction sa;
            sa.sa_handler = signalHandler;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;
            if (sigaction(SIGHUP, &sa, NULL))
                SLog(EError, "Could not install a custom signal handler!");
            if (sigaction(SIGFPE, &sa, NULL))
                SLog(EError, "Could not install a custom signal handler!");
            if (sigaction(SIGINT, &sa, NULL))
                SLog(EError, "Could not install a custom signal handler!");
#endif
            /* Prepare for parsing scene descriptions */
            std::unique_ptr<SAXParser> parser = std::make_unique<SAXParser>();
            fs::path schemaPath = fileResolver->resolveAbsolute("data/schema/scene.xsd");
            /* Check against the 'scene.xsd' XML Schema */
            parser->setDoSchema(true);
            parser->setValidationSchemaFullChecking(true);
            parser->setValidationScheme(SAXParser::Val_Always);
            parser->setExternalNoNamespaceSchemaLocation(schemaPath.c_str());
            /* Set the handler */
            std::unique_ptr<SceneHandler> handler = std::make_unique<SceneHandler>(parameters);
            parser->setDoNamespaces(true);
            parser->setDocumentHandler(handler.get());
            parser->setErrorHandler(handler.get());
            const int i = 1;
            fs::path
                    filename = fileResolver->resolve(argv[i]),
                    filePath = fs::absolute(filename).parent_path(),
                    baseName = filename.stem();
            ref<FileResolver> frClone = fileResolver->clone();
            frClone->prependPath(filePath);
            Thread::getThread()->setFileResolver(frClone);
            SLog(EInfo, "Parsing scene description from \"%s\" ..", argv[i]);
            parser->parse(filename.c_str());
            m_scene = handler->getScene();
            m_scene->setSourceFile(filename);
            m_scene->setDestinationFile(destFile.length() > 0 ?
                                            fs::path(destFile) : (filePath / baseName));
            m_scene->setBlockSize(blockSize);
            // init scene and kd-tree
            m_scene->initialize();
            // register scene and sensor
            Scheduler* sched = Scheduler::getInstance();
            m_sceneResID = sched->registerResource(m_scene);
            m_sensorResID = sched->registerResource(m_scene->getSensor());
            // set deterministic sampler and create per-thread samplers
            // also registers sampler with the scheduler
            setSampleCount(m_scene->getSampler()->getSampleCount());

            renderQueue = new RenderQueue();
            m_renderJob = new RenderJob(formatString("ren%i", 0), m_scene, renderQueue, m_sceneResID, m_sensorResID, m_samplerResID);
        } catch (const std::exception &e) {
            std::cerr << "Caught a critical exception: " << e.what() << endl;
            return -1;
        } catch (...) {
            std::cerr << "Caught a critical exception of unknown type!" << endl;
            return -1;
        }
        return 0;
    }

    std::vector<emca::Mesh> m_meshes;
    ref<Scene> m_scene;
    ref<Sampler> m_sampler;
    // the per-thread samplers need to be managed here to respond to updates in the sample count
    std::vector<SerializableObject *> m_perThreadSamplers;
    int m_sceneResID {-1};
    int m_sensorResID {-1};
    int m_samplerResID {-1};
    ref<RenderJob> m_renderJob;

    bool m_preprocessed;
};

class EMCAServer : public Utility {
public:
    int run(int argc, char **argv) {
        if (argc < 2) {
            std::cout << "Need a scene.xml file ..." << std::endl;
            return 0;
        }

        // Init renderer and plugins
        std::unique_ptr<MitsubaEMCAInterface> mitsuba = std::make_unique<MitsubaEMCAInterface>(argc, argv);
        std::unique_ptr<SphericalView> sphericalView = std::make_unique<SphericalView>("SphericalView", 66);
        sphericalView->setScene(mitsuba->getScene());

        DataApiMitsuba* dataApi = DataApiMitsuba::getInstance();

        dataApi->setCamera(mitsuba->getCameraData());
        dataApi->setMeshData(mitsuba->getMeshData());
        dataApi->plugins.addPlugin(std::move(sphericalView));

        mitsuba->runPreprocess();

        // Init EMCA server (set pointer in mitsuba namespace to catch the interrupt signal)
        emcaServer = new emca::EMCAServer(mitsuba.get(), dataApi);
        // Run EMCA server
        emcaServer->run();
        // clean up
        delete emcaServer;

        return 0;
    }
    MTS_DECLARE_UTILITY()
};

MTS_EXPORT_UTILITY(EMCAServer, "Explorer of Monte Carlo based Algorithms")
MTS_NAMESPACE_END
