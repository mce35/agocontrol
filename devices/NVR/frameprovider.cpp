#include "frameprovider.h"
#include <string>
#include <queue>
#include <uuid/uuid.h>
#include "agolog.h"

using namespace std;

/********************************
 * FRAME CLASS
 *******************************/
/*AgoFrame::AgoFrame(int consumers)
{
    pthread_mutex_init(&_mutex, NULL);
    _consumers = consumers;
}*/

/*AgoFrame::~AgoFrame()
{
    pthread_mutex_destroy(&_mutex);
}*/


/**
 * Decrease internal counter when frame has been processed
 */
/*void AgoFrame::done()
{
    pthread_mutex_lock(&_mutex);
    _consumers--;
    pthread_mutex_unlock(&_mutex);
}*/

/**
 * Return number of consumers
 */
/*int AgoFrame::getConsumers()
{
    return _consumers;
}*/


/********************************
 * FRAME CONSUMER CLASS
 *******************************/
AgoFrameConsumer::AgoFrameConsumer()
{
    uuid_t tmpuuid;
    char name[50];
    uuid_generate(tmpuuid);
    uuid_unparse(tmpuuid, name);
    _id = string(name);
}

AgoFrameConsumer::~AgoFrameConsumer()
{
    //nothing to do
}

/**
 * Return consumer id
 */
string AgoFrameConsumer::getId()
{
    return _id;
}

/**
 * Push frame to queue
 */
//void AgoFrameConsumer::pushFrame(AgoFrame* frame)
void AgoFrameConsumer::pushFrame(Mat frame)
{
    //AGO_DEBUG() << "frame pushed " << frame->consumers << " " << frame->frame.cols << "x" << frame->frame.rows;
    //AGO_DEBUG() << "consumer push frame";
    _frames.push(frame);
}

/**
 * Return oldest frame in queue.
 * /!\ This function is blocking!
 */
//AgoFrame* AgoFrameConsumer::popFrame(CallbackBoostThreadSleep theadSleepForCallback)
Mat AgoFrameConsumer::popFrame(CallbackBoostThreadSleep theadSleepForCallback)
{
    while( _frames.size()==0 )
    {
        theadSleepForCallback(boost::chrono::milliseconds(10));
    }
    //if( _frames.size()>0 )
    //{
        //AGO_DEBUG() << "consumer pop frame";
        /*AgoFrame* f = _frames.front();
        _frames.pop();
        return f;*/
        Mat frame = _frames.front();
        _frames.pop();
        return frame;
    //}
    //else
    //{
        //no frame in queue
    //    return NULL;
   // }
}


/********************************
 * FRAME PROVIDER CLASS
 *******************************/
AgoFrameProvider::AgoFrameProvider(std::string uri)
{
    _uri = uri;
    _fps = 0;
    _resolution = Size(0,0);
    _isRunning = false;
}

AgoFrameProvider::~AgoFrameProvider()
{
    stop();
}

/**
 * Start producer
 */
bool AgoFrameProvider::start()
{
    if( _isRunning )
    {
        AGO_WARNING() << "AgoFrameProvider for uri '" << _uri << "' is already running. Unable to start again.";
        return false;
    }

    //init video reader
    _capture = new VideoCapture(_uri);
    if( !_capture->isOpened() )
    {
        AGO_ERROR() << "AgoFrameProvider: unable to capture uri '" << _uri << "'";
        _isRunning = false;
        return false;
    }

    //get some video properties
    _resolution = Size(_capture->get(CV_CAP_PROP_FRAME_WIDTH), _capture->get(CV_CAP_PROP_FRAME_HEIGHT));
    _fps = _capture->get(CV_CAP_PROP_FPS);

    //launch thread
    _isRunning = true;
    _thread = new boost::thread(&AgoFrameProvider::threadFunction, this);

    return true;
}

/**
 * Stop producer
 */
void AgoFrameProvider::stop()
{
    if( !_isRunning )
    {
        AGO_WARNING() << "AgoFrameProvider for uri '" << _uri << "' is not running. Unable to stop";
        return;
    }

    //stop thread
    _thread->interrupt();
    _thread->join();

    //clear queue
    while( _frames.size()>0 )
    {
        _frames.pop();
    }
}

/**
 * Return true if thread is running
 */
bool AgoFrameProvider::isRunning()
{
    return _isRunning;
}

/**
 * Add subscription
 */
void AgoFrameProvider::subscribe(AgoFrameConsumer* consumer)
{
    bool found = false;
    for( list<AgoFrameConsumer*>::iterator it=_consumers.begin(); it!=_consumers.end(); it++ )
    {
        if( consumer->getId()==(*it)->getId() )
        {
            found = true;
            break;
        }
    }

    if( !found )
    {
        _consumers.push_back(consumer);
    }
}

/**
 * Remove subscription
 */
void AgoFrameProvider::unsubscribe(AgoFrameConsumer* consumer)
{
    for( list<AgoFrameConsumer*>::iterator it=_consumers.begin(); it!=_consumers.end(); it++ )
    {
        if( consumer->getId()==(*it)->getId() )
        {
            _consumers.erase(it);
            break;
        }
    }
}

/**
 * Return stream fps
 */
int AgoFrameProvider::getFps()
{
    return _fps;
}

/**
 * Return stream resolution
 */
Size AgoFrameProvider::getResolution()
{
    return _resolution;
}

/**
 * Thread function
 */
void AgoFrameProvider::threadFunction()
{
    AGO_DEBUG() << "AgoFrameProvider for uri '" << _uri << "' started";
    Mat frame;

    //thread loop
    try
    {
        while(true)
        {
            try
            {
                if( _consumers.size()==0 )
                {
                    //no consumers connected, drop frame (shoud not consume ressources)
                    _capture->grab();
                }
                else
                {
                    //consumers connected
                    (*_capture) >> frame;
                    //_frames.push(frame);
    
                    //push frame to registered consumers
                    for( list<AgoFrameConsumer*>::iterator it=_consumers.begin(); it!=_consumers.end(); it++ )
                    {
                        (*it)->pushFrame(frame);
                    }
                }
    
                /*while( _frames.size()>0 )
                {
                    AgoFrame* f =_frames.front();
                    if( f->getConsumers()==0 )
                    {
                        //AGO_DEBUG() << "provider purge frame";
                        delete f;
                        _frames.pop();
                    }
                    else
                    {
                        break;
                    }
                }*/
            }
            catch(cv::Exception& e)
            {
                AGO_ERROR() << "AgoFrameProvider for uri '" << _uri << "' opencv exception occured: " << e.what();
            }

            //check interruption
            boost::this_thread::interruption_point();
        }
    }
    catch(boost::thread_interrupted& interruption)
    {
        AGO_DEBUG() << "AgoFrameProvider for uri '" << _uri << "' interrupted";
    }
    /*catch(cv::Exception& e)
    {
        AGO_ERROR() << "AgoFrameProvider for uri '" << _uri << "' opencv exception occured: " << e.what();
    }*/
    catch(std::exception& e)
    {
        AGO_ERROR() << "AgoFrameProvider for uri '" << _uri << "' exception occured: " << e.what();
    }

    //set running flag
    _isRunning = false;

    //close all
    _capture->release();

    AGO_DEBUG() << "AgoFrameProvider for uri '" << _uri << "' stopped";
}

