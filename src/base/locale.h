// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace cutefish::base {

// A day of the proleptic Gregorian calendar.
struct CalendarDate
{
    int year = 0;
    // 1 to 12.
    int month = 0;
    int day = 0;

    bool isValid() const;
    // 1 for Monday to 7 for Sunday.
    int dayOfWeek() const;
    bool operator==(const CalendarDate&) const = default;
};

struct ClockTime
{
    int hour = 0;
    int minute = 0;
    int second = 0;
    int millisecond = 0;

    bool isValid() const;
    bool operator==(const ClockTime&) const = default;
};

// A wall-clock date and time with the zone it was read in, which the "t"
// format letters show.
struct DateTime
{
    CalendarDate date;
    ClockTime time;
    // Seconds east of UTC.
    int utcOffset = 0;
    // As the system's time zone database abbreviates it, e.g. "+08" or "CEST".
    std::string zoneAbbreviation;
    // The IANA id, such as "Asia/Singapore", for the zone's long name.
    std::string zoneId;

    // The local date and time at that moment, in the system's time zone.
    static DateTime fromSecondsSinceEpoch(int64_t seconds);
    // The same wall clock in UTC.
    static DateTime utc(CalendarDate date, ClockTime time);
};

// Dates, times and numbers written exactly as Qt's QLocale writes them, with
// Qt's format strings and CLDR's data through ICU. The system locale follows
// QLocale::system(): dates, times, day and month names and the first day of
// the week come from LC_TIME, number symbols from LC_NUMERIC, sorting from
// LC_COLLATE, and the name, the weekend and the digit grouping from
// LC_MESSAGES. The categories are read through the C library's locale setup,
// so a locale that is not installed counts as "C".
class Locale
{
public:
    enum class FormatType {
        Long,
        Short,
        Narrow,
    };

    enum class DataSizeFormat {
        // 1024-based with KiB, MiB, ...
        Iec,
        // 1024-based with kB, MB, ...
        Traditional,
        // 1000-based with kB, MB, ...
        Si,
    };

    // The session's locale; read once.
    static const Locale& system();
    // Every category from one locale, such as "en_SG", "zh_CN" or "C"; a
    // codeset or modifier ("en_SG.UTF-8@euro") is ignored.
    explicit Locale(std::string_view name);
    ~Locale();

    Locale(const Locale&);
    Locale& operator=(const Locale&);

    // As QLocale::name(): language and territory, such as "en_US", or "C".
    std::string name() const;
    // The language's code, such as "en"; "C" for the C locale.
    std::string language() const;

    // Qt format strings, such as "dddd, d MMMM yyyy" or "h:mm Ap".
    std::string dateFormat(FormatType type = FormatType::Long) const;
    std::string timeFormat(FormatType type = FormatType::Long) const;
    std::string dateTimeFormat(FormatType type = FormatType::Long) const;

    // Months 1 to 12; days 1 for Monday to 7 for Sunday. The standalone
    // forms name a month or day on its own, the others within a date.
    std::string monthName(int month, FormatType type = FormatType::Long) const;
    std::string standaloneMonthName(int month, FormatType type = FormatType::Long) const;
    std::string dayName(int day, FormatType type = FormatType::Long) const;
    std::string standaloneDayName(int day, FormatType type = FormatType::Long) const;
    std::string amText() const;
    std::string pmText() const;
    // 1 for Monday to 7 for Sunday.
    int firstDayOfWeek() const;
    // The working days, 1 for Monday to 7 for Sunday, in that order.
    std::vector<int> weekdays() const;

    std::string toString(const CalendarDate& date, FormatType type = FormatType::Long) const;
    std::string toString(const ClockTime& time, FormatType type = FormatType::Long) const;
    std::string toString(const DateTime& dateTime, FormatType type = FormatType::Long) const;
    // With a Qt format string: d dd ddd dddd, M MM MMM MMMM, yy yyyy, h hh
    // H HH, m mm, s ss, z zzz, AP ap Ap aP, t tt ttt tttt and 'quoted text'.
    std::string toString(const CalendarDate& date, std::string_view format) const;
    std::string toString(const ClockTime& time, std::string_view format) const;
    std::string toString(const DateTime& dateTime, std::string_view format) const;

    std::string decimalPoint() const;
    std::string groupSeparator() const;
    std::string negativeSign() const;
    std::string positiveSign() const;
    std::string zeroDigit() const;

    // As QLocale::toString(value, 'f', decimals): exactly that many decimals,
    // halfway values rounded away from zero, digits grouped.
    std::string toString(double value, int decimals) const;
    std::string toString(int64_t value) const;
    std::string toString(int value) const { return toString(int64_t(value)); }
    // As QLocale::formattedDataSize(), e.g. "1.50 KiB" or "999 bytes".
    std::string formattedDataSize(int64_t bytes, int precision = 2,
                                  DataSizeFormat format = DataSizeFormat::Iec) const;

    // As QString::localeAwareCompare(): < 0, 0 or > 0.
    int compare(std::string_view a, std::string_view b) const;

    // As QLocale::territoryToString(): the English name of an ISO 3166 code,
    // such as "SG" for "Singapore".
    static std::string territoryName(std::string_view code);

    struct Data;

private:
    Locale() = default;

    std::shared_ptr<const Data> m_messages;
    std::shared_ptr<const Data> m_time;
    std::shared_ptr<const Data> m_numeric;
    std::string m_collation;
    // Off for an explicit C locale, as Qt's QLocale::c() omits separators.
    bool m_groupDigits = true;
};

} // namespace cutefish::base
