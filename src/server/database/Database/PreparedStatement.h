#ifndef _PREPAREDSTATEMENT_H
#define _PREPAREDSTATEMENT_H

#include "Define.h"
#include "Duration.h"
#include "SQLOperation.h"
#include <future>
#include <vector>
#include <variant>

struct PreparedStatementData
{
    std::variant<
        bool,
        uint8,
        uint16,
        uint32,
        uint64,
        int8,
        int16,
        int32,
        int64,
        float,
        double,
        std::string,
        std::vector<uint8>,
        SystemTimePoint,
        std::nullptr_t
    > data;

    template<typename T>
    static std::string ToString(T value);

    static std::string ToString(bool value);
    static std::string ToString(uint8 value);
    static std::string ToString(int8 value);
    static std::string ToString(std::string const& value);
    static std::string ToString(std::vector<uint8> const& value);
    static std::string ToString(SystemTimePoint value);
    static std::string ToString(std::nullptr_t);
};

//- Upper-level class that is used in code
class TC_DATABASE_API PreparedStatementBase
{
    friend class PreparedStatementTask;

    public:
        explicit PreparedStatementBase(uint32 index, uint8 capacity);
        virtual ~PreparedStatementBase();

        void setNull(uint8 index);
        void setBool(uint8 index, bool value);
        void setUInt8(uint8 index, uint8 value);
        void setUInt16(uint8 index, uint16 value);
        void setUInt32(uint8 index, uint32 value);
        void setUInt64(uint8 index, uint64 value);
        void setInt8(uint8 index, int8 value);
        void setInt16(uint8 index, int16 value);
        void setInt32(uint8 index, int32 value);
        void setInt64(uint8 index, int64 value);
        void setFloat(uint8 index, float value);
        void setDouble(uint8 index, double value);
        void setDate(uint8 index, SystemTimePoint value);
        void setString(uint8 index, std::string const& value);
        void setStringView(uint8 index, std::string_view value);
        void setBinary(uint8 index, std::vector<uint8> const& value);
        template <size_t Size>
        void setBinary(const uint8 index, std::array<uint8, Size> const& value)
        {
            std::vector<uint8> vec(value.begin(), value.end());
            setBinary(index, vec);
        }

        uint32 GetIndex() const { return m_index; }
        std::vector<PreparedStatementData> const& GetParameters() const { return statement_data; }

    protected:
        uint32 m_index;

        //- Buffer of parameters, not tied to MySQL in any way yet
        std::vector<PreparedStatementData> statement_data;

        PreparedStatementBase(PreparedStatementBase const& right) = delete;
        PreparedStatementBase& operator=(PreparedStatementBase const& right) = delete;
};

template<typename T>
class PreparedStatement : public PreparedStatementBase
{
public:
    explicit PreparedStatement(uint32 index, uint8 capacity) : PreparedStatementBase(index, capacity)
    {
    }

private:
    PreparedStatement(PreparedStatement const& right) = delete;
    PreparedStatement& operator=(PreparedStatement const& right) = delete;
};

//- Lower-level class, enqueuable operation
class TC_DATABASE_API PreparedStatementTask : public SQLOperation
{
    public:
        // 是否异步
        PreparedStatementTask(PreparedStatementBase* stmt, bool async = false);
        ~PreparedStatementTask();

        bool Execute() override;
        PreparedQueryResultFuture GetFuture() { return m_result->get_future(); }

    protected:
        PreparedStatementBase* m_stmt;
        bool m_has_result;
        PreparedQueryResultPromise* m_result;
};
#endif
