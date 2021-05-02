#pragma once


class ExcCppExit
{
public:
    ExcCppExit(int ret) : m_ret(ret){}

    int ret() const { return m_ret; }
private:
    int m_ret;

};

[[noreturn]]
void cpp_exit(int ret);

