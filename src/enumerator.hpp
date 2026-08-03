#ifndef ENUMERATOR_HPP
#define ENUMERATOR_HPP

#include <type_traits>


namespace enumerator {

    template<typename T>
    struct limits;


    namespace concepts {

        template<typename T>
        concept enumeration = std::is_enum_v<T>;


        template<typename T>
        concept limited_below = enumeration<T> && requires { limits<T>::begin; };


        template<typename T>
        concept limited_above = enumeration<T> && requires { limits<T>::end; };


        template<typename T>
        concept limited = limited_below<T> && limited_above<T>;

        template<typename T>
        concept implicit_limit_above = enumeration<T> && requires { T::count; };

    } // namespace concepts


    template<concepts::enumeration E>
    class sequence {

    public:

        using enum_type = E;
        using underlying_type = std::underlying_type_t<enum_type>;


        class iterator {

        public:

            using enum_type = sequence::enum_type;
            using underlying_type = sequence::underlying_type;


            constexpr
            iterator()
                noexcept = default;


            explicit
            constexpr
            iterator(enum_type e)
                noexcept :
                current{static_cast<underlying_type>(e)}
            {}


            explicit
            constexpr
            iterator(underlying_type v)
                noexcept :
                current{v}
            {}


            constexpr
            iterator&
            operator ++()
                noexcept
            {
                ++current;
                return *this;
            }


            constexpr
            iterator
            operator ++(int)
                noexcept
            {
                iterator other = *this;
                ++current;
                return other;
            }


            constexpr
            enum_type
            operator *()
                const noexcept
            {
                return static_cast<enum_type>(current);
            }


            [[nodiscard]]
            constexpr
            bool
            operator ==(const iterator&)
                const noexcept = default;


            [[nodiscard]]
            auto
            operator <=>(const iterator&)
                const noexcept = default;


        private:

            underlying_type current{0};

        }; // class iterator


        constexpr
        sequence(enum_type begin_value,
                 enum_type end_value)
            noexcept :
            begin_it{iterator{begin_value}},
            end_it{iterator{end_value}}
        {}


        [[nodiscard]]
        constexpr
        iterator
        begin()
            const noexcept
        {
            return begin_it;
        }


        [[nodiscard]]
        constexpr
        iterator
        end()
            const noexcept
        {
            return end_it;
        }


    private:

        const iterator begin_it;
        const iterator end_it;

    }; // class sequence<E>


    template<concepts::limited E>
    [[nodiscard]]
    constexpr
    sequence<E>
    enumerate(E begin_value = limits<E>::begin,
              E end_value = limits<E>::end)
        noexcept
    {
        return {begin_value, end_value};
    }


    template<concepts::limited_above E>
    [[nodiscard]]
    constexpr
    sequence<E>
    enumerate(E begin_value = E{},
              E end_value = limits<E>::end)
        noexcept
    {
        return { begin_value, end_value };
    }


    template<concepts::implicit_limit_above E>
    [[nodiscard]]
    constexpr
    sequence<E>
    enumerate(E begin_value = E{},
              E end_value = E::count)
    {
        return { begin_value, end_value };
    }


    template<concepts::enumeration E>
    [[nodiscard]]
    constexpr
    sequence<E>
    enumerate(E begin_value,
              E end_value)
        noexcept
    {
        return { begin_value, end_value };
    }

} // namespace enumerator

#endif
