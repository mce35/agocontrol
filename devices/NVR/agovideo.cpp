#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <syslog.h>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <opencv2/opencv.hpp>
#include <queue>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>
#include <sys/wait.h>

#include "agoapp.h"
#include "frameprovider.h"

#ifndef VIDEOMAPFILE
#define VIDEOMAPFILE "maps/videomap.json"
#endif

#ifndef RECORDINGSDIR
#define RECORDINGSDIR "recordings/"
#endif

using namespace qpid::messaging;
using namespace qpid::types;
using namespace agocontrol;
using namespace std;
using namespace cv;
namespace pt = boost::posix_time;
namespace fs = boost::filesystem;

typedef struct TBox {
    int minX;
    int maxX;
    int minY;
    int maxY;
} Box;

class AgoVideo: public AgoApp {
private:
    std::string agocontroller;
    qpid::types::Variant::Map videomap;
    pthread_mutex_t videomapMutex;
    void eventHandler(std::string subject, qpid::types::Variant::Map content);
    qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content);
    bool stopProcess;
    void setupApp();
    void cleanupApp();

    //video
    void getRecordings(std::string type, qpid::types::Variant::List& list);
    std::string getDateTimeString(bool date, bool time, bool withSeparator=true, std::string fieldSeparator="_");
    map<string, AgoFrameProvider*> frameProviders;
    AgoFrameProvider* getFrameProvider(string uri);
    pthread_mutex_t frameProvidersMutex;

    //timelapse
    bool stopTimelapses;
    std::map<std::string, boost::thread*> timelapseThreads;
    void fillTimelapse(qpid::types::Variant::Map* timelapse, qpid::types::Variant::Map* content);
    void timelapseFunction(string internalid, qpid::types::Variant::Map timelapse);
    void restartTimelapses();
    void launchTimelapses();
    void launchTimelapse(string internalid, qpid::types::Variant::Map& timelapse);
    void stopTimelapse(string internalid);

    //motion
    std::map<std::string, boost::thread*> motionThreads;
    void fillMotion(qpid::types::Variant::Map* timelapse, qpid::types::Variant::Map* content);
    void motionFunction(string internalid, qpid::types::Variant::Map timelapse);
    void launchMotions();
    void launchMotion(string internalid, qpid::types::Variant::Map& motion);
    void stopMotion(string internalid);

public:

    AGOAPP_CONSTRUCTOR_HEAD(AgoVideo)
        , stopProcess(false)
        , stopTimelapses(false) {}
};


/**
 * Return frame provider. If provider doesn't exist for specified uri, new one is created
 * and returned
 */
AgoFrameProvider* AgoVideo::getFrameProvider(string uri)
{
    AgoFrameProvider* out = NULL;

    pthread_mutex_lock(&frameProvidersMutex);

    //search existing frame provider
    map<string, AgoFrameProvider*>::iterator item = frameProviders.find(uri);
    if( item==frameProviders.end() )
    {
        AGO_DEBUG() << "Create new frame provider '" << uri << "'";
        //frame provider doesn't exist for specified uri, create new one
        AgoFrameProvider* provider = new AgoFrameProvider(uri);
        if( !provider->start() )
        {
            //unable to start frame provider, uri is valid?
            return NULL;
        }
        frameProviders[uri] = provider;
        out = provider;
    }
    else
    {
        AGO_DEBUG() << "Frame provider already exists for '" << uri << "'";
        out = item->second;
    }

    pthread_mutex_unlock(&frameProvidersMutex);

    return out;
}

/**
 * Timelapse function (threaded)
 */
