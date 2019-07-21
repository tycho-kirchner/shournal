#pragma once

#include <type_traits>


template <class ContainerT>
class TypeHas_push_back
{
    template <class T>
    // signature has to match *exactly* (indclduing e.g. a 'const' at the end for a
    // const object method
    static std::true_type testSignature(void (T::*)(const typename ContainerT::value_type&));

    template <class T>
    static decltype(testSignature(&T::push_back)) test(std::nullptr_t);

    template <class T>
    static std::false_type test(...);

public:
    using type = decltype(test<ContainerT>(nullptr));
    static const bool value = type::value;
};


/// Allow adding to containers which either implement push_back or
/// insert. The container must provide Container::value_type.
template <class ContainerT,
          typename std::enable_if<TypeHas_push_back<ContainerT>::value,
          std::nullptr_t>::type = nullptr>
void addToContainer(ContainerT& t, const typename ContainerT::value_type& val ){
    t.push_back(val);
}

template <class ContainerT,
          typename std::enable_if<!TypeHas_push_back<ContainerT>::value,
          std::nullptr_t>::type = nullptr>
void addToContainer(ContainerT& t, const typename ContainerT::value_type& val ){
    t.insert(val);
}



