//
// Copyright 2017 Segment Inc. <friends@segment.com>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

#include "analytics.hpp"

#include <condition_variable>
#include <mutex>
#include <thread>
#include <ctime>

#if defined(SEGMENT_USE_WININET)
#   include "http-wininet.hpp"
#elif defined(SEGMENT_USE_CURL)
#   include <curl/curl.h> // So that we can clean up memory at the end.
#   include "http-curl.hpp"
#else
#   include "http-none.hpp"
#endif

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

using namespace segment::analytics;

class myTestCB : public Callback {
public:
    void Success(const Event& ev)
    {
        std::lock_guard<std::mutex> l(lk);
        success++;
        wake();
    }

    void Failure(const Event& ev, const std::string& reason)
    {
        std::lock_guard<std::mutex> l(lk);
        last_reason = reason;
        fail++;
        wake();
    }

    void wake()
    {
        count++;
        cv.notify_all();
    }

    void Wait(int num = 1)
    {
        std::unique_lock<std::mutex> l(lk);

        while (count < num) {
            cv.wait(l);
        }
    }

    int count;
    std::mutex lk;
    std::condition_variable cv;
    int success;
    int fail;
    std::string last_reason;
};

TEST_CASE("Submissions to Segment work", "[analytics]")
{
    GIVEN("A valid writeKey")
    {
        auto writeKey = "LmiGFAvSuRLBgIpFzj9pMzhMDXRpvdt7";
        auto apiHost = "https://api.segment.io";

        THEN("we can submit tracked events")
        {
            auto cb = std::make_shared<myTestCB>();
            Analytics analytics(writeKey, apiHost);
            analytics.MaxRetries = 0;
            analytics.FlushCount = 1;
            analytics.Callback = cb;

            analytics.Track("humptyDumpty", "Sat On A Wall", { { "crown", "broken" }, { "kingsHorses", "NoHelp" }, { "kingsMen", "NoHelp" } });
            cb->Wait();
            analytics.FlushWait();
            REQUIRE(cb->fail == 0);
        }
    }

    GIVEN("Batching tests")
    {
        auto writeKey = "LmiGFAvSuRLBgIpFzj9pMzhMDXRpvdt7";
        auto apiHost = "https://api.segment.io";

        THEN("we can submit tracked events")
        {
            auto cb = std::make_shared<myTestCB>();
            Analytics analytics(writeKey, apiHost);
            analytics.MaxRetries = 0;
            analytics.Callback = cb;
            analytics.FlushInterval = std::chrono::seconds(3);

            analytics.Track("batch1", "First", { { "abc", "def" } });
            analytics.Track("batch2", "Second", { { "abc", "234" } });
            std::this_thread::sleep_for(std::chrono::seconds(1));
            analytics.Track("batch3", "Third", { { "abc", "567" } });
            //            analytics.Flush();
            cb->Wait(3);
            analytics.FlushWait();
            REQUIRE(cb->fail == 0);
            REQUIRE(cb->success == 3);
        }
    }

    GIVEN("Flushed events")
    {
        auto writeKey = "LmiGFAvSuRLBgIpFzj9pMzhMDXRpvdt7";
        auto apiHost = "https://api.segment.io";

        THEN("we can submit tracked events")
        {
            auto cb = std::make_shared<myTestCB>();
            Analytics analytics(writeKey, apiHost);
            analytics.MaxRetries = 0;
            analytics.Callback = cb;
            analytics.FlushInterval = std::chrono::seconds(3);

            analytics.Track("flush1", "Nanny", { { "abc", "def" } });
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            analytics.Track("flush2", "Charles", { { "abc", "234" } });
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            analytics.Track("flush3", "Flushing", { { "abc", "567" } });
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            analytics.Flush();

            cb->Wait(3);
            REQUIRE(cb->success == 3);
            REQUIRE(cb->fail == 0);
        }
    }

    GIVEN("A bogus URL")
    {
        auto writeKey = "LmiGFAvSuRLBgIpFzj9pMzhMDXRpvdt7";
        auto apiHost = "https://api.segment.io/nobodyishome";

        THEN("gives a 404 HttpException")
        {
            auto cb = std::make_shared<myTestCB>();
            Analytics analytics(writeKey, apiHost);
            analytics.MaxRetries = 0;
            analytics.FlushCount = 1;
            analytics.Callback = cb;

            analytics.Track("bogosURL", "Did Something", { { "foo", "bar" } });

            cb->Wait();
            analytics.FlushWait();
            REQUIRE_THAT(cb->last_reason, Catch::Contains("404"));
            REQUIRE(cb->fail == 1);
        }
    }

#if 0
    // Apparently the remote server does not validate the writeKey!
    GIVEN("A bogus writeKey")
    {
        auto writeKey = "youShallNotPass!";
        auto apiHost = "https://api.segment.io";
        Analytics* analytics = new Analytics(writeKey, apiHost);

        THEN("we can submit tracked events")
        {
            analytics->Track("userId", "Did Something",
                { { "foo", "bar" }, { "qux", "mux" } });
        }
    }
#endif

    GIVEN("Localhost (random port)")
    {
        auto writeKey = "LmiGFAvSuRLBgIpFzj9pMzhMDXRpvdt7";
        auto apiHost = "https://localhost:50051";
        THEN("Connection is refused")
        {
            auto cb = std::make_shared<myTestCB>();
            Analytics analytics(writeKey, apiHost);
            analytics.MaxRetries = 0;
            analytics.FlushCount = 1;
            analytics.Callback = cb;

            analytics.Track("userId", "Did Something", { { "foo", "bar" }, { "qux", "mux" } });
            cb->Wait();
            analytics.FlushWait();
            REQUIRE(cb->fail == 1);
            REQUIRE(cb->last_reason == "Connection refused");
        }
    }
}

