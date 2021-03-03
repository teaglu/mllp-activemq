
AC_PREREQ([2.69])
AC_INIT([mllp-activemq], [0.9], [info@teaglu.com])
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE

AC_PROG_CXX
AC_LANG_PUSH(C++)

AC_TYPE_SIZE_T

CXXFLAGS="${CXXFLAGS} -I/usr/include/activemq-cpp-3.9.3"
CXXFLAGS="${CXXFLAGS} -I/usr/local/include/activemq-cpp-3.9.5"
CXXFLAGS="${CXXFLAGS} -I/usr/include/jsoncpp"

AC_CHECK_HEADERS([json/value.h], [], [AC_MSG_ERROR([JsonCPP Missing])])
AC_CHECK_HEADERS([activemq/core/ActiveMQConnectionFactory.h], [], [AC_MSG_ERROR([ActiveMQ Missing])])

AC_OUTPUT(Makefile src/Makefile)