void AgoVideo::timelapseFunction(string internalid, qpid::types::Variant::Map timelapse)
{
    //init video reader (provider and consumer)
    string timelapseUri = timelapse["uri"].asString();
    AgoFrameProvider* provider = getFrameProvider(timelapseUri);
    if( provider==NULL )
    {
        //no frame provider
        AGO_ERROR() << "Timelapse '" << internalid << "': stopped because no provider available";
        return;
    }
    AgoFrameConsumer consumer;
    provider->subscribe(&consumer);

    AGO_DEBUG() << "Timelapse '" << internalid << "': started";

    //init video writer
    bool fileOk = false;
    int inc = 0;
    fs::path filepath;
    while( !fileOk )
    {
        std::string name = timelapse["name"].asString();
        stringstream filename;
        filename << RECORDINGSDIR;
        filename << "timelapse_";
        filename << internalid << "_";
        filename << getDateTimeString(true, false, false);
        if( inc>0 )
        {
            filename << "_" << inc;
        }
        filename << ".avi";
        filepath = ensureParentDirExists(getLocalStatePath(filename.str()));
        if( fs::exists(filepath) )
        {
            //file already exists
            inc++;
        }
        else
        {
            fileOk = true;
        }
    }
    AGO_DEBUG() << "Record into '" << filepath.c_str() << "'";
    string codec = timelapse["codec"].asString();
    int fourcc = CV_FOURCC('F', 'M', 'P', '4');
    if( codec.length()==4 )
    {
        fourcc = CV_FOURCC(codec[0], codec[1], codec[2], codec[3]);
    }
    int fps = 24;
    VideoWriter recorder(filepath.c_str(), fourcc, fps, provider->getResolution());
    if( !recorder.isOpened() )
    {
        //XXX emit error?
        AGO_ERROR() << "Timelapse '" << internalid << "': unable to open recorder";
        return;
    }

    try
    {
        int now = (int)(time(NULL));
        int last = 0;
        Mat frame;
        while( !stopProcess && !stopTimelapses )
        {
            if( provider->isRunning() )
            {
                //get frame in any case (to empty queue)
                frame = consumer.popFrame(&boost::this_thread::sleep_for);
                recorder << frame;

                //TODO handle recording fps using timelapse["fps"] param.
                //For now it records at 1 fps to minimize memory leak of cv::VideoWriter
                if( now!=last )
                {
                    //need to copy frame to alter it
                    Mat copiedFrame = frame.clone();

                    //add text
                    stringstream stream;
                    stream << getDateTimeString(true, true, true, " ");
                    stream << " - " << timelapse["name"].asString();
                    string text = stream.str();

                    try
                    {
                        putText(copiedFrame, text.c_str(), Point(20,20), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0,0,0), 4, CV_AA);
                        putText(copiedFrame, text.c_str(), Point(20,20), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255,255,255), 1, CV_AA);
                    }
                    catch(cv::Exception& e)
                    {
                        AGO_ERROR() << "Timelapse '" << internalid << "': opencv exception occured " << e.what();
                    }

                    //record frame
                    recorder << copiedFrame;
                    last = now;
                }
            }

            //update current time
            now = (int)(time(NULL));
    
            //check if thread has been interrupted
            boost::this_thread::interruption_point();
        }
    }
    catch(boost::thread_interrupted &e)
    {
        AGO_DEBUG() << "Timelapse '" << internalid << "': thread interrupted";
    }
    
    //close all
    AGO_DEBUG() << "Timelapse '" << internalid << "': close recorder";
    if( recorder.isOpened() )
    {
        recorder.release();
    }
    
    //remove timelapse device
    //AGO_DEBUG() << "Timelapse '" << internalid << "': remove device";
    //agoConnection->removeDevice(internalid.c_str());

    //unsubscribe from provider
    AGO_DEBUG() << "Timelapse '" << internalid << "': unsubscribe from frame provider";
    provider->unsubscribe(&consumer);

    AGO_DEBUG() << "Timelapse '" << internalid << "': stopped";
}

/**
 * Fill timelapse map
 * @param timelapse: map to fill
 * @param content: map from request used to prefill output map
 */
void AgoVideo::fillTimelapse(qpid::types::Variant::Map* timelapse, qpid::types::Variant::Map* content=NULL)
{
    if( content==NULL )
    {
        //fill with default values
        (*timelapse)["name"] = "noname";
        (*timelapse)["uri"] = "";
        (*timelapse)["fps"] = 1;
        (*timelapse)["codec"] = "FMP4";
        (*timelapse)["enabled"] = true;
    }
    else
    {
        //fill with specified content
        if( !(*content)["name"].isVoid() )
            (*timelapse)["name"] = (*content)["name"].asString();
        else
            (*timelapse)["name"] = "noname";

        if( !(*content)["uri"].isVoid() )
            (*timelapse)["uri"] = (*content)["uri"].asString();
        else
            (*timelapse)["uri"] = "";

        if( !(*content)["fps"].isVoid() )
            (*timelapse)["fps"] = (*content)["fps"].asInt32();
        else
            (*timelapse)["fps"] = 1;

        if( !(*content)["codec"].isVoid() )
            (*timelapse)["codec"] = (*content)["codec"].asString();
        else
            (*timelapse)["codec"] = "FMP4";

        if( !(*content)["enabled"].isVoid() )
            (*timelapse)["enabled"] = (*content)["enabled"].asBool();
        else
            (*timelapse)["enabled"] = true;
    }
}

/**
 * Restart timelapses
 */
void AgoVideo::restartTimelapses()
{
    //stop current timelapses
    stopTimelapses = true;
    for( std::map<std::string, boost::thread*>::iterator it=timelapseThreads.begin(); it!=timelapseThreads.end(); it++ )
    {
        stopTimelapse(it->first);
    }

    //then restart them all
    stopTimelapses = false;
    launchTimelapses();
}

/**
 * Launch all timelapses
 */
void AgoVideo::launchTimelapses()
{
    qpid::types::Variant::Map timelapses = videomap["timelapses"].asMap();
    for( qpid::types::Variant::Map::iterator it=timelapses.begin(); it!=timelapses.end(); it++ )
    {
        string internalid = it->first;
        qpid::types::Variant::Map timelapse = it->second.asMap();
        launchTimelapse(internalid, timelapse);
    }
}

/**
 * Launch specified timelapse
 */
void AgoVideo::launchTimelapse(string internalid, qpid::types::Variant::Map& timelapse)
{
    //create timelapse device
    agoConnection->addDevice(internalid.c_str(), "timelapse");
    
    AGO_DEBUG() << "Launch timelapse '" << internalid << "'";
    if( timelapse["enabled"].asBool()==true )
    {
        boost::thread* thread = new boost::thread(boost::bind(&AgoVideo::timelapseFunction, this, internalid, timelapse));
        timelapseThreads[internalid] = thread;
    }
    else
    {
        AGO_DEBUG() << " -> not launch because timelapse is disabled";
    }
}

/**
 * Stop timelapse thread
 */
void AgoVideo::stopTimelapse(string internalid)
{
    //stop thread
    timelapseThreads[internalid]->interrupt();
    timelapseThreads[internalid]->join();

    //remove thread from list
    timelapseThreads.erase(internalid);
}

/**
 * Detect motion function
 * @return number of changes
 */