extern "C"
{
#ifdef WIN32
#include <Rpc.h>
#else
#include <uuid/uuid.h>
#endif
}

std::string newUUID()
{
#ifdef WIN32
    UUID uuid;
    UuidCreate ( &uuid );

    unsigned char * str;
    UuidToStringA ( &uuid, &str );

    std::string s( ( char* ) str );

    RpcStringFreeA ( &str );
#else
    uuid_t uuid;
    uuid_generate_random ( uuid );
    char s[37];
    uuid_unparse ( uuid, s );
#endif
    return s;
}

std::string datetime_now()
{
    std::stringstream ss;
    time_t cur_time = time(nullptr);
    ss << ctime(&cur_time);

    return ss.str();
}

TEST_CASE("Action Tests", "[analytics]")
{
    GIVEN("Analytics object with default configuration")
    {
        auto writeKey = "LmiGFAvSuRLBgIpFzj9pMzhMDXRpvdt7";
        auto apiHost = "https://api.segment.io";

        auto cb = std::make_shared<myTestCB>();
        Analytics analytics(writeKey, apiHost);
        analytics.MaxRetries = 0;
        analytics.FlushCount = 1;
        analytics.Callback = cb;

        Object properties = {
            { "Success", true },
            { "When", datetime_now() }
        };

        Object traits = {
            { "Subscription Plan", "Free" },
            { "Friends", 30 },
            { "Joined", datetime_now() },
            { "Cool", true },
            { "Company", { { "name", "Initech, Inc " } } },
            { "Revenue", 40.32 },
            { "Don't Submit This, Kids", "Unauthorized Access" }
        };

        Object context = {
            { "ip", "12.212.12.49" },
            { "language", "en-us" }
        };

        Object integrations = {
            { "all", false },
            { "Mixpanel", true },
            { "Salesforce", true }
        };

        WHEN("send identify message to server")
        {
            analytics.Identify("user", newUUID(), traits, context, integrations);

            THEN("no failing response from server")
            {
                cb->Wait();
                analytics.FlushWait();
                REQUIRE(cb->fail == 0);
            }
        }

        WHEN("send track message to server")
        {
            analytics.Track("user", newUUID(), "Ran cpp test", properties, context, integrations);

            THEN("no failing response from server")
            {
                cb->Wait();
                analytics.FlushWait();
                REQUIRE(cb->fail == 0);
            }
        }

        WHEN("send alias message to server")
        {
            analytics.Alias("previousId", "to");

            THEN("no failing response from server")
            {
                cb->Wait();
                analytics.FlushWait();
                REQUIRE(cb->fail == 0);
            }
        }

        WHEN("send group message to server")
        {
            analytics.Group("group", "user", newUUID(), traits, context, integrations);

            THEN("no failing response from server")
            {
                cb->Wait();
                analytics.FlushWait();
                REQUIRE(cb->fail == 0);
            }
        }

        WHEN("send page message to server")
        {
            analytics.Page("name", "user", newUUID(), properties, context, integrations);

            THEN("no failing response from server")
            {
                cb->Wait();
                analytics.FlushWait();
                REQUIRE(cb->fail == 0);
            }
        }

        WHEN("send screen message to server")
        {
            analytics.Screen("name", "user", newUUID(), properties, context, integrations);

            THEN("no failing response from server")
            {
                cb->Wait();
                analytics.FlushWait();
                REQUIRE(cb->fail == 0);
            }
        }

        WHEN("send screen message with null option")
        {
            analytics.Screen("bar", "qaz", "", nullptr, nullptr, nullptr);

            THEN("no failing response from server")
            {
                cb->Wait();
                analytics.FlushWait();
                REQUIRE(cb->fail == 0);
            }
        }

        WHEN("send multiple messages asynchronously")
        {
            int trials = 10;

            for (int i = 0; i < trials; i++)
            {
                switch (std::rand() % 6)
                {
                case 0:
                    analytics.Identify("user", newUUID(), traits, context, integrations);
                    break;
                case 1:
                    analytics.Track("user", newUUID(), "Ran cpp test", properties, context, integrations);
                    break;
                case 2:
                    analytics.Alias("previousId", "to", newUUID(), context, integrations);
                    break;
                case 3:
                    analytics.Group("group", "user", newUUID(), traits, context, integrations);
                    break;
                case 4:
                    analytics.Page("name", "user", newUUID(), properties, context, integrations);
                    break;
                case 5:
                    analytics.Screen("name", "user", newUUID(), properties, context, integrations);
                    break;
                }
            }

            THEN("no failing response from server")
            {
                cb->Wait(trials);
                analytics.FlushWait();
                REQUIRE(cb->fail == 0);
            }
        }
    }
}

