#include "ComputationThread.hpp"
#include "SimpleWindow.hpp"
#include "View.hpp"
#include <QGLContext>
#include <QApplication>
#include <QMessageBox>

namespace fast {

ComputationThread::ComputationThread() {
    mIsRunning = false;
    mStop = false;
}

ComputationThread::~ComputationThread() {
    if(mIsRunning)
        stop();
    reportInfo() << "Computation thread object destroyed" << Reporter::end();
}

bool ComputationThread::isRunning() {
    std::lock_guard<std::mutex> lock(mUpdateThreadMutex); // this locks the mutex
    return mIsRunning;
}

void ComputationThread::run() {
    // This is run in the secondary (computation thread)
    {
        std::unique_lock<std::mutex> lock(mUpdateThreadMutex); // this locks the mutex
        mIsRunning = true;
        mStop = false;
    }
    QGLContext* mainGLContext = Window::getMainGLContext();
    mainGLContext->makeCurrent();

    m_signalFinished = true;

    uint executeToken = 1;
    while(true) {
		bool canUpdate = false;
        std::vector<View*> mViews;
        std::vector<std::shared_ptr<ProcessObject>> processObjects;
        {
            std::unique_lock<std::mutex> lock(mUpdateThreadMutex); // this locks the mutex
            mViews = getViews();
            processObjects = getProcessObjects();
            std::set<std::shared_ptr<ProcessObject>> setOfprocessObjects;
            setOfprocessObjects.insert(processObjects.begin(), processObjects.end());
            if(mStop)
                break;
            for(View* view : mViews) {
                auto rendererList = view->getRenderers();
                setOfprocessObjects.insert(rendererList.begin(), rendererList.end());
            }

            // Find all process objects in pipeline and check if any are modified, or if any streamers have new data which must be processed
            std::queue<ProcessObject::pointer> queue;
            for(const auto& PO : setOfprocessObjects)
                queue.push(PO);
            while(!queue.empty()) {
                auto PO = queue.front();
                queue.pop();
                if(PO->isModified()) {
                    canUpdate = true;
                    break;
                }
                if(PO->hasNewInputData(0)) {
                    canUpdate = true;
                    break;
                }
                setOfprocessObjects.insert(PO);
                for(const auto& connection : PO->getInputConnections()) {
                    auto parentPO = connection.second->getProcessObject();
                    if(setOfprocessObjects.count(parentPO) == 0) {
                        queue.push(parentPO);
                    }
                }
            }
        }
		if(!canUpdate) { // There is nothing for this computation thread to do atm, perform microsleep
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}
		bool isStreaming = false;
		bool isDone = true;
        try {
            for(auto po : processObjects) {
                po->update(executeToken);
                for(int i = 0; i < po->getNrOfInputConnections(); ++i) {
                    try {
                        auto inputData = po->getInputPort(i)->getFrame();
                        isStreaming = isStreaming || inputData->hasFrameData("streaming");
                        if(inputData->hasFrameData("streaming")) {
                            if(!inputData->isLastFrame())
                                isDone = false;
                        }
                    } catch(Exception &e) {

                    }
                }
            }
            for(View *view : mViews) {
                view->updateRenderersInput(executeToken);
                for(auto renderer : view->getRenderers()) {
                    for(int i = 0; i < renderer->getNrOfInputConnections(); ++i) {
                        try {
                            auto inputData = renderer->getInputPort(i)->getFrame();
                            isStreaming = isStreaming || inputData->hasFrameData("streaming");
                            if(inputData->hasFrameData("streaming")) {
                                if(!inputData->isLastFrame())
                                    isDone = false;
                            }
                        } catch(Exception &e) {

                        }
                    }
                }
            }
            for(View *view : mViews) {
                view->updateRenderers(executeToken);
            }
            bool signalFinished;
            {
                std::lock_guard<std::mutex> lock(mUpdateThreadMutex);
                signalFinished = m_signalFinished;
            }
            if(signalFinished) {
                if(isStreaming) {
                    if(isDone) {
                        emit pipelineFinished();
                        std::lock_guard<std::mutex> lock(mUpdateThreadMutex);
                        m_signalFinished = false;
                    }
                } else {
                    emit pipelineFinished();
                    std::lock_guard<std::mutex> lock(mUpdateThreadMutex);
                    m_signalFinished = false;
                }
            }
        } catch(ThreadStopped &e) {
            reportInfo() << "Thread stopped exception occured in ComputationThread, exiting.." << reportEnd();
            if(e.wasDueToError()) {
                QString msg = "Exception caught: " + QString(e.what());
                emit criticalError(msg);
                for (View* view : mViews) {
                    view->stopPipeline();
                    view->removeAllRenderers();
                }
                break;
            }
		} catch(Exception &e) {
            QString msg = "FAST exception caught: " + QString(e.what());
            emit criticalError(msg);
            for (View* view : mViews) {
                view->stopPipeline();
                view->removeAllRenderers();
            }
        } catch(cl::Error &e) {
			QString msg = "OpenCL exception caught: "  + QString(e.what()) + "(" + QString(getCLErrorString(e.err()).c_str()) + ")";
            emit criticalError(msg);
            for (View* view : mViews) {
                view->stopPipeline();
                view->removeAllRenderers();
            }
        } catch(std::exception &e) {
            QString msg = "Standard (std) exception caught: " + QString(e.what());
            emit criticalError(msg);
            for (View* view : mViews) {
                view->stopPipeline();
                view->removeAllRenderers();
            }
        }

        ++executeToken;
    }

    // Move GL context back to main thread
    mainGLContext->doneCurrent();
    mainGLContext->moveToThread(QApplication::instance()->thread());

    // Move this object back to main thread
    moveToThread(QApplication::instance()->thread());

    emit threadFinished();
    reportInfo() << "Computation thread has finished in run(): " << std::this_thread::get_id() << Reporter::end();
    {
        std::unique_lock<std::mutex> lock(mUpdateThreadMutex); // this locks the mutex
        mIsRunning = false;
    }
    mUpdateThreadConditionVariable.notify_one();
}

void ComputationThread::stop() {
    stopWithoutBlocking();

    std::unique_lock<std::mutex> lock(mUpdateThreadMutex); // this locks the mutex
    // Block until mIsRunning is set to false
    while(mIsRunning) {
        // Unlocks the mutex and wait until someone calls notify.
        // When it wakes, the mutex is locked again and mIsRunning is checked.
        mUpdateThreadConditionVariable.wait(lock);
    }
}

void ComputationThread::stopWithoutBlocking() {
    std::unique_lock<std::mutex> lock(mUpdateThreadMutex); // this locks the mutex
    if(!mIsRunning)
        return;
    mStop = true;
    auto views = getViews();
    auto processObjects = getProcessObjects();

    // This is run in the main thread
    reportInfo() << "Stopping pipelines and waking any blocking threads..." << Reporter::end();
    for(View* view : views) {
        view->stopRenderers();
    }
    reportInfo() << "Pipelines stopped" << Reporter::end();
    reportInfo() << "Stopping computation thread..." << Reporter::end();
    reportInfo() << "Computation thread stopped" << Reporter::end();
    for(View* view : views) {
        view->resetRenderers();
    }

    for(auto po : processObjects)
        po->stopPipeline();
}

QThread* ComputationThread::start() {
    if(isRunning())
        return new QThread();
    // Start computation thread using QThreads which is a strange thing, see https://mayaposch.wordpress.com/2011/11/01/how-to-really-truly-use-qthreads-the-full-explanation/
    reportInfo() << "Trying to start computation thread" << Reporter::end();
    QThread* thread = new QThread();
    moveToThread(thread);
    connect(thread, SIGNAL(started()), this, SLOT(run()));
    connect(this, &ComputationThread::threadFinished, [thread]() {
        thread->quit();
    });
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));

    std::weak_ptr<Window> ptr = std::static_pointer_cast<Window>(mPtr.lock());
    QGLContext* mainGLContext = Window::getMainGLContext();
    if(!mainGLContext->isValid()) {
        throw Exception("QGL context is invalid!");
    }

    mainGLContext->doneCurrent();
    mainGLContext->moveToThread(thread);
    thread->start();
    reportInfo() << "Computation thread started" << Reporter::end();
    return thread;
}

