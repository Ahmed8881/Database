#ifndef DATA_UTILS_H
#define DATA_UTILS_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Date utilities
typedef struct {
    int year;   // Year (e.g., 2023)
    int month;  // Month (1-12)
    int day;    // Day (1-31)
} Date;

// Time utilities
typedef struct {
    int hour;   // Hour (0-23)
    int minute; // Minute (0-59)
    int second; // Second (0-59)
} Time;

// Timestamp = Date + Time
typedef struct {
    Date date;
    Time time;
} Timestamp;

// Convert Date to int32_t (days since epoch 1970-01-01)
int32_t date_to_int32(const Date* date);

// Convert int32_t to Date (days since epoch 1970-01-01)
Date int32_to_date(int32_t days);

// Convert Time to int32_t (seconds since midnight)
int32_t time_to_int32(const Time* time);

// Convert int32_t to Time (seconds since midnight)
Time int32_to_time(int32_t seconds);

// Convert Timestamp to int64_t (seconds since epoch)
int64_t timestamp_to_int64(const Timestamp* ts);

// Convert int64_t to Timestamp (seconds since epoch)
Timestamp int64_to_timestamp(int64_t seconds);

// Get current date
Date get_current_date();

// Get current time
Time get_current_time();

// Get current timestamp
Timestamp get_current_timestamp();

// Format date as string (YYYY-MM-DD)
void format_date(const Date* date, char* buffer, size_t buffer_size);

// Format time as string (HH:MM:SS)
void format_time(const Time* time, char* buffer, size_t buffer_size);

// Format timestamp as string (YYYY-MM-DD HH:MM:SS)
void format_timestamp(const Timestamp* ts, char* buffer, size_t buffer_size);

// Parse date from string (YYYY-MM-DD)
bool parse_date(const char* str, Date* date);

// Parse time from string (HH:MM:SS)
bool parse_time(const char* str, Time* time);

// Parse timestamp from string (YYYY-MM-DD HH:MM:SS)
bool parse_timestamp(const char* str, Timestamp* ts);

#endif // DATA_UTILS_H
