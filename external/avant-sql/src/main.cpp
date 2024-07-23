#include <iostream>
#include <memory>
#include <cstring>

#include "query.h"
#include "connection.h"
#include "sql.h"
#include "pool.h"
#include "transaction.h"

using namespace avant::sql;
using std::shared_ptr;

int main(int argc, char **argv)
{
    pool m_pool;
    m_pool.init(10, "IP", "USERNAME", "PASSWORD", "DB", 3306);
    {
        shared_ptr<connection> conn = m_pool.get();
        do
        {
            transaction auto_transaction(conn);
            query m_query(conn);
            m_query.select("SELECT * FROM DbMessageData");
            auto_transaction.commit();
        } while (false);
        m_pool.back(conn);
    }
    for (int i = 0; i < 10; i++)
    {
        shared_ptr<connection> conn = m_pool.get();

        do
        {
            transaction auto_transaction(conn);

            // SELECT
            sql<1> m_sql(conn, "SELECT GID,Uin,BlobData FROM DbMessageData WHERE GID > ?");

            class DbMessageData
            {
            public:
                DbMessageData() : BlobDataSize(16777215), BlobData(new char[BlobDataSize])
                {
                }
                ~DbMessageData()
                {
                    if (BlobData)
                    {
                        delete[] BlobData;
                    }
                }
                int64_t GID;
                int32_t Uin;
                const unsigned int BlobDataSize;
                char *BlobData{nullptr};
            };

            DbMessageData Row;

            int64_t MinGID = 0;
            m_sql.set_bind(0, &MinGID, sizeof(MinGID), value_type::LONGLONG);

            avant::sql::result<3> m_result;
            m_result.set_bind(0, &Row.GID, sizeof(Row.GID), value_type::LONGLONG);
            m_result.set_bind(1, &Row.Uin, sizeof(Row.Uin), value_type::LONG);
            m_result.set_bind(2, Row.BlobData, Row.BlobDataSize, value_type::MEDIUM_BLOB);

            int64_t maxGID = 0;
            if (m_sql.execute(m_result))
            {
                while (m_sql.fetch())
                {
                    if (!m_result.is_null[0] && maxGID < Row.GID)
                    {
                        maxGID = Row.GID;
                    }
                    if (!m_result.is_null[0] && !m_result.is_null[1] && !m_result.is_null[2] && !m_result.is_null[3] && !m_result.is_null[4])
                    {
                        std::cout << "filed len " << m_result.out_length[0] << "," << m_result.out_length[1] << "," << m_result.out_length[2] << "," << m_result.out_length[3] << "," << m_result.out_length[4] << std::endl;
                        std::cout << "GID " << Row.GID << " Uin " << Row.Uin << " BlobData " << std::string(Row.BlobData) << std::endl;
                    }
                    else
                    {
                        std::cout << "have null" << std::endl;
                    }
                }
            }
            else
            {
                auto_transaction.rollback();
                std::cout << "roll back" << std::endl;
                break;
            }

            // UPDATE
            auto_transaction.save_point();
            sql<0> m_sql_update(conn, "UPDATE DbMessageData SET Uin=777 WHERE GID > 0");
            avant::sql::result<0> m_update_result;
            if (m_sql_update.execute(m_update_result))
            {
                std::cout << "affected_rows: " << m_sql_update.get_affected_rows() << std::endl;
            }
            else
            {
                std::cout << "m_sql_update.execute failed" << std::endl;
                auto_transaction.rollback();
                std::cout << "roll back" << std::endl;
                break;
            }
            // rollback this update oper
            if (0 == auto_transaction.rollback_once())
            {
                std::cout << "rollback_once" << std::endl;
            }
            else
            {
                std::cout << "rollback_once failed" << std::endl;
                auto_transaction.rollback();
                break;
            }

            // DELETE
            sql<0> m_sql_delete(conn, "DELETE FROM DbMessageData WHERE BlobData is NULL");
            avant::sql::result<0> m_delete_result;
            if (m_sql_delete.execute(m_delete_result))
            {
                std::cout << "affected_rows: " << m_sql_delete.get_affected_rows() << std::endl;
            }
            else
            {
                std::cout << "m_sql_delete.execute failed" << std::endl;
                auto_transaction.rollback();
                std::cout << "roll back" << std::endl;
                break;
            }

            // max_allowed_packet
            sql<0> m_sql_11(conn, "SET GLOBAL max_allowed_packet = 67108864000;");
            avant::sql::result<0> m_11_result;
            if (m_sql_11.execute(m_11_result))
            {
                std::cout << "SET GLOBAL max_allowed_packet affected_rows: " << m_sql_11.get_affected_rows() << std::endl;
            }
            else
            {
                std::cout << "m_sql_11.execute failed" << std::endl;
                auto_transaction.rollback();
                std::cout << "roll back" << std::endl;
                break;
            }

            // INSERT
            sql<6> m_sql_insert(conn, "INSERT INTO DbMessageData(GID,Uin,Time,Title,ExistBlob,BlobData)  VALUES (?,?,?,?,? ,?)");
            int64_t GID = maxGID + 1;
            m_sql_insert.set_bind(0, &GID, sizeof(GID), value_type::LONGLONG);

            int32_t Uin = 0;
            m_sql_insert.set_bind(1, &Uin, sizeof(Uin), value_type::LONG);

            uint32_t Time = UINT32_MAX;
            m_sql_insert.set_bind(2, &Time, sizeof(Time), value_type::LONG_UNSIGNED);

            char Title[64]{0};
            const char *TITLE_STR = "this is a title";
            strncpy(Title, TITLE_STR, strlen(TITLE_STR));
            m_sql_insert.set_bind(3, Title, sizeof(Title), value_type::VAR_STRING, 0);

            unsigned char ExistBlob = 1;
            m_sql_insert.set_bind(4, &ExistBlob, sizeof(ExistBlob), value_type::TINY);

            char BlobData[1]{0};
            m_sql_insert.set_bind(5, BlobData, sizeof(BlobData), value_type::MEDIUM_BLOB, 0);

            avant::sql::result<0> m_insert_result;
            if (m_sql_insert.execute(m_insert_result))
            {
                std::cout << "affected_rows: " << m_sql_insert.get_affected_rows() << std::endl;
            }
            else
            {
                std::cout << "m_sql_insert.execute failed" << std::endl;
                auto_transaction.rollback();
                std::cout << "roll back" << std::endl;
                break;
            }

            auto_transaction.commit();
        } while (false);

        m_pool.back(conn);
    }
    return 0;
}

// CREATE TABLE DbMessageData (
//     GID BIGINT,
//     Uin INT,
//     Time INT UNSIGNED,
//     Title VARCHAR(64),
//     ExistBlob TINYINT,
//     BlobData MEDIUMBLOB,
//     PRIMARY KEY (GID)
// );