#include <unity.h>

void test_dummy(void)
{
  // 何もしない
  TEST_ASSERT_TRUE(true);
}

void setup()
{
  UNITY_BEGIN();
  RUN_TEST(test_dummy);
  UNITY_END();
}

void loop()
{
  // テストループは空
}
