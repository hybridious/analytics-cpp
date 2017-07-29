//
// Copyright 2017 Segment Inc. <friends@segment.com>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

#include "analytics.hpp"

`using namespace segment::analytics;

int main()
{
    auto writeKey = "<your-custom-write-key-here>";
    auto apiHost = "https://api.segment.io";
    Analytics analytics(writeKey, apiHost);
    analytics.Track("joeUser", "Did Something", { { "foo", "bar" }, { "qux", "mux" } });
    return 0;
}
