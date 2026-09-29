#pragma once

#include <optional>
#include <stdexcept>
#include <utility>

namespace amen::core
{

template <typename T, typename E>
class Result
{
public:
    static Result ok(T value)
    {
        return Result(std::move(value), std::nullopt);
    }

    static Result err(E error)
    {
        return Result(std::nullopt, std::move(error));
    }

    bool isOk() const noexcept { return value_.has_value(); }
    bool isErr() const noexcept { return error_.has_value(); }

    const T& value() const
    {
        if (!value_.has_value())
            throw std::logic_error("Attempted to read value() from error Result");
        return *value_;
    }

    T& value()
    {
        if (!value_.has_value())
            throw std::logic_error("Attempted to read value() from error Result");
        return *value_;
    }

    const E& error() const
    {
        if (!error_.has_value())
            throw std::logic_error("Attempted to read error() from ok Result");
        return *error_;
    }

private:
    Result(std::optional<T> value, std::optional<E> error)
        : value_(std::move(value)), error_(std::move(error))
    {
    }

    std::optional<T> value_;
    std::optional<E> error_;
};

} // namespace amen::core
