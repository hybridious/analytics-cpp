//
// Copyright 2017 Segment Inc. <friends@segment.com>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

#include "analytics.hpp"
#include "date.hpp"
#include "json.hpp"

#ifdef SEGMENT_USE_CURL
#include "http-curl.hpp"
#elif defined(SEGMENT_USE_WININET)
#include "http-wininet.hpp"
#else
#include "http-none.hpp"
#endif

#ifndef _WIN32
#include <sys/utsname.h>
#include <unistd.h>
#endif

// To keep this simple, we include the entire implementation in this one
// C++ file.  Organizationally we'd probably have rather split this up,
// but having a single file to integrate makes it easier to integrate
// into other projects.

using json = nlohmann::json;

namespace segment {
namespace analytics {

    std::string Version = "0.9";

#ifdef _WIN32
    void getOs(std::string& name, std::string& vers)
    {
        // Modern Windows has deprecated old GetVersionEx(), and
        // now we are supposed to check the version of a file
        // such as kernel32.dll, and report on that.
        name = "Windows";
        vers = "";
    }
#else
    void getOs(std::string& name, std::string& vers)
    {
        struct utsname uts;
        (void)uname(&uts);
        name = uts.sysname;
        vers = uts.release;
    }
#endif
    std::string TimeStamp()
    {
        auto now = std::chrono::system_clock::now();
        auto tmstamp = date::format("%FT%TZ",
            std::chrono::time_point_cast<std::chrono::milliseconds>(now));
        return tmstamp;
    }

    // raw event (untyped) -- do not use from application code.
    Event createEvent(const std::string& type)
    {
        Event ev;
        ev["timestamp"] = TimeStamp();
        ev["type"] = type;
        return (ev);
    }

    // Add the object into the event.  If the object is null or otherwise
    // not a valid JSON object, then any prior value is removed.
    void addEventObject(Event& ev,
        const std::string& name, const Object& obj)
    {
        if (obj.is_object()) {
            ev[name] = obj;
        } else {
            ev.erase(name);
        }
    }

    // Add the string into the event.  If the string is empty, then any prior value
    // is removed.
    void addEventString(Event& ev,
        const std::string& name, const std::string& val)
    {
        if (val != "") {
            ev[name] = val;
        } else {
            ev.erase(name);
        }
    }

    Event Analytics::CreateTrackEvent(
        const std::string& event,
        const std::string& userId,
        const Object& properties)
    {
        auto ev = createEvent("track");
        addEventString(ev, "event", event);
        addEventString(ev, "userId", userId);
        addEventObject(ev, "properties", properties);
        return (ev);
    }

    Event Analytics::CreateAliasEvent(
        const std::string& previousId,
        const std::string& userId)
    {
        auto ev = createEvent("alias");
        addEventString(ev, "previousId", previousId);
        addEventString(ev, "userId", userId);
        return (ev);
    }

    Event Analytics::CreateIdentifyEvent(
        const std::string& userId,
        const Object& traits)
    {
        auto ev = createEvent("identify");
        addEventString(ev, "userId", userId);
        addEventObject(ev, "traits", traits);
        return (ev);
    }

    Event Analytics::CreateGroupEvent(
        const std::string& groupId,
        const Object& traits)
    {
        auto ev = createEvent("group");
        addEventString(ev, "groupId", groupId);
        addEventObject(ev, "traits", traits);
        return (ev);
    }

    Event Analytics::CreatePageEvent(
        const std::string& name,
        const std::string& userId,
        const Object& properties)
    {
        auto ev = createEvent("page");
        addEventString(ev, "name", name);
        addEventString(ev, "userId", userId);
        addEventObject(ev, "properties", properties);
        return (ev);
    }

    Event Analytics::CreateScreenEvent(
        const std::string& name,
        const std::string& userId,
        const Object& properties)
    {
        auto ev = createEvent("screen");
        addEventString(ev, "name", name);
        addEventString(ev, "userId", userId);
        addEventObject(ev, "properties", properties);
        return (ev);
    }