TEST_CASE("E2E Test", "[analytics]")
{
    GIVEN("Analytics object with runscope token")
    {
        // Segment Write Key from https://app.segment.com/segment-libraries/sources/analytics_cpp_e2e_test/overview
        auto writeKey = "LmiGFAvSuRLBgIpFzj9pMzhMDXRpvdt7";
        auto apiHost = "https://api.segment.io";

        std::string runscopeBucket = "ptvhfe8q5b24";
        std::string runscopeToken = std::getenv("RUNSCOPE_TOKEN");
        std::string runscopeHost = "https://api.runscope.com";

        auto anonymousId = newUUID();

        Object properties = {
            { "Success", true },
            { "When", datetime_now() }
        };

        Object context = {
            { "ip", "12.212.12.49" },
            { "language", "en-us" }
        };

        Object integrations = {
            { "all", false },
            { "Mixpanel", true },
            { "Salesforce", true }
        };

        WHEN("Send events to a Runscope bucket used by this test")
        {
            auto cb = std::make_shared<myTestCB>();
            Analytics analytics(writeKey, apiHost);
            analytics.MaxRetries = 0;
            analytics.FlushCount = 1;
            analytics.Callback = cb;


            analytics.Track("prateek", anonymousId, "Item Purchased", properties, context, integrations);

            THEN("no failing response from server")
            {
                cb->Wait();
                analytics.FlushWait();
                REQUIRE(cb->fail == 0);

                // Give some time for events to be delivered from the API to destinations.
                std::this_thread::sleep_for(std::chrono::seconds(5));

                std::shared_ptr<segment::http::Handler> Handler;
#ifdef SEGMENT_USE_CURL
                Handler = std::make_shared<segment::http::HandlerCurl>();
#elif defined(SEGMENT_USE_WININET)
                Handler = std::make_shared<segment::http::HandlerWinInet>();
#else
                Handler = std::make_shared<segment::http::HandlerNone>();
#endif
                auto messageUrl = runscopeHost + "/buckets/" + runscopeBucket + "/messages";

                segment::http::Request req;
                req.Method = "GET";
                req.Headers["Authorization"] = "Bearer " + runscopeToken;

                bool message_found = false;
                for (int i = 0; i < 5; i++)
                {
                    req.URL = messageUrl + "?count=20";

                    auto res = Handler->Handle(req);
                    auto data = Object::parse(res->Body);

                    std::vector<Object> messages;
                    for (auto item : data["data"])
                    {
                        req.URL = messageUrl + "/" + item["uuid"].get<std::string>();
                        res = Handler->Handle(req);

                        auto messageData = Object::parse(res->Body);
                        messages.push_back(messageData["data"]["request"]["body"]);
                    }

                    for (auto m : messages)
                    {
                        auto msg = Object::parse(m.get<std::string>());
                        if (msg["anonymousId"].get<std::string>() == anonymousId)
                        {
                            message_found = true;
                            break;
                        }
                    }

                    if (message_found)
                        break;
                    else
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                }

                REQUIRE(message_found);
            }
        }
    }
}

int main(int argc, char* argv[])
{
#if defined(SEGMENT_USE_CURL)
    // Technically we don't have to init curl, but doing so ensures that
    // any global libcurl leaks are blamed on libcurl in valgrind runs.
    curl_global_init(CURL_GLOBAL_DEFAULT);
#endif

    int result = Catch::Session().run(argc, argv);

#if defined(SEGMENT_USE_CURL)
    curl_global_cleanup();
#endif
    return (result < 0xff ? result : 0xff);
}