inline int detectMotion(const Mat& motion, Mat& result, Box& area, int maxDeviation, Scalar& color)
{
    //calculate the standard deviation
    Scalar mean, stddev;
    meanStdDev(motion, mean, stddev);
    //AGO_DEBUG() << "stddev[0]=" << stddev[0];

    //if not to much changes then the motion is real (neglect agressive snow, temporary sunlight)
    if( stddev[0]<maxDeviation )
    {
        int numberOfChanges = 0;
        int minX = motion.cols, maxX = 0;
        int minY = motion.rows, maxY = 0;

        // loop over image and detect changes
        for( int j=area.minY; j<area.maxY; j+=2 ) // height
        {
            for( int i=area.minX; i<area.maxX; i+=2 ) // width
            {
                //check if at pixel (j,i) intensity is equal to 255
                //this means that the pixel is different in the sequence
                //of images (prev_frame, current_frame, next_frame)
                if( static_cast<int>(motion.at<uchar>(j,i))==255 )
                {
                    numberOfChanges++;
                    if( minX>i ) minX = i;
                    if( maxX<i ) maxX = i;
                    if( minY>j ) minY = j;
                    if( maxY<j ) maxY = j;
                }
            }
        }

        if( numberOfChanges )
        {
            //check if not out of bounds
            if( minX-10>0) minX -= 10;
            if( minY-10>0) minY -= 10;
            if( maxX+10<result.cols-1 ) maxX += 10;
            if( maxY+10<result.rows-1 ) maxY += 10;

            // draw rectangle round the changed pixel
            Point x(minX, minY);
            Point y(maxX, maxY);
            Rect rect(x, y);
            rectangle(result, rect, color, 2);
        }

        return numberOfChanges;
    }

    return 0;
}

/**
 * Motion function (threaded)
 * Based on Cédric Verstraeten source code: https://github.com/cedricve/motion-detection/blob/master/motion_src/src/motion_detection.cpp
 */