    Object initContext()
    {
        auto context = json::object();
        auto os = json::object();
        auto lib = json::object();
        std::string osname;
        std::string osvers;

        getOs(osname, osvers);
        os["name"] = osname;
        if (osvers != "") {
            os["version"] = osvers;
        }
        lib["name"] = "analytics-cpp";
        lib["version"] = Version;
        context["os"] = os;
        context["library"] = lib;
        return context;
    }

    Analytics::Analytics(std::string writeKey)
        : writeKey(writeKey)
    {
        host = "https://api.segment.io";
#ifdef SEGMENT_USE_CURL
        Handler = std::make_shared<segment::http::HandlerCurl>();
#elif defined(SEGMENT_USE_WININET)
        Handler = std::make_shared<segment::http::HandlerWinInet>();
#else
        Handler = std::make_shared<segment::http::HandlerNone>();
#endif
        thr = std::thread(worker, this);
        MaxRetries = 5;
        RetryInterval = std::chrono::seconds(1);
        shutdown = false;
        FlushCount = 250;
        FlushSize = 500 * 1024;
        FlushInterval = std::chrono::seconds(10);
        needFlush = false;
        wakeTime = std::chrono::time_point<std::chrono::system_clock>::max();
        Context = initContext();
    }

    Analytics::Analytics(std::string writeKey, std::string host)
        : writeKey(writeKey)
        , host(host)
    {
#ifdef SEGMENT_USE_CURL
        Handler = std::make_shared<segment::http::HandlerCurl>();
#elif defined(SEGMENT_USE_WININET)
        Handler = std::make_shared<segment::http::HandlerWinInet>();
#else
        Handler = std::make_shared<segment::http::HandlerNone>();
#endif

        thr = std::thread(worker, this);
        MaxRetries = 5;
        RetryInterval = std::chrono::seconds(1);
        shutdown = false;
        FlushCount = 250;
        FlushSize = 500 * 1024;
        FlushInterval = std::chrono::seconds(10);
        needFlush = false;
        wakeTime = std::chrono::time_point<std::chrono::system_clock>::max();
        Context = initContext();
    }

    Analytics::~Analytics()
    {
        FlushWait();
        std::unique_lock<std::mutex> lk(this->lock);
        shutdown = true;
        flushCv.notify_one();
        lk.unlock();

        thr.join();
    }

    void Analytics::FlushWait()
    {
        std::unique_lock<std::mutex> lk(this->lock);

        // NB: If an event has been taken off the queue and is being
        // processed, then the lock will be held, preventing us from
        // executing this check.
        while (!events.empty()) {
            needFlush = true;
            flushCv.notify_one();
            emptyCv.wait(lk);
        }
    }

    void Analytics::Flush()
    {
        std::lock_guard<std::mutex> lk(this->lock);
        needFlush = true;
        flushCv.notify_one();
    }

    void Analytics::Scrub()
    {
        std::lock_guard<std::mutex> lk(this->lock);
        events.clear();
        emptyCv.notify_all();
        flushCv.notify_one();
    }

    void Analytics::Track(
        const std::string& userId,
        const std::string& event,
        const Object& properties)
    {
        Track(userId, "", event, properties, nullptr, nullptr);
    }

    void Analytics::Track(
        const std::string& userId,
        const std::string& anonymousId,
        const std::string& event,
        const Object& properties,
        const Object& context,
        const Object& integrations)
    {
        auto ev = CreateTrackEvent(userId, event, properties);
        addEventString(ev, "anonymousId", anonymousId);
        addEventObject(ev, "context", context);
        addEventObject(ev, "integrations", integrations);

        queueEvent(std::move(ev));
    }

    void Analytics::Identify(
        const std::string& userId,
        const Object& traits)
    {
        Identify(userId, "", traits, nullptr, nullptr);
    }

    void Analytics::Identify(
        const std::string& userId,
        const std::string& anonymousId,
        const Object& traits,
        const Object& context,
        const Object& integrations)
    {
        auto ev = CreateIdentifyEvent(userId, traits);
        addEventString(ev, "anonymousId", anonymousId);
        addEventObject(ev, "context", context);
        addEventObject(ev, "integrations", integrations);

        queueEvent(std::move(ev));
    }

    void Analytics::Page(
        const std::string& name,
        const std::string& userId,
        const Object& properties)
    {
        Page(name, userId, "", properties, nullptr, nullptr);
    }

