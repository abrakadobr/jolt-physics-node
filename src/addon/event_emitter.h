#pragma once

#include "napi/js_convert.h"

namespace JOLT {

class EventEmitter {
public:
    void on(const std::string &event, JsCallback cb) {
        _callbacksMap[event].push_back(std::move(cb));
    }

    template<class T>
    void emit(const std::string &event, T data) const {
        if (_callbacksMap.count(event) > 0) {
            for (const JsCallback &cb : _callbacksMap.at(event)) {
                cb.call(data);
            }
        }
    }

protected:
    JsCallbacksMap _callbacksMap;
};

}