void AgoVideo::motionFunction(string internalid, qpid::types::Variant::Map motion)
{
    AGO_DEBUG() << "Motion '" << internalid << "': started";

    //init video reader (provider and consumer)
    string motionUri = motion["uri"].asString();
    AgoFrameProvider* provider = getFrameProvider(motionUri);
    if( provider==NULL )
    {
        //no frame provider
        AGO_ERROR() << "Motion '" << internalid << "': stopped because no provider available";
        return;
    }
    AgoFrameConsumer consumer;
    provider->subscribe(&consumer);
    Size resolution = provider->getResolution();
    int fps = provider->getFps();
    AGO_DEBUG() << "Motion '" << internalid << "': fps=" << fps;

    //init buffer
    unsigned int maxBufferSize = motion["bufferduration"].asInt32() * fps;
    std::queue<Mat> buffer;

    //get frames and convert to gray
    //AgoFrame* frame = NULL;
    Mat prevFrame, currentFrame, nextFrame, result, tempFrame;
    if( provider->isRunning() )
    {
        //frame = consumer.popFrame(&boost::this_thread::sleep_for);
        tempFrame = consumer.popFrame(&boost::this_thread::sleep_for);
        //prevFrame = temp.clone();
        cvtColor(tempFrame, prevFrame, CV_RGB2GRAY);
        //frame->done();

        //frame = consumer.popFrame(&boost::this_thread::sleep_for);
        //currentFrame = frame->frame.clone();
        tempFrame = consumer.popFrame(&boost::this_thread::sleep_for);
        //currentFrame = temp.clone();
        cvtColor(tempFrame, currentFrame, CV_RGB2GRAY);
        //frame->done();

        //frame = consumer.popFrame(&boost::this_thread::sleep_for);
        //nextFrame = frame->frame.clone();
        tempFrame = consumer.popFrame(&boost::this_thread::sleep_for);
        //nextFrame = temp.clone();
        cvtColor(tempFrame, nextFrame, CV_RGB2GRAY);
        //frame->done();
    }

    //other declarations
    std::string name = motion["name"].asString();
    fs::path recordPath;
    int fourcc = CV_FOURCC('F', 'M', 'P', '4');
    int now = (int)time(NULL);
    int startup = now;
    int onDuration = motion["onduration"].asInt32() - motion["bufferduration"].asInt32();
    int recordDuration = motion["recordduration"].asInt32();
    bool isRecording, isTriggered = false;
    int triggerStart = 0;
    Mat d1, d2, _motion;
    int numberOfChanges = 0;
    Scalar color(0,0,255);
    int thereIsMotion = motion["sensitivity"].asInt32();
    int maxDeviation = motion["deviation"].asInt32();
    AGO_TRACE() << "Motion '" << internalid << "': maxdeviation=" << maxDeviation << " sensitivity=" << thereIsMotion;
    Mat kernelErode = getStructuringElement(MORPH_RECT, Size(2,2));
    Box area = {0, currentFrame.cols, 0, currentFrame.rows};
    AGO_TRACE() << "Motion '" << internalid << "': area minx=" << area.minX << " maxx=" << area.maxX << " miny=" << area.minY << " maxy=" << area.maxY;
    VideoWriter recorder("", 0, 1, Size(10,10)); //dummy recorder, unable to compile with empty constructor :S

    //debug purpose: display frame in window
    //namedWindow("Display window", WINDOW_AUTOSIZE );

    try
    {
        while( !stopProcess )
        {
            if( provider->isRunning() )
            {
                //get new frame
                prevFrame = currentFrame;
                currentFrame = nextFrame;
                //frame = consumer.popFrame(&boost::this_thread::sleep_for);
                //nextFrame = frame->frame; //copy frame to alter it (add text)
                tempFrame = consumer.popFrame(&boost::this_thread::sleep_for);
                //nextFrame = temp.clone();
                //frame->done();
                result = tempFrame; //keep color copy
                cvtColor(tempFrame, nextFrame, CV_RGB2GRAY);

                //add text to frame (current time and motion name)
                stringstream stream;
                stream << getDateTimeString(true, true, true, " ");
                if( name.length()>0 )
                {
                    stream << " - " << name;
                }
                string text = stream.str();
                try
                {
                    putText(result, text.c_str(), Point(20,20), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0,0,0), 4, CV_AA);
                    putText(result, text.c_str(), Point(20,20), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255,255,255), 1, CV_AA);
                }
                catch(cv::Exception& e)
                {
                    AGO_ERROR() << "Motion '" << internalid << "': opencv exception #1 occured " << e.what();
                }

                //handle buffer
                if( !isRecording )
                {
                    while( buffer.size()>=maxBufferSize )
                    {
                        //remove old frames
                        buffer.pop();
                    }
                    buffer.push(result);
                }

                //calc differences between the images and do AND-operation
                //threshold image, low differences are ignored (ex. contrast change due to sunlight)
                try
                {
                    absdiff(prevFrame, nextFrame, d1);
                    absdiff(nextFrame, currentFrame, d2);
                    bitwise_and(d1, d2, _motion);
                    threshold(_motion, _motion, 35, 255, CV_THRESH_BINARY);
                    erode(_motion, _motion, kernelErode);
                }
                catch(cv::Exception& e)
                {
                    AGO_ERROR() << "Motion '" << internalid << "': opencv exception #2 occured " << e.what();
                }

                //debug purpose: display frame in window
                //imshow("Display window", _motion);
                
                //check if thread has been interrupted
                boost::this_thread::interruption_point();

                //update current time
                now = (int)time(NULL);

                //drop first 5 seconds for stabilization
                if( now<=(startup+5) )
                {
                    continue;
                }

                //detect motion
                numberOfChanges = 0;
                try
                {
                    numberOfChanges = detectMotion(_motion, result, area, maxDeviation, color);
                }
                catch(cv::Exception& e)
                {
                    AGO_ERROR() << "Motion '" << internalid << "': opencv exception #3 occured " << e.what();
                }

                if( !isTriggered )
                {
                    //analyze motion
                    if( numberOfChanges>=thereIsMotion )
                    {
                        //save picture and send pictureavailable event
                        stringstream filename;
                        filename << "/tmp/" << internalid << ".jpg";
                        std::string picture = filename.str();
                        try
                        {
                            imwrite(picture.c_str(), result);
                            qpid::types::Variant::Map content;
                            content["filename"] = picture;
                            agoConnection->emitEvent(internalid.c_str(), "event.device.pictureavailable", content);
                        }
                        catch(...)
                        {
                            AGO_ERROR() << "Motion '" << internalid << "': Unable to write motion picture '" << picture << "' to disk";
                        }
                
                        //prepare recorder
                        filename.str("");
                        filename << RECORDINGSDIR;
                        filename << "motion_";
                        filename << internalid << "_";
                        filename << getDateTimeString(true, true, false, "_");
                        filename << ".avi";
                        recordPath = ensureParentDirExists(getLocalStatePath(filename.str()));
                        AGO_DEBUG() << "Motion '" << internalid << "': record to " << recordPath.c_str();

                        try
                        {
                            recorder.open(recordPath.c_str(), fourcc, fps, resolution);
                            if( !recorder.isOpened() )
                            {
                                //XXX emit error?
                                AGO_ERROR() << "Motion '" << internalid << "': unable to open recorder";
                            }
                            triggerStart = (int)time(NULL);
                            AGO_DEBUG() << "Motion '" << internalid << "': enable motion trigger and start motion recording";
                            isRecording = true;
                            isTriggered = true;
    
                            //and empty buffer into recorder
                            while( buffer.size()>0 )
                            {
                                recorder << buffer.front();
                                buffer.pop();
                            }
                        }
                        catch(cv::Exception& e)
                        {
                            AGO_ERROR() << "Motion '" << internalid << "': opencv exception #4 occured " << e.what();
                        }

                        //emit security event (enable motion sensor)
                        agoConnection->emitEvent(internalid.c_str(), "event.device.statechanged", 255, "");
                    }
                }
                else
                {
                    //handle recording
                    if( isRecording && now>=(triggerStart+recordDuration) )
                    {
                        AGO_DEBUG() << "Motion '" << internalid << "': stop motion recording";

                        //stop recording
                        if( recorder.isOpened() )
                        {
                            recorder.release();
                        }
                        isRecording = false;

                        //emit video available event
                        qpid::types::Variant::Map content;
                        content["filename"] = recordPath.c_str();
                        agoConnection->emitEvent(internalid.c_str(), "event.device.videoavailable", content);
                    }
                    else
                    {
                        //save current frame
                        recorder << result;
                    }

                    //handle trigger
                    if( isTriggered && now>=(triggerStart+onDuration) )
                    {
                        AGO_DEBUG() << "Motion '" << internalid << "': disable motion trigger";
                        isTriggered = false;

                        //emit security event (disable motion sensor)
                        agoConnection->emitEvent(internalid.c_str(), "event.device.statechanged", 0, "");
                    }
                }
            }

            //debug purpose: display frame in window
            //waitKey(5);
            
            //check if thread has been interrupted
            boost::this_thread::interruption_point();
        }
    }
    catch(boost::thread_interrupted &e)
    {
        AGO_DEBUG() << "Motion '" << internalid << "': thread interrupted";
    }

    //close all
    provider->unsubscribe(&consumer);
    if( recorder.isOpened() )
    {
        recorder.release();
    }

    //remove motion device
    //agoConnection->removeDevice(internalid.c_str());

    AGO_DEBUG() << "Motion '" << internalid << "': stopped";
}

