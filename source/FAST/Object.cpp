#include "FAST/Object.hpp"
#include "Utility.hpp"
#include <iostream>
#include <mutex>
#include <FAST/Config.hpp>

#if defined(__APPLE__) || defined(__MACOSX)

#else
#if _WIN32
#include <windows.h>

#else
#include <GL/glx.h>
#endif
#endif

#undef ERROR // undefine some windows garbage

namespace fast {

static void terminateHandler() {
#ifdef WIN32
    CONSOLE_SCREEN_BUFFER_INFO Info;
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(hStdout, &Info);
    auto defaultAttributes = Info.wAttributes;
#endif
    // This terminate handler basically just writes out the what() error message of unhandled exceptions
    try {
        auto exception = std::current_exception();
        if(exception) {
            std::rethrow_exception(exception);
        } else {
            // Normal termination, do nothing
        }
    } catch (const std::exception & e) { // For exceptions based on std::exception
#ifdef WIN32
        SetConsoleTextAttribute(hStdout, (defaultAttributes & 0x00F0) | FOREGROUND_RED | FOREGROUND_INTENSITY);
#else
        std::cerr << "\033[31;1m";
#endif
        std::cerr << "ERROR [" << std::this_thread::get_id() << "] Terminated with unhandled exception: " << e.what() << std::endl;
#ifdef WIN32
        SetConsoleTextAttribute(hStdout, defaultAttributes);
#else
        std::cerr << "\033[0m";
#endif
    } catch (...) { // For other things like `throw 1;`
#ifdef WIN32
        SetConsoleTextAttribute(hStdout, (defaultAttributes & 0x00F0) | FOREGROUND_RED | FOREGROUND_INTENSITY);
#else
        std::cerr << "\033[31;1m";
#endif
        std::cerr << "ERROR [" << std::this_thread::get_id() << "] Terminated with unknown exception." << std::endl;
#ifdef WIN32
        SetConsoleTextAttribute(hStdout, defaultAttributes);
#else
        std::cerr << "\033[0m";
#endif
    }
}

Object::Object() {
    if(!Config::getTerminateHandlerDisabled()) {
        if(std::get_terminate() != terminateHandler) {
            // Terminate handler not set, create it:
            std::set_terminate(terminateHandler);
        }
    }

    static std::once_flag flag;
    std::call_once(flag, []() {
        // Print the splash
        auto disableSplashFlag = std::getenv("FAST_DISABLE_SPLASH");
        if(disableSplashFlag == nullptr) {
            print("\n     - Powered by -     \n"
                "   _______   __________   \n"
                "  / __/ _ | / __/_  __/   https://fast-imaging.github.io\n"
                " / _// __ |_\\ \\  / /               " + getVersion() + "\n"
                "/_/ /_/ |_/___/ /_/       \n\n",
            ConsoleColor::GREEN, true, false);
            std::cout << std::flush;
        }
    });
    mRuntimeManager = RuntimeMeasurementsManager::New();
}

Reporter& Object::reportError() {
    mReporter.setType(Reporter::ERROR);
    return mReporter;
}

Reporter& Object::reportWarning() {
    mReporter.setType(Reporter::WARNING);
    return mReporter;
}

Reporter& Object::reportInfo() {
    mReporter.setType(Reporter::INFO);
    return mReporter;
}

Reporter& Object::getReporter() {
    return mReporter;
}

ReporterEnd Object::reportEnd() const {
    return Reporter::end();
}

// AttributeObject

void AttributeObject::setAttributes(std::vector<std::shared_ptr<Attribute>> attributes) {
    for(std::shared_ptr<Attribute> attribute : attributes) {
        std::string name = attribute->getName();
        if(mAttributes.count(name) == 0) {
            throw Exception("Attribute " + name + " not found for process object " + getNameOfClass());
        }

        std::shared_ptr<Attribute> localAttribute = mAttributes.at(name);
        if(localAttribute->getType() != attribute->getType())
            throw Exception("Attribute " + name + " for process object " + getNameOfClass() + " had different type then the one loaded.");

        localAttribute->setValues(attribute->getValues());
    }
}

void AttributeObject::loadAttributes() {
    //throw Exception("The process object " + getNameOfClass() + " has not implemented the loadAttributes method and therefore cannot be loaded from fast pipeline files (.fpl).");
}

void AttributeObject::createFloatAttribute(std::string id, std::string name, std::string description, float initialValue) {
    std::shared_ptr<Attribute> attribute = std::make_shared<Attribute>(id, name, description, ATTRIBUTE_TYPE_FLOAT);
    std::shared_ptr<AttributeValue> value = std::make_shared<AttributeValueFloat>(initialValue);
    attribute->setValue(value);
    mAttributes[id] = attribute;
}

void AttributeObject::createIntegerAttribute(std::string id, std::string name, std::string description, int initialValue) {
    std::shared_ptr<Attribute> attribute = std::make_shared<Attribute>(id, name, description, ATTRIBUTE_TYPE_INTEGER);
    std::shared_ptr<AttributeValue> value = std::make_shared<AttributeValueInteger>(initialValue);
    attribute->setValue(value);
    mAttributes[id] = attribute;
}

void AttributeObject::createBooleanAttribute(std::string id, std::string name, std::string description, bool initialValue) {
    std::shared_ptr<Attribute> attribute = std::make_shared<Attribute>(id, name, description, ATTRIBUTE_TYPE_BOOLEAN);
    std::shared_ptr<AttributeValue> value = std::make_shared<AttributeValueBoolean>(initialValue);
    attribute->setValue(value);
    mAttributes[id] = attribute;
}

void AttributeObject::createStringAttribute(std::string id, std::string name, std::string description, std::string initialValue) {
    std::shared_ptr<Attribute> attribute = std::make_shared<Attribute>(id, name, description, ATTRIBUTE_TYPE_STRING);
    std::shared_ptr<AttributeValue> value = std::make_shared<AttributeValueString>(initialValue);
    attribute->setValue(value);
    mAttributes[id] = attribute;
}

std::shared_ptr<Attribute> AttributeObject::getAttribute(std::string id) {
    if(mAttributes.count(id) == 0)
        throw Exception("Attribute " + id + " not found for process object " + getNameOfClass() +
                        ". Did you forget to define it in the constructor?");

    return mAttributes[id];
}

float AttributeObject::getFloatAttribute(std::string id) {
    auto attribute = getAttribute(id);
    if(attribute->getType() != ATTRIBUTE_TYPE_FLOAT)
        throw Exception("Attribute " + id + " is not of type float in process object " + getNameOfClass());

    std::shared_ptr<AttributeValueFloat> value = std::dynamic_pointer_cast<AttributeValueFloat>(attribute->getValue());
    return value->get();
}

std::vector<float> AttributeObject::getFloatListAttribute(std::string id) {
    auto attribute = getAttribute(id);
    if(attribute->getType() != ATTRIBUTE_TYPE_FLOAT)
        throw Exception("Attribute " + id + " is not of type float in process object " + getNameOfClass());

    std::vector<std::shared_ptr<AttributeValue>> values = attribute->getValues();
    std::vector<float> list;
    for(auto &&value : values) {
        auto floatValue = std::dynamic_pointer_cast<AttributeValueFloat>(value);
        list.push_back(floatValue->get());
    }
    return list;
}

int AttributeObject::getIntegerAttribute(std::string id) {
    auto attribute = getAttribute(id);
    if(attribute->getType() != ATTRIBUTE_TYPE_INTEGER)
        throw Exception("Attribute " + id + " is not of type integer in process object " + getNameOfClass());

    std::shared_ptr<AttributeValueInteger> value = std::dynamic_pointer_cast<AttributeValueInteger>(attribute->getValue());
    return value->get();
}

std::vector<int> AttributeObject::getIntegerListAttribute(std::string id) {
    auto attribute = getAttribute(id);
    if(attribute->getType() != ATTRIBUTE_TYPE_INTEGER)
        throw Exception("Attribute " + id + " is not of type integer in process object " + getNameOfClass());

    std::vector<std::shared_ptr<AttributeValue>> values = attribute->getValues();
    std::vector<int> list;
    for(auto &&value : values) {
        auto floatValue = std::dynamic_pointer_cast<AttributeValueInteger>(value);
        list.push_back(floatValue->get());
    }
    return list;
}

bool AttributeObject::getBooleanAttribute(std::string id) {
    auto attribute = getAttribute(id);
    if(attribute->getType() != ATTRIBUTE_TYPE_BOOLEAN)
        throw Exception("Attribute " + id + " is not of type boolean in process object " + getNameOfClass());

    std::shared_ptr<AttributeValueBoolean> value = std::dynamic_pointer_cast<AttributeValueBoolean>(attribute->getValue());
    return value->get();
}


std::vector<bool> AttributeObject::getBooleanListAttribute(std::string id) {
    auto attribute = getAttribute(id);
    if(attribute->getType() != ATTRIBUTE_TYPE_BOOLEAN)
        throw Exception("Attribute " + id + " is not of type boolean in process object " + getNameOfClass());

    std::vector<std::shared_ptr<AttributeValue>> values = attribute->getValues();
    std::vector<bool> list;
    for(auto &&value : values) {
        auto floatValue = std::dynamic_pointer_cast<AttributeValueBoolean>(value);
        list.push_back(floatValue->get());
    }
    return list;
}

std::string AttributeObject::getStringAttribute(std::string id) {
    auto attribute = getAttribute(id);
    if(attribute->getType() != ATTRIBUTE_TYPE_STRING)
        throw Exception("Attribute " + id + " is not of type string in process object " + getNameOfClass());

    std::shared_ptr<AttributeValueString> value = std::dynamic_pointer_cast<AttributeValueString>(attribute->getValue());
    return value->get();
}


std::vector<std::string> AttributeObject::getStringListAttribute(std::string id) {
    auto attribute = getAttribute(id);
    if(attribute->getType() != ATTRIBUTE_TYPE_STRING)
        throw Exception("Attribute " + id + " is not of type string in process object " + getNameOfClass());

    std::vector<std::shared_ptr<AttributeValue>> values = attribute->getValues();
    std::vector<std::string> list;
    for(auto &&value : values) {
        auto floatValue = std::dynamic_pointer_cast<AttributeValueString>(value);
        if(!floatValue->get().empty())
            list.push_back(floatValue->get());
    }
    return list;
}

std::unordered_map<std::string, std::shared_ptr<Attribute>> AttributeObject::getAttributes() {
    return mAttributes;
}

void Object::enableRuntimeMeasurements() {
    mRuntimeManager->enable();
}

void Object::disableRuntimeMeasurements() {
    mRuntimeManager->disable();
}

RuntimeMeasurement::pointer Object::getRuntime() {
    return mRuntimeManager->getTiming("execute");
}

RuntimeMeasurement::pointer Object::getRuntime(std::string name) {
    return mRuntimeManager->getTiming(name);
}

RuntimeMeasurementsManager::pointer Object::getAllRuntimes() {
    return mRuntimeManager;
}

RuntimeMeasurementsManager::pointer Object::getRuntimeManager() {
    return getAllRuntimes();
}

} // end namespace fast
