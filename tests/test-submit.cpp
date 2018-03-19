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

#ifdef SEGMENT_USE_CURL
#include <curl/curl.h> // So that we can clean up memory at the end.
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
        auto writeKey = "LpSP8WJmW312Z0Yj1wluUcr76kd4F0xl";
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
        auto writeKey = "LpSP8WJmW312Z0Yj1wluUcr76kd4F0xl";
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
        auto writeKey = "LpSP8WJmW312Z0Yj1wluUcr76kd4F0xl";
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
        auto writeKey = "LpSP8WJmW312Z0Yj1wluUcr76kd4F0xl";
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
        auto writeKey = "LpSP8WJmW312Z0Yj1wluUcr76kd4F0xl";
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
        auto writeKey = "LpSP8WJmW312Z0Yj1wluUcr76kd4F0xl";
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
