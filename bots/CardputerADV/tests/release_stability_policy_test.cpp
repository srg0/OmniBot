#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../src/release_stability_policy.h"

int main() {
  unsigned checks = 0;

  assert(!release_stability::shouldStopOrdinaryRecording(false, 1000U, 0U, 1000U));
  assert(!release_stability::shouldStopOrdinaryRecording(true, 999U, 0U, 1000U));
  assert(release_stability::shouldStopOrdinaryRecording(true, 1000U, 0U, 1000U));
  assert(release_stability::shouldStopOrdinaryRecording(true, 1001U, 0U, 1000U));
  assert(!release_stability::shouldStopOrdinaryRecording(true, 1000U, 0U, 0U));
  checks += 5;

  const uint32_t wrapStart = 0xfffffff0U;
  assert(!release_stability::shouldStopOrdinaryRecording(true, 0x0000000eU, wrapStart, 31U));
  assert(release_stability::shouldStopOrdinaryRecording(true, 0x0000000fU, wrapStart, 31U));
  checks += 2;

  assert(!release_stability::otaUpdateAvailable(false, "0.2.141-dev", "0.2.142-dev"));
  assert(!release_stability::otaUpdateAvailable(true, "0.2.141-dev", ""));
  assert(!release_stability::otaUpdateAvailable(true, "0.2.141-dev", "0.2.141-dev"));
  assert(!release_stability::otaUpdateAvailable(true, "0.2.142-dev", "0.2.141-dev"));
  assert(!release_stability::otaUpdateAvailable(true, "broken", "0.2.142-dev"));
  assert(release_stability::otaUpdateAvailable(true, "0.2.141-dev", "0.2.142-dev"));
  assert(release_stability::otaUpdateAvailable(true, "0.2.999-dev", "0.3.0-dev"));
  checks += 7;

  printf("release_stability_policy_checks=%u\n", checks);
  return 0;
}
