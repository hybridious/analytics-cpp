//
// Copyright 2017 Segment Inc. <friends@segment.com>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

#include <chrono>
#include <condition_variable>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "http.hpp"
#include "json.hpp"

#ifndef SEGMENT_ANALYTICS_HPP_
#define SEGMENT_ANALYTICS_HPP_

namespace segment {
namespace analytics {

    /// Object represents a JSON object.  It supports very flexible
    /// map style operations, etc.  In the places where this is used
    /// we validate the entity is actually an Object.
    using Object = nlohmann::json;

    // Events are just JSON objects under the hood.
    using Event = Object;

#if 0
    class Event {
    public:
        Event(
            std::string type, /// Event type, like "alias"
            std::string userId, /// userId, or empty
            std::string anonymousId, /// anonymousId, or empty
            Object context, /// context, or null
            Object integrations /// integrations, or null
            );

        ~Event(){};
        Object object;
    };
#endif

    /// TimeStamp is a convenience function that returns the current system
    /// time in ISO-8601 format.  It will include fractional times to the
    /// precision of the system clock.
    std::string TimeStamp();

    /// Callback is the base class for analytics event callbacks.
    /// This should be subclassed, and an instance stored in the Analytics
    /// object, if necessary.  The default implementation does nothing.
    class Callback {
    public:
        virtual ~Callback(){};
        /// Success is called when the Event has successfully been uploaded
        /// to Segment.
        virtual void Success(const Event&) = 0;

        /// Failure is called when the Event was unable to be uploaded.
        /// @param ev [in] An incoming event.
        /// @param reason [in] Some reason why the failure occurrecd.
        virtual void Failure(const Event& ev, const std::string& reason) = 0;
    };

    /// Analytics is the main object for accessing Segment's Analytics
    /// services; think of it as a handle or client object used to talk
    /// to Segment's servers.
    class Analytics {
    public:
        Analytics(std::string writeKey);
        Analytics(std::string writeKey, std::string host);
        ~Analytics();

        /// Flush flushes events to the server.  This just wakes up the
        /// asynchronous background thread, and starts it sending events
        /// to the server; it will process all events in the queue, unless
        /// an error occurs.
        void Flush();

        /// FlushWait flushes the queue, and waits for it to empty.  This
        /// should be called upon program exit; the destructor calls it
        /// automatically.  This can mean that it may take some time
        /// to destroy this object.
        void FlushWait();

        /// Scrub deletes all events that are queued for processing.
        /// If an immediate exit is required, call this first.  This
        /// method should be called with caution, as it generally will
        /// lead to lost events.
        void Scrub();

        /// Handler is the backend HTTP transport handler.  The constructor
        /// will initialize a default based upon compile time operations.
        std::shared_ptr<segment::http::Handler> Handler;

        /// Callback is a callback object that wlll be called to inform
        /// the caller of success or failure of posting events to the
        /// service.
        std::shared_ptr<segment::analytics::Callback> Callback;

        /// MaxRetries represents the maximum number of retries to perform
        /// posting an event, before giving up.  The failure will not be
        /// reported until all retries are exhausted.
        int MaxRetries;

        /// FlushCount is the maximum number of messages to hold before flushing.
        /// Changing this value is not recommended.
        size_t FlushCount;

        /// FlushSize is the upper bound on batch size before we flush to the
        /// network.  This is actuallly a count in bytes; the entire marshalled
        /// object size is considered.  Note that this implemnentation uses
        /// compact encoding by default.
        size_t FlushSize;

        /// FlushInterval indicates how long we wait to collect messages to
        /// send them in a single batch.  We will upload a batch of messages
        /// whenever we have enough data to meet FlushCount, this much time
        /// has passed, or an explicit Flush or FlushWait is called (or this
        /// object is destroyed.)
        std::chrono::seconds FlushInterval;

        /// The amount of time to wait before retrying a post.
        std::chrono::seconds RetryInterval;

        /// Default context. We populate a default context with the
        /// library and operating system.  This will be merged against
        /// any other more detail context you might wish to set.
        Object Context;

        /// Default integrations. This must be a dictionary of string
        /// keys to booleans.  (A JSON object where all values are booleans.)
        Object Integrations;

        // With each of these functions, if you need to use an anonymous ID
        // instead of a user ID, just pass the empty string for the user ID
        // and set the anonymous ID after.

        Event CreateTrackEvent(
            const std::string& event,
            const std::string& userId,
            const Object& properties = nullptr);

        Event CreateAliasEvent(
            const std::string& previousId,
            const std::string& userId);

        Event CreateIdentifyEvent(
            const std::string& userId,
            const Object& traits = nullptr);

        Event CreateGroupEvent(
            const std::string& groupId,
            const Object& traits);

        Event CreatePageEvent(
            const std::string& name,
            const std::string& userId,
            const Object& properties = nullptr);

        Event CreateScreenEvent(
            const std::string& name,
            const std::string& userId,
            const Object& properties = nullptr);

        void SetEventAnonymousId(Event&, const std::string&);
        void SetEventIntegrations(Event&, const Object&);
        void SetEventContext(Event&, const Object&);
        void SetEventTimeStamp(Event&, const std::string&);

        void PostEvent(Event);

        void Track(
            const std::string& userId,
            const std::string& event,
            const Object& properties = nullptr);

        void Track(
            const std::string& userId,
            const std::string& anonymousId,
            const std::string& event,
            const Object& properties,
            const Object& context,
            const Object& integrations);

        void Identify(
            const std::string& userId,
            const Object& traits = nullptr);

        void Identify(
            const std::string& userId,
            const std::string& anonymousId,
            const Object& traits,
            const Object& context,
            const Object& integrations);

        void Page(
            const std::string& name,
            const std::string& userId,
            const Object& properties = nullptr);

        void Page(const std::string& name,
            const std::string& userId,
            const std::string& anonymousId,
            const Object& properties,
            const Object& context,
            const Object& integrations);

        void Screen(
            const std::string& name,
            const std::string& userId,
            const Object& properties = nullptr);

        void Screen(
            const std::string& name,
            const std::string& userId,
            const std::string& anonymousId,
            const Object& properties,
            const Object& context,
            const Object& integrations);

        void Alias(
            const std::string& previousId,
            const std::string& userId);

        void Alias(
            const std::string& previousId,
            const std::string& userId,
            const std::string& anonymousId,
            const Object& context,
            const Object& integrations);

        void Group(
            const std::string& groupId,
            const Object& traits = nullptr);

        void Group(
            const std::string& groupId,
            const std::string& userId,
            const std::string& anonymousId,
            const Object& traits,
            const Object& context,
            const Object& integrations);

    private:
        std::string writeKey;
        std::string host;

        std::mutex lock;
        std::condition_variable emptyCv;
        std::condition_variable flushCv;
        std::thread thr;
        std::deque<Event> events;
        std::deque<Event> batch;
        std::chrono::system_clock::time_point flushTime;
        std::chrono::system_clock::time_point retryTime;
        std::chrono::system_clock::time_point wakeTime;

        bool needFlush;
        bool shutdown;

        void sendBatch();
        void queueEvent(Event);
        void processQueue();
        static void worker(Analytics*);
    };

} // namespace analytics
} // namespace segment

#endif // SEGMENT_ANALYTICS_HPP_