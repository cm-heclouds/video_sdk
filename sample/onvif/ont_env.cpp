#include <stdlib.h>
#include <string>
#include <iostream>

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "ont_onvif_videoandiohandler.h"
#include "ont_env.h"


using namespace std;


class ont_task_scheduler: public BasicTaskScheduler
{
    public:
        static ont_task_scheduler* createNew(unsigned maxSchedulerGranularity=10000)
        {
            return new ont_task_scheduler(maxSchedulerGranularity);
        }
        ~ont_task_scheduler()
        {
            
        }
        /*refine internal error*/
        virtual  void internalError()
        {
            cout<<"internal err";
        }
    protected:
        ont_task_scheduler(unsigned maxSchedulerGranularity=10000):BasicTaskScheduler(maxSchedulerGranularity){
        }
};


class ont_usage_environment: public BasicUsageEnvironment {
    public:
        static ont_usage_environment* createNew(TaskScheduler& taskScheduler)
        {
            return new ont_usage_environment(taskScheduler);
        }

        ~ont_usage_environment(){
        }
        /*refine internal error*/
        virtual  void internalError(){
            cout<<"internal err";
        }

        protected:
            ont_usage_environment(TaskScheduler& taskScheduler):BasicUsageEnvironment(taskScheduler){
        }
};

void *ont_onvifdevice_create_playenv()
{
    TaskScheduler* scheduler = ont_task_scheduler::createNew();
    ont_usage_environment* env = ont_usage_environment::createNew(*scheduler);
    return env;
}


void ont_onvifdevice_delete_playenv(void *penv)
{
	ont_usage_environment* env = (ont_usage_environment*)penv;
	TaskScheduler &scheduler = env->taskScheduler();
    delete &scheduler;
    delete env;
}

