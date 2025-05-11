#include "../include/data_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Number of days in each month (non-leap year)
static const int days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// Check if a year is a leap year
static bool is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// Get number of days in a month, accounting for leap years
static int get_days_in_month(int year, int month) {
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }
    return days_in_month[month];
}

// Calculate day of year (1-366)
static int day_of_year(int year, int month, int day) {
    int result = day;
    for (int i = 1; i < month; i++) {
        result += (i == 2 && is_leap_year(year)) ? 29 : days_in_month[i];
    }
    return result;
}

// Convert Date to int32_t (days since epoch 1970-01-01)
int32_t date_to_int32(const Date* date) {
    // Start with days from years
    int32_t days = 0;
    
    // Count days for complete years
    for (int y = 1970; y < date->year; y++) {
        days += is_leap_year(y) ? 366 : 365;
    }
    
    // Add days from current year
    days += day_of_year(date->year, date->month, date->day) - 1; // -1 because Jan 1 is day 0 from epoch
    
    return days;
}

// Convert int32_t to Date (days since epoch 1970-01-01)
Date int32_to_date(int32_t days) {
    Date date;
    int32_t remaining_days = days;
    
    // Start at epoch
    date.year = 1970;
    
    // Count up years until we have fewer days than in a year
    while (true) {
        int days_in_year = is_leap_year(date.year) ? 366 : 365;
        if (remaining_days < days_in_year) {
            break;
        }
        remaining_days -= days_in_year;
        date.year++;
    }
    
    // Now count up months
    date.month = 1;
    while (true) {
        int days_in_this_month = (date.month == 2 && is_leap_year(date.year)) ? 29 : days_in_month[date.month];
        if (remaining_days < days_in_this_month) {
            break;
        }
        remaining_days -= days_in_this_month;
        date.month++;
    }
    
    // Remaining days plus 1 is the day of month
    date.day = remaining_days + 1;
    
    return date;
}

// Convert Time to int32_t (seconds since midnight)
int32_t time_to_int32(const Time* time) {
    return time->hour * 3600 + time->minute * 60 + time->second;
}

// Convert int32_t to Time (seconds since midnight)
Time int32_to_time(int32_t seconds) {
    Time time;
    time.hour = seconds / 3600;
    seconds %= 3600;
    time.minute = seconds / 60;
    time.second = seconds % 60;
    return time;
}

// Convert Timestamp to int64_t (seconds since epoch)
int64_t timestamp_to_int64(const Timestamp* ts) {
    int64_t seconds = date_to_int32(&ts->date) * 86400; // Days to seconds
    seconds += time_to_int32(&ts->time);
    return seconds;
}

// Convert int64_t to Timestamp (seconds since epoch)
Timestamp int64_to_timestamp(int64_t seconds) {
    Timestamp ts;
    
    // Split into days and seconds within day
    int32_t days = (int32_t)(seconds / 86400);
    int32_t secs = (int32_t)(seconds % 86400);
    
    ts.date = int32_to_date(days);
    ts.time = int32_to_time(secs);
    
    return ts;
}

// Get current date
Date get_current_date() {
    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    
    Date date;
    date.year = tm_info->tm_year + 1900;
    date.month = tm_info->tm_mon + 1;
    date.day = tm_info->tm_mday;
    
    return date;
}

// Get current time
Time get_current_time() {
    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    
    Time time;
    time.hour = tm_info->tm_hour;
    time.minute = tm_info->tm_min;
    time.second = tm_info->tm_sec;
    
    return time;
}

// Get current timestamp
Timestamp get_current_timestamp() {
    Timestamp ts;
    ts.date = get_current_date();
    ts.time = get_current_time();
    return ts;
}

// Format date as string (YYYY-MM-DD)
void format_date(const Date* date, char* buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "%04d-%02d-%02d", date->year, date->month, date->day);
}

// Format time as string (HH:MM:SS)
void format_time(const Time* time, char* buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "%02d:%02d:%02d", time->hour, time->minute, time->second);
}

// Format timestamp as string (YYYY-MM-DD HH:MM:SS)
void format_timestamp(const Timestamp* ts, char* buffer, size_t buffer_size) {
    char date_str[11]; // YYYY-MM-DD
    char time_str[9];  // HH:MM:SS
    
    format_date(&ts->date, date_str, sizeof(date_str));
    format_time(&ts->time, time_str, sizeof(time_str));
    
    snprintf(buffer, buffer_size, "%s %s", date_str, time_str);
}

// Parse date from string (YYYY-MM-DD)
bool parse_date(const char* str, Date* date) {
    int year, month, day;
    if (sscanf(str, "%d-%d-%d", &year, &month, &day) != 3) {
        return false;
    }
    
    // Validate values
    if (year < 1970 || month < 1 || month > 12 || day < 1 || 
        day > get_days_in_month(year, month)) {
        return false;
    }
    
    date->year = year;
    date->month = month;
    date->day = day;
    
    return true;
}

// Parse time from string (HH:MM:SS)
bool parse_time(const char* str, Time* time) {
    int hour, minute, second;
    if (sscanf(str, "%d:%d:%d", &hour, &minute, &second) != 3) {
        return false;
    }
    
    // Validate values
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || 
        second < 0 || second > 59) {
        return false;
    }
    
    time->hour = hour;
    time->minute = minute;
    time->second = second;
    
    return true;
}

// Parse timestamp from string (YYYY-MM-DD HH:MM:SS)
bool parse_timestamp(const char* str, Timestamp* ts) {
    char date_str[11];
    char time_str[9];
    
    if (sscanf(str, "%10s %8s", date_str, time_str) != 2) {
        return false;
    }
    
    return parse_date(date_str, &ts->date) && parse_time(time_str, &ts->time);
}