/**
 * Fill motion map
 * @param motion: map to fill
 * @param content: content from request. This map is used to prefill output map
 */
void AgoVideo::fillMotion(qpid::types::Variant::Map* motion, qpid::types::Variant::Map* content=NULL)
{
    if( content==NULL )
    {
        //fill with default parameters
        (*motion)["name"] = "noname";
        (*motion)["uri"] = "";
        (*motion)["deviation"] = 20;
        (*motion)["sensitivity"] = 10;
        (*motion)["bufferduration"] = 10;
        (*motion)["onduration"] = 300;
        (*motion)["recordduration"] = 30;
        (*motion)["enabled"] = true;
    }
    else
    {
        //fill with content
        if( !(*content)["name"].isVoid() )
            (*motion)["name"] = (*content)["name"].asString();
        else
            (*motion)["name"] = "noname";

        if( !(*content)["uri"].isVoid() )
            (*motion)["uri"] = (*content)["uri"].asString();
        else
            (*motion)["uri"] = "";

        if( !(*content)["deviation"].isVoid() )
            (*motion)["deviation"] = (*content)["deviation"].asInt32();
        else
            (*motion)["deviation"] = 20;

        if( !(*content)["sensitivity"].isVoid() )
            (*motion)["sensitivity"] = (*content)["sensitivity"].asInt32();
        else
            (*motion)["sensitivity"] = 10;

        if( !(*content)["bufferduration"].isVoid() )
            (*motion)["bufferduration"] = (*content)["bufferduration"].asInt32();
        else
            (*motion)["bufferduration"] = 1;

        if( !(*content)["onduration"].isVoid() )
            (*motion)["onduration"] = (*content)["onduration"].asInt32();
        else
            (*motion)["onduration"] = 300;

        if( !(*content)["recordduration"].isVoid() )
            (*motion)["recordduration"] = (*content)["recordduration"].asInt32();
        else
            (*motion)["recordduration"] = 30;

        if( !(*content)["enabled"].isVoid() )
            (*motion)["enabled"] = (*content)["enabled"].asBool();
        else
            (*motion)["enabled"] = true;
    }
}

/**
 * Launch all motions
 */
void AgoVideo::launchMotions()
{
    qpid::types::Variant::Map motions = videomap["motions"].asMap();
    for( qpid::types::Variant::Map::iterator it=motions.begin(); it!=motions.end(); it++ )
    {
        string internalid = it->first;
        qpid::types::Variant::Map motion = it->second.asMap();
        launchMotion(internalid, motion);
    }
}

/**
 * Launch specified motion
 */
void AgoVideo::launchMotion(string internalid, qpid::types::Variant::Map& motion)
{
    //create motion device
    agoConnection->addDevice(internalid.c_str(), "motionsensor");

    AGO_DEBUG() << "Launch motion: " << internalid;
    if( motion["enabled"].asBool()==true )
    {
        boost::thread* thread = new boost::thread(boost::bind(&AgoVideo::motionFunction, this, internalid, motion));
        motionThreads[internalid] = thread;
    }
    else
    {
        AGO_DEBUG() << " -> not launch because motion is disabled";
    }
}

/**
 * Stop motion thread
 */
void AgoVideo::stopMotion(string internalid)
{
    //stop thread
    motionThreads[internalid]->interrupt();
    motionThreads[internalid]->join();

    //remove thread from list
    motionThreads.erase(internalid);
}

/**
 * Get recordings of specified type
 */
void AgoVideo::getRecordings(std::string type, qpid::types::Variant::List& list)
{
    fs::path recordingsPath = getLocalStatePath(RECORDINGSDIR);
    if( fs::exists(recordingsPath) )
    {
        fs::recursive_directory_iterator it(recordingsPath);
        fs::recursive_directory_iterator endit;
        while( it!=endit )
        {
            if( fs::is_regular_file(*it) && it->path().extension().string()==".avi" && boost::algorithm::starts_with(it->path().filename().string(), type) )
            {
                qpid::types::Variant::Map props;
                props["filename"] = it->path().filename().string();
                props["path"] = it->path().string();
                props["size"] = fs::file_size(it->path());
                props["date"] = fs::last_write_time(it->path());
                vector<string> splits;
                split(splits, it->path().filename().string(), boost::is_any_of("_"));
                props["internalid"] = string(splits[1]);
                list.push_back( props );
            }
            ++it;
        }
    }
}

/**
 * Return current date and time
 */
std::string AgoVideo::getDateTimeString(bool date, bool time, bool withSeparator/*=true*/, std::string fieldSeparator/*="_"*/)
{
    stringstream out;
    if( date )
    {
        out << pt::second_clock::local_time().date().year();
        if( withSeparator )
            out << "/";
        int month = (int)pt::second_clock::local_time().date().month();
        if( month<10 )
            out << "0" << month;
        else
            out << month;
        if( withSeparator )
            out << "/";
        int day = (int)pt::second_clock::local_time().date().day();
        if( day<10 )
            out << "0" << day;
        else
            out << day;
    }

    if( date && time )
    {
        out << fieldSeparator;
    }

    if( time )
    {
        int hours = (int)pt::second_clock::local_time().time_of_day().hours();
        if( hours<10 )
            out << "0" << hours;
        else
            out << hours;
        if( withSeparator )
            out << ":";
        int minutes = (int)pt::second_clock::local_time().time_of_day().minutes();
        if( minutes<10 )
            out << "0" << minutes;
        else
            out << minutes;
        if( withSeparator )
            out << ":";
        int seconds = (int)pt::second_clock::local_time().time_of_day().seconds();
        if( seconds<10 )
            out << "0" << seconds;
        else
            out << seconds;
    }

    return out.str();
}

