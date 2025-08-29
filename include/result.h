#pragma once
#include <WString.h>
#include <optional>
#include <utility>

template <typename T, typename E> class Result {
  public:
    static Result Ok(T value) { return Result(std::move(value), std::nullopt); }

    static Result Err(E error) { return Result(std::nullopt, std::move(error)); }

    bool isOk() const { return value_.has_value(); }
    bool isErr() const { return error_.has_value(); }

    T& unwrap() { return *value_; }
    const T& unwrap() const { return *value_; }

    E& unwrapErr() { return *error_; }
    const E& unwrapErr() const { return *error_; }

    explicit operator bool() const { return isOk(); }

  private:
    std::optional<T> value_;
    std::optional<E> error_;

    Result(std::optional<T> value, std::optional<E> error)
        : value_(std::move(value)), error_(std::move(error)) {}
};