void ComputationThread::addView(View *view) {
    std::lock_guard<std::mutex> lock(mUpdateThreadMutex);
    m_views.push_back(view);
    m_signalFinished = true;
}

void ComputationThread::clearViews() {
    std::lock_guard<std::mutex> lock(mUpdateThreadMutex);
    m_views.clear();
}

View *ComputationThread::getView(int index) const {
    return m_views.at(index);
}

std::vector<View *> ComputationThread::getViews() const {
    return m_views;
}

void ComputationThread::addProcessObject(std::shared_ptr<ProcessObject> po) {
    std::lock_guard<std::mutex> lock(mUpdateThreadMutex);
    m_processObjects.push_back(po);
    m_signalFinished = true;
}

void ComputationThread::clearProcessObjects() {
    std::lock_guard<std::mutex> lock(mUpdateThreadMutex);
    m_processObjects.clear();
}

std::shared_ptr<ProcessObject> ComputationThread::getProcessObjects(int index) const {
    return m_processObjects.at(index);
}

std::vector<std::shared_ptr<ProcessObject>> ComputationThread::getProcessObjects() const {
    return m_processObjects;
}

void ComputationThread::setPipeline(const Pipeline &pipeline) {
    std::lock_guard<std::mutex> lock(mUpdateThreadMutex);
    for(auto po : pipeline.getProcessObjects()) {
        m_processObjects.push_back(po.second);
    }
    m_views = pipeline.getViews();
    m_signalFinished = true;
}

void ComputationThread::reset() {
    std::lock_guard<std::mutex> lock(mUpdateThreadMutex);
    m_signalFinished = true;
}

}