/**
 * Event handler
 */
void AgoVideo::eventHandler(std::string subject, qpid::types::Variant::Map content)
{
    if( videomap["config"].isVoid() )
    {
        //nothing configured, exit right now
        AGO_DEBUG() << "no config";
        return;
    }

    if( subject=="event.environment.timechanged" && !content["minute"].isVoid() && !content["hour"].isVoid() )
    {
        //midnight, create new timelapse for new day
        //if( content["hour"].asInt8()==0 && content["minute"].asInt8()==0 )
        if( content["minute"].asInt8()%2==0 )
        {
            restartTimelapses();
        }
    }
    else if( subject=="event.system.devicenamechanged" )
    {
        //handle motion name changed
        bool found = false;
        string name = content["name"].asString();
        string internalid = agoConnection->uuidToInternalId(content["uuid"].asString());

        pthread_mutex_lock(&videomapMutex);

        //update motion device
        qpid::types::Variant::Map motions = videomap["motions"].asMap();
        if( !motions[internalid].isVoid() )
        {
            //its a motion device
            found = true;
            qpid::types::Variant::Map motion = motions[internalid].asMap();
            if( !motion["name"].isVoid() )
            {
                motion["name"] = name;
                motions[internalid] = motion;
                videomap["motions"] = motions;

                //restart motion
                AGO_DEBUG() << " ==> stopMotion";
                stopMotion(internalid);
                AGO_DEBUG() << " ==> launchMotion";
                launchMotion(internalid, motion);
            }
        }
        
        //update timelapse device
        if( !found )
        {
            qpid::types::Variant::Map timelapses = videomap["timelapses"].asMap();
            if( !timelapses[internalid].isVoid() )
            {
                //its a timelapse device
                found = true;
                qpid::types::Variant::Map timelapse = timelapses[internalid].asMap();
                if( !timelapse["name"].isVoid() )
                {
                    timelapse["name"] = name;
                    timelapses[internalid] = timelapse;
                    videomap["timelapses"] = timelapses;

                    //restart timelapse
                    AGO_DEBUG() << " ==> stopTimelapse";
                    stopTimelapse(internalid);
                    AGO_DEBUG() << " ==> launchTimelapse";
                    launchTimelapse(internalid, timelapse);
                }
            }
        }

        pthread_mutex_unlock(&videomapMutex);

        if( found )
        {
            if( variantMapToJSONFile(videomap, getConfigPath(VIDEOMAPFILE)) )
            {
                AGO_DEBUG() << "Event 'devicenamechanged': motion name changed";
            }
            else
            {
                AGO_ERROR() << "Event 'devicenamechanged': cannot save videomap";
            }
        }
        else
        {
            //just log an error
            AGO_ERROR() << "devicenamechanged event: no device '" << internalid << "' found in AgoVideo. Unable to rename device";
        }
    }
}

/**
 * Command handler
 */
