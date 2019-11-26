#pragma once

#include <cstddef>
#include <limits>
#include <queue>


template<typename T, typename container, typename compare>
class limited_priority_queue : public std::priority_queue<T, container, compare>
{
public:
    typedef typename container::value_type value_type;

public:

    void
    push(const T& val)
    {
        std::priority_queue<T, container, compare>::push(val);
        if(static_cast<size_t>(this->size()) > m_maxSize){
            this->pop();
        }
    }

    void
    setMaxSize(const size_t &maxSize){
        m_maxSize = maxSize;
    }

    template<typename PopContainerT>
    PopContainerT
    popAll(bool reverse=false){
        PopContainerT ret(this->size());
        if(reverse){
            for(auto it = ret.rbegin(); it != ret.rend(); ++it ){
                *it = this->top();
                this->pop();
            }
        } else {
            for(auto & el : ret){
                el = this->top();
                this->pop();
            }
        }
        return ret;
    }

protected:
    size_t m_maxSize{std::numeric_limits<size_t>::max()};
};