    void Analytics::Page(
        const std::string& name,
        const std::string& userId,
        const std::string& anonymousId,
        const Object& properties,
        const Object& context,
        const Object& integrations)
    {
        auto ev = CreatePageEvent(name, userId, properties);
        addEventString(ev, "anonymousId", anonymousId);
        addEventObject(ev, "context", context);
        addEventObject(ev, "integrations", integrations);

        queueEvent(std::move(ev));
    }
    void Analytics::Screen(
        const std::string& name,
        const std::string& userId,
        const Object& properties)
    {
        Screen(name, userId, "", properties, nullptr, nullptr);
    }

    void Analytics::Screen(
        const std::string& name,
        const std::string& userId,
        const std::string& anonymousId,
        const Object& properties,
        const Object& context,
        const Object& integrations)
    {
        auto ev = CreateScreenEvent(name, userId, properties);
        addEventString(ev, "anonymousId", anonymousId);
        addEventObject(ev, "context", context);
        addEventObject(ev, "integrations", integrations);

        queueEvent(std::move(ev));
    }

    void Analytics::Alias(
        const std::string& previousId,
        const std::string& userId)
    {
        Alias(previousId, userId, "", nullptr, nullptr);
    }

    void Analytics::Alias(
        const std::string& previousId,
        const std::string& userId,
        const std::string& anonymousId,
        const Object& context,
        const Object& integrations)
    {
        auto ev = CreateAliasEvent(previousId, userId);
        addEventString(ev, "anonymousId", anonymousId);
        addEventObject(ev, "context", context);
        addEventObject(ev, "integrations", integrations);

        queueEvent(std::move(ev));
    }

    void Analytics::Group(
        const std::string& groupId,
        const Object& traits)
    {
        // The docs seem to claim that a userId or anonymousId must be set,
        // but the code I've seen suggests otherwise.
        Group(groupId, "", "", traits, nullptr, nullptr);
    }

    void Analytics::Group(
        const std::string& groupId,
        const std::string& userId,
        const std::string& anonymousId,
        const Object& traits,
        const Object& context,
        const Object& integrations)
    {
        auto ev = CreateGroupEvent(groupId, traits);
        addEventString(ev, "anonymousId", anonymousId);
        addEventObject(ev, "context", context);
        addEventObject(ev, "integrations", integrations);

        queueEvent(std::move(ev));
    }

    void Analytics::PostEvent(Event ev)
    {
        queueEvent(std::move(ev));
    }

