#pragma once

#include <variant>
#include <type_traits>
#include <concepts>

// Basic expected implementation

namespace utils {

  template<typename E>
  class unexpected {
  public:
    explicit unexpected(E&& e)
      : m_error(std::move(e))
    {}
    explicit unexpected(const E& e)
      : m_error(e)
    {}

    const E& error() const& { return m_error; }
    E& error()& { return m_error; }
    const E&& error() const&& { return static_cast<const E&&>(m_error); }
    E&& error()&& { return std::move(m_error); }

  private:
    E m_error;
  };

  template<typename T, typename E>
  class expected {
  public:
    using TrueT = std::conditional_t<std::is_void_v<T>, std::monostate, T>;

    expected() = default;
    expected(const expected&) = default;
    expected(expected&&) = default;
    expected& operator=(const expected&) = default;
    expected& operator=(expected&&) = default;

    expected(const TrueT& t)
      : m_value(std::in_place_index<0>, t)
    {}
    expected(TrueT&& t)
      : m_value(std::in_place_index<0>, std::move(t))
    {}

    template<std::convertible_to<E> U>
    expected(const unexpected<U>& e)
      : m_value(std::in_place_index<1>, e.error())
    {}

    template<std::convertible_to<E> U>
    expected(unexpected<U>&& e)
      : m_value(std::in_place_index<1>, std::move(e).error())
    {}

    bool has_value() const {
      return m_value.index() == 0;
    }

    explicit operator bool() const {
      return has_value();
    }

    const TrueT& value() const& { return std::get<0>(m_value); }
    TrueT& value()& { return std::get<0>(m_value); }
    const TrueT&& value() const&& { return std::get<0>(m_value); }
    TrueT&& value()&& { return std::get<0>(m_value); }

    const TrueT& operator*() const& { return std::get<0>(m_value); }
    TrueT& operator*()& { return std::get<0>(m_value); }
    const TrueT&& operator*() const&& { return std::get<0>(m_value); }
    TrueT&& operator*()&& { return std::get<0>(m_value); }

    template<typename U>
    T value_or(U&& defaultValue) const& { return has_value() ? value() : defaultValue; }
    template<typename U>
    T value_or(U&& defaultValue) && { return has_value() ? std::move(value()) : defaultValue; }


    const TrueT* operator->() const { return &std::get<0>(m_value); }
    TrueT* operator->() { return &std::get<0>(m_value); }

    const E& error() const& { return std::get<1>(m_value); }
    E& error()& { return std::get<1>(m_value); }
    const E&& error() const&& { return std::get<1>(m_value); }
    E&& error()&& { return std::get<1>(m_value); }

  private:
    std::variant<TrueT, E> m_value;
  };

}