#ifndef __CONTAINER_H__
#define __CONTAINER_H__

#include <iostream>
#include <string>
#include <mutex>
#include <map>
#include <vector>

#include "kvalue.hpp"

using namespace std;

class TTask;

class TContainer {
    const string name;

    mutex lock;
    enum EContainerState {
        Stopped,
        Running,
        Paused,
        Destroying
    };
    EContainerState state;
    TTask *task;
    map<string, string> properties;

    mutex _data_lock;
    // data

    bool CheckState(EContainerState expected);
    string GetPropertyLocked(string property);

public:
    TContainer(const string name);
    ~TContainer();

    string Name();

    bool Start();
    bool Stop();
    bool Pause();
    bool Resume();

    string GetProperty(string property);
    bool SetProperty(string property, string value);

    string GetData(string data);
};

class TContainerHolder {
    mutex lock;
    map <string, TContainer*> containers;

    public:
    TContainer* Create(string name);
    TContainer* Find(string name);

    void Destroy(string name);

    vector<string> List();
};

#endif