    // This implementation of base64 is taken from StackOverflow:
    // https://stackoverflow.com/questions/180947/base64-decode-snippet-in-c
    // We use it here perform the base64 encode for user authentication with
    // Basic Auth, freeing the transport providers from dealing with this.
    // (Note that libcurl has this builtin though.)
    static std::string base64_encode(const std::string& in)
    {
        std::string out;

        int val = 0, valb = -6;
        for (unsigned char c : in) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        if (valb > -6)
            out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val << 8) >> (valb + 8)) & 0x3F]);
        while (out.size() % 4)
            out.push_back('=');
        return out;
    }

    void Analytics::sendBatch()
    {
        segment::http::Request req;
        // XXX add default context or integrations?

        auto tmstamp = TimeStamp();
        // Update the time on the elements of the batch.  We do this
        // on each new attempt, since we're trying to synchronize our clock
        // with the server's.
        for (auto ev : batch) {
            ev["sentAt"] = tmstamp;
        }

        json body;
        body["batch"] = batch;
        if (Integrations.is_object()) {
            body["integrations"] = Integrations;
        }
        if (Context.is_object()) {
            body["context"] = Context;
        }

        req.Method = "POST";
        req.URL = this->host + "/v1/batch";
        req.Headers["User-Agent"] = "AnalyticsCPP/0.1";
        // Note: libcurl could do this for us, but for other transports
        // we do it here -- keeping the transports as unaware as possible.
        req.Headers["Authorization"] = "Basic " + base64_encode(this->writeKey + ":");
        req.Headers["Content-Type"] = "application/json";
        req.Headers["Accept"] = "application/json";
        req.Body = body.dump();

        auto resp = this->Handler->Handle(req);
        if (resp->Code != 200) {
            throw(segment::http::Error(resp->Code));
        }
    }

    void Analytics::queueEvent(Event ev)
    {
        std::lock_guard<std::mutex> lk(lock);
        events.push_back(std::move(ev));
        if (events.size() == 1) {
            flushTime = std::chrono::system_clock::now() + FlushInterval;
            if (flushTime < wakeTime) {
                wakeTime = flushTime;
            }
        }
        flushCv.notify_one();
    }

    void Analytics::processQueue()
    {
        int fails = 0;
        bool ok;
        std::string reason;
        std::deque<Event> notifyq;
        std::unique_lock<std::mutex> lk(this->lock);

        for (;;) {
            // There is a little mystery here.  Without this sleep,
            // the Win32 system seems to get stuck; perhaps there is
            // a subtle bug in the handling of condition variables
            // or locks in the C++ runtime or threading libraries.
            // POSIX systems don't seem to need it, but 10 millis
            // is no sweat.
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            if (events.empty() && batch.empty()) {
                // Reset failure count so we start with a clean slate.
                // Otherwise we could have a failure hours earlier that
                // allows only one failed post hours later.
                fails = 0;
                wakeTime = std::chrono::time_point<std::chrono::system_clock>::max();

                // We might have a flusher waiting
                emptyCv.notify_all();

                // We only shut down if the queue was empty.  To force
                // a shutdown without draining, just clear the queue
                // independently.
                if (shutdown) {
                    return;
                }

                flushCv.wait(lk);
                continue;
            }

            // Gather up new items into the batch, assuming that the batch
            // is not already full.
            while ((!events.empty()) && (batch.size() < FlushCount)) {
                json j;

                // Try adding an event to the batch and checking the
                // serialization...  This is pretty ineffecient as we
                // serialize the objects multiple times, but it's easier
                // to understand.  Later look at saving the last size,
                // and only adding the size of the serialized event.

                auto ev = events.front();
                batch.push_back(ev);
                j["batch"] = batch;
                if (j.dump().size() >= FlushSize) {
                    batch.pop_back(); // remove what we just added.
                    needFlush = true;
                    break;
                }
                // We inclucded this event, so remove it from the queue
                // (it is already inthe batch.)
                events.pop_front();
            }

            // We hit the limit.
            if (batch.size() >= FlushCount) {
                needFlush = true;
            }

            auto now = std::chrono::system_clock::now();
            auto tmstamp = date::format("%FT%TZ",
                std::chrono::time_point_cast<std::chrono::milliseconds>(now));

            if ((!needFlush) && (now < wakeTime)) {
                flushCv.wait_until(lk, wakeTime);
                continue;
            }

            // We're trying to flush, so clear our "need".
            needFlush = false;

            try {
                sendBatch();
                ok = true;
                fails = false;
            } catch (std::exception& e) {
                if (fails < MaxRetries) {
                    // Something bad happened.  Let's wait a bit and
                    // try again later.  We return this even to the
                    // front.
                    fails++;
                    retryTime = now + RetryInterval;
                    if (retryTime < wakeTime) {
                        wakeTime = retryTime;
                    }
                    flushCv.wait_until(lk, wakeTime);
                    continue;
                }
                ok = false;
                reason = e.what();
                // We intentionally have chosen not to reset the failure
                // count.  Which means if we wind up failing to send one
                // event after maxtries, we only try each of the following
                // one time, until either the queue is empty or we have
                // a success.
            }
            auto cb = Callback;
            notifyq = batch;
            batch.clear();
            lk.unlock();

            while (!notifyq.empty()) {
                auto ev = notifyq.front();
                notifyq.pop_front();
                try {
                    if (cb != nullptr) {
                        if (ok) {
                            cb->Success(ev);
                        } else {
                            cb->Failure(ev, reason);
                        }
                    }
                } catch (std::exception&) {
                    // User supplied callback code failed.  There isn't really
                    // anything else we can do.  Muddle on.  This prevents a
                    // failure there from silently causing the processing thread
                    // to stop functioning.
                }
            }

            lk.lock();
        }
    }

    void Analytics::worker(Analytics* self)
    {
        self->processQueue();
    }

} // namespace analytics
} // namespace segment