qpid::types::Variant::Map AgoVideo::commandHandler(qpid::types::Variant::Map content)
{
    AGO_TRACE() << "handling command: " << content;
    qpid::types::Variant::Map returnData;

    std::string internalid = content["internalid"].asString();
    if (internalid == "videocontroller")
    {
        if( content["command"]=="addtimelapse" )
        {
            checkMsgParameter(content, "uri", VAR_STRING);
            checkMsgParameter(content, "fps", VAR_INT32);
            checkMsgParameter(content, "codec", VAR_STRING);
            checkMsgParameter(content, "enabled", VAR_BOOL);

            //check if timelapse already exists or not
            pthread_mutex_lock(&videomapMutex);
            string uri = content["uri"].asString();
            qpid::types::Variant::Map timelapses = videomap["timelapses"].asMap();
            for( qpid::types::Variant::Map::iterator it=timelapses.begin(); it!=timelapses.end(); it++ )
            {
                qpid::types::Variant::Map timelapse = it->second.asMap();
                if( !timelapse["uri"].isVoid() )
                {
                    string timelapseUri = timelapse["uri"].asString();
                    if( timelapseUri==uri )
                    {
                        //uri already exists, stop here
                        pthread_mutex_unlock(&videomapMutex);
                        return responseError("error.security.addtimelapse", "Timelapse already exists");
                    }
                }
            }

            //fill new timelapse
            qpid::types::Variant::Map timelapse;
            fillTimelapse(&timelapse, &content);

            //and save it
            string internalid = generateUuid();
            timelapses[internalid] = timelapse;
            videomap["timelapses"] = timelapses;
            pthread_mutex_unlock(&videomapMutex);
            if( variantMapToJSONFile(videomap, getConfigPath(VIDEOMAPFILE)) )
            {
                AGO_DEBUG() << "Command 'addtimelapse': timelapse added " << timelapse;

                //and finally launch timelapse thread
                launchTimelapse(internalid, timelapse);

                qpid::types::Variant::Map result;
                result["internalid"] = internalid;
                return responseSuccess("Timelapse added", result);
            }
            else
            {
                AGO_ERROR() << "Command 'addtimelapse': cannot save videomap";
                return responseError("error.security.addtimelapse", "Cannot save config");
            }
        }
        /*else if( content["command"]=="removetimelapse" )
        {
            bool found = false;

            checkMsgParameter(content, "uri", VAR_STRING);
            
            //search and destroy specified timelapse
            pthread_mutex_lock(&videomapMutex);
            string uri = content["uri"].asString();
            qpid::types::Variant::List timelapses = videomap["timelapses"].asList();
            for( qpid::types::Variant::List::iterator it=timelapses.begin(); it!=timelapses.end(); it++ )
            {
                qpid::types::Variant::Map timelapse = it->asMap();
                if( !timelapse["uri"].isVoid() )
                {
                    string timelapseUri = timelapse["uri"].asString();
                    if( timelapseUri==uri )
                    {
                        //timelapse found
                        found = true;
                        
                        //stop running timelapse thread
                        timelapseThreads[uri]->interrupt();

                        //and remove it from config
                        timelapses.erase(it);
                        videomap["timelapses"] = timelapses;
                        break;
                    }
                }
            }
            pthread_mutex_unlock(&videomapMutex);

            if( found )
            {
                if( variantMapToJSONFile(videomap, getConfigPath(VIDEOMAPFILE)) )
                {
                    AGO_DEBUG() << "Command 'removetimelapse': timelapse remove";
                    return responseSuccess("Timelapse removed");
                }
                else
                {
                    AGO_ERROR() << "Command 'removetimelapse': cannot save videomap";
                    return responseError("error.security.removetimelapse", "Cannot save config");
                }
            }
            else
            {
                return responseError("error.security.removetimelapse", "Specified timelapse was not found");
            }
        }*/
        else if( content["command"]=="gettimelapses" )
        {
            qpid::types::Variant::List timelapses;
            getRecordings("timelapse_", timelapses);
            returnData["timelapses"] = timelapses;
            return responseSuccess(returnData);
        }
        else if( content["command"]=="addmotion" )
        {
            checkMsgParameter(content, "uri", VAR_STRING);
            checkMsgParameter(content, "sensitivity", VAR_INT32);
            checkMsgParameter(content, "deviation", VAR_INT32);
            checkMsgParameter(content, "bufferduration", VAR_INT32);
            checkMsgParameter(content, "onduration", VAR_INT32);
            checkMsgParameter(content, "recordduration", VAR_INT32);
            checkMsgParameter(content, "enabled", VAR_BOOL);

            //check values
            if( content["recordduration"].asInt32()>=content["onduration"].asInt32() )
            {
                AGO_WARNING() << "Addmotion: record duration must be lower than on duration. Record duration forced to on duration.";
                content["recordduration"] = content["onduration"].asInt32() - 1;
            }
            if( content["bufferduration"].asInt32()>=content["recordduration"].asInt32() )
            {
                AGO_WARNING() << "Addmotion: buffer duration must be lower than record duration. Buffer duration forced to record duration.";
                content["bufferduration"] = content["recordduration"].asInt32() - 1;
            }

            //check if motion already exists or not
            pthread_mutex_lock(&videomapMutex);
            string uri = content["uri"].asString();
            qpid::types::Variant::Map motions = videomap["motions"].asMap();
            for( qpid::types::Variant::Map::iterator it=motions.begin(); it!=motions.end(); it++ )
            {
                qpid::types::Variant::Map motion = it->second.asMap();
                if( !motion["uri"].isVoid() )
                {
                    string motionUri = motion["uri"].asString();
                    if( motionUri==uri )
                    {
                        //uri already exists, stop here
                        pthread_mutex_unlock(&videomapMutex);
                        return responseError("error.security.addmotion", "Motion already exists");
                    }
                }
            }

            //fill new motion
            qpid::types::Variant::Map motion;
            fillMotion(&motion, &content);

            //and save it
            string internalid = generateUuid();
            motions[internalid] = motion;
            videomap["motions"] = motions;
            pthread_mutex_unlock(&videomapMutex);
            if( variantMapToJSONFile(videomap, getConfigPath(VIDEOMAPFILE)) )
            {
                AGO_DEBUG() << "Command 'addmotion': motion added " << motion;

                //and finally launch motion thread
                launchMotion(internalid, motion);

                qpid::types::Variant::Map result;
                result["internalid"] = internalid;
                return responseSuccess("Motion added", result);
            }
            else
            {
                AGO_ERROR() << "Command 'addmotion': cannot save videomap";
                return responseError("error.security.addmotion", "Cannot save config");
            }
        }
        /*else if( content["command"]=="removemotion" )
        {
            bool found = false;

            checkMsgParameter(content, "uri", VAR_STRING);
            
            //search and destroy specified motion
            pthread_mutex_lock(&videomapMutex);
            string uri = content["uri"].asString();
            qpid::types::Variant::Map motions = videomap["motions"].asMap();
            for( qpid::types::Variant::List::iterator it=motions.begin(); it!=motions.end(); it++ )
            {
                qpid::types::Variant::Map motion = it->asMap();
                if( !motion["uri"].isVoid() )
                {
                    string motionUri = motion["uri"].asString();
                    if( motionUri==uri )
                    {
                        //motion found
                        found = true;
                        
                        //stop running motion thread
                        stopMotion(motion);

                        //and remove it from config
                        motions.erase(it);
                        videomap["motions"] = motions;
                        break;
                    }
                }
            }
            pthread_mutex_unlock(&videomapMutex);

            if( found )
            {
                if( variantMapToJSONFile(videomap, getConfigPath(VIDEOMAPFILE)) )
                {
                    AGO_DEBUG() << "Command 'removemotion': motion removed";
                    return responseSuccess("Motion removed");
                }
                else
                {
                    AGO_ERROR() << "Command 'removemotion': cannot save videomap";
                    return responseError("error.security.removemotion", "Cannot save config");
                }
            }
            else
            {
                return responseError("error.security.removemotion", "Specified motion was not found");
            }
        }*/
        else if( content["command"]=="getmotions" )
        {
            qpid::types::Variant::List motions;
            getRecordings("motion_", motions);
            returnData["motions"] = motions;
            return responseSuccess(returnData);
        }
        else if( content["command"]=="getrecordingsconfig" )
        {
            qpid::types::Variant::Map config = videomap["recordings"].asMap();
            returnData["config"] = config;
            return responseSuccess(returnData);
        }
        else if( content["command"]=="setrecordingsconfig" )
        {
            checkMsgParameter(content, "timelapseslifetime", VAR_INT32);
            checkMsgParameter(content, "motionslifetime", VAR_INT32);

            pthread_mutex_lock(&videomapMutex);
            qpid::types::Variant::Map config = videomap["recordings"].asMap();
            config["timelapseslifetime"] = content["timelapseslifetime"].asInt32();
            config["motionslifetime"] = content["motionslifetime"].asInt32();
            pthread_mutex_lock(&videomapMutex);

            if( variantMapToJSONFile(videomap, getConfigPath(VIDEOMAPFILE)) )
            {
                AGO_DEBUG() << "Command 'setrecordingsconfig': recordings config stored";
                return responseSuccess();
            }
            else
            {
                AGO_ERROR() << "Command 'setrecordingsconfig': cannot save videomap";
                return responseError("error.security.setrecordingsconfig", "Cannot save config");
            }
        }

        return responseUnknownCommand();
    }
    else
    {
        //handle motion/timelapse devices
        if( content["command"]=="enable" )
        {
            /*bool found = false;

            checkMsgParameter(content, "enabled", VAR_BOOL);
            bool enabled = content["enabled"].asBool();

            if( found )
            {
                if( variantMapToJSONFile(videomap, getConfigPath(VIDEOMAPFILE)) )
                {
                    if( enabled )
                    {
                        AGO_DEBUG() << "Command 'enablemotion': motion enabled";
                        return responseSuccess("Motion enabled");
                    }
                    else
                    {
                        AGO_DEBUG() << "Command 'enablemotion': motion disabled";
                        return responseSuccess("Motion disabled");
                    }
                }
                else
                {
                    AGO_ERROR() << "Command 'enablemotion': cannot save videomap";
                    return responseError("error.security.enablemotion", "Cannot save config");
                }
            }
            else
            {
                return responseError("error.security.enablemotion", "Specified motion was not found");
            }*/
        }
        else if( content["command"]=="enablerecording" )
        {
        }

        return responseUnknownCommand();
    }

    // We have no devices registered but our own
    throw std::logic_error("Should not go here");
}

