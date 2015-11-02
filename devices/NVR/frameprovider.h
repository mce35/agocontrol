#ifndef FRAMEPROVIDER_H
#define FRAMEPROVIDER_H

#include <string>
#include <queue>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/chrono/duration.hpp>

using namespace std;
using namespace cv;
using namespace boost;

typedef void (*CallbackBoostThreadSleep)(const chrono::duration<int, milli>&);

/*class AgoFrame
{
    private:
        pthread_mutex_t _mutex;
        int _consumers;

    public:
        AgoFrame(int _consumers);
        ~AgoFrame();

        Mat frame;
        void done();
        int getConsumers();
};*/

class AgoFrameConsumer
{
    private:
        string _id;
        queue<Mat> _frames;

    public:
        AgoFrameConsumer();
        ~AgoFrameConsumer();
        
        string getId();
        //void pushFrame(AgoFrame* frame);
        void pushFrame(Mat frame);
        //AgoFrame* popFrame(CallbackBoostThreadSleep sleepCallback);
        Mat popFrame(CallbackBoostThreadSleep sleepCallback);
};

class AgoFrameProvider
{
    private:
        string _uri;
        //queue<AgoFrame*> _frames;
        queue<Mat> _frames;
        list<AgoFrameConsumer*> _consumers;
        boost::thread* _thread;
        int _fps;
        Size _resolution;
        bool _isRunning;
        VideoCapture* _capture;

        void threadFunction();

    public:
        AgoFrameProvider(string uri);
        ~AgoFrameProvider();
    
        bool start();
        void stop();
        void subscribe(AgoFrameConsumer* consumer);
        void unsubscribe(AgoFrameConsumer* consumer);
        int getFps();
        Size getResolution();
        bool isRunning();
};

#endif

