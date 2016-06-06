#ifndef COMPUTATION_THREAD_HPP
#define COMPUTATION_THREAD_HPP

#include "FAST/Object.hpp"
#include <QThread>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <vector>


namespace fast {

class View;

class ComputationThread : public QObject, public Object {
    Q_OBJECT
    public:
        ComputationThread(QThread* mainThread);
        ~ComputationThread();
        bool isRunning();
        void stop();
        void addView(View* view);
        void clearViews();
    public slots:
        void run();
    signals:
        void finished();
    private:

        bool mUpdateThreadIsStopped;
        bool mIsRunning;
        boost::condition_variable mUpdateThreadConditionVariable;
        boost::mutex mUpdateThreadMutex;

        QThread* mMainThread;

        std::vector<View*> mViews;
};

}

#endif
