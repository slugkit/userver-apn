#include <apn/types/credentials.hpp>
#include <apn/types/notification.hpp>
#include <apn/types/result.hpp>

#include <userver/utest/utest.hpp>

UTEST(ApnNotification, DefaultValues) {
    apn::Notification n;
    EXPECT_EQ(n.push_type, "alert");
    EXPECT_EQ(n.priority, 10);
    EXPECT_EQ(n.expiration, 0);
    EXPECT_TRUE(n.collapse_id.empty());
    EXPECT_TRUE(n.topic.empty());
}

UTEST(ApnCredentials, DefaultSandbox) {
    apn::Credentials c;
    EXPECT_FALSE(c.use_sandbox);
}

UTEST(ApnSendResult, Fields) {
    apn::SendResult r{.status_code = 200, .apns_id = "uuid-123", .reason = ""};
    EXPECT_EQ(r.status_code, 200);
    EXPECT_EQ(r.apns_id, "uuid-123");
    EXPECT_TRUE(r.reason.empty());
}
