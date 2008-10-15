#ifndef DHT_DB_HH
#define DHT_DB_HH
#include <click/element.hh>
#include <click/vector.hh>


#define DB_INT 0
#define DB_ARRAY 1

CLICK_DECLS


class BRNDB
{
  public:
    class DBrow {
      public:
        Vector<char*> _row_entries;
        Vector<int> _row_entries_len;

        DBrow()
        {
        }

        ~DBrow()
        {
        }

    };

  public:

    //BRNDB();
    BRNDB(Vector<String>,Vector<int>);
    ~BRNDB();

  private:

    Vector<String> _col_names;
    Vector<int> _col_types;
    Vector<DBrow> _rows;

    int _debug;
};

CLICK_ENDDECLS
#endif