void AgoVideo::setupApp()
{
    //init
    pthread_mutex_init(&videomapMutex, NULL);
    pthread_mutex_init(&frameProvidersMutex, NULL);

    //load config
    videomap = jsonFileToVariantMap(getConfigPath(VIDEOMAPFILE));
    //add missing sections if necessary
    if( videomap["timelapses"].isVoid() )
    {
        qpid::types::Variant::Map timelapses;
        videomap["timelapses"] = timelapses;
        variantMapToJSONFile(videomap, getConfigPath(VIDEOMAPFILE));
    }
    if( videomap["motions"].isVoid() )
    {
        qpid::types::Variant::Map motions;
        videomap["motions"] = motions;
        variantMapToJSONFile(videomap, getConfigPath(VIDEOMAPFILE));
    }
    if( videomap["recordings"].isVoid() )
    {
        qpid::types::Variant::Map recordings;
        recordings["timelapseslifetime"] = 7;
        recordings["motionslifetime"] = 14;
        videomap["recordings"] = recordings;
        variantMapToJSONFile(videomap, getConfigPath(VIDEOMAPFILE));
    }
    AGO_DEBUG() << "Loaded videomap: " << videomap;

    //finalize
    agoConnection->addDevice("videocontroller", "videocontroller");
    addCommandHandler();
    addEventHandler();

    //launch timelapse threads
    launchTimelapses();

    //launch motion threads
    launchMotions();
}

void AgoVideo::cleanupApp()
{
    //stop processes
    stopProcess = true;

    //wait for timelapse threads stop
    for( std::map<std::string, boost::thread*>::iterator it=timelapseThreads.begin(); it!=timelapseThreads.end(); it++ )
    {
        stopTimelapse(it->first);
        /*(it->second)->interrupt();
        (it->second)->join();*/
    }

    //wait for motion threads stop
    for( std::map<std::string, boost::thread*>::iterator it=motionThreads.begin(); it!=motionThreads.end(); it++ )
    {
        stopMotion(it->first);
        /*(it->second)->interrupt();
        (it->second)->join();*/
    }

    //close frame providers
    for( std::map<std::string, AgoFrameProvider*>::iterator it=frameProviders.begin(); it!=frameProviders.end(); it++ )
    {
        (it->second)->stop();
    }
}

AGOAPP_ENTRY_POINT(AgoVideo);

