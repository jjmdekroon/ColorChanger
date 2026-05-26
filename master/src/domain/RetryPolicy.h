#pragma once
#include "Config.h"

/**
 * RetryPolicy: pure backoff helpers (T061)
 * FR-011:  Feed retry 3× with 1000/2000/4000 ms backoff
 * FR-017:  Bus retry 3× with 10/20/40 ms backoff
 *
 * All functions are pure/constexpr — no Arduino dependencies.
 * Values come from master/include/Config.h constants.
 */
namespace RetryPolicy {

/// Number of bus retries (FR-017)
constexpr uint8_t MAX_BUS_RETRIES = ::MAX_BUS_RETRIES;

/// Number of feed retries (FR-011)
constexpr uint8_t MAX_FEED_RETRIES = ::MAX_FEED_RETRIES;

/**
 * Return the bus-retry backoff in ms for the given attempt (0-indexed).
 * attempt=0 → 10 ms, attempt=1 → 20 ms, attempt=2 → 40 ms.
 * Out-of-bounds clamped to last value (40 ms).
 */
inline uint32_t nextBusBackoff(uint8_t attempt) {
    constexpr uint8_t N = 3;
    uint8_t idx = (attempt < N) ? attempt : (N - 1);
    return ::BUS_RETRY_BACKOFF_MS[idx];
}

/**
 * Return the feed-retry backoff in ms for the given attempt (0-indexed).
 * attempt=0 → 1000 ms, attempt=1 → 2000 ms, attempt=2 → 4000 ms.
 * Out-of-bounds clamped to last value (4000 ms).
 */
inline uint32_t nextFeedBackoff(uint8_t attempt) {
    constexpr uint8_t N = 3;
    uint8_t idx = (attempt < N) ? attempt : (N - 1);
    return ::FEED_RETRY_BACKOFF_MS[idx];
}

/**
 * Return true if another bus retry is allowed.
 * attempt is the count of retries already attempted (0 = no retries yet).
 */
inline bool shouldRetryBus(uint8_t attempt) {
    return attempt < MAX_BUS_RETRIES;
}

/**
 * Return true if another feed retry is allowed.
 * attempt is the count of retries already attempted (0 = no retries yet).
 */
inline bool shouldRetryFeed(uint8_t attempt) {
    return attempt < MAX_FEED_RETRIES;
}

}  // namespace RetryPolicy
