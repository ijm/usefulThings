/**
  @file: cmdlinearg.hh

  @brief: Provide a simple interface for decoding command line arguments
          directly into variables. (and isn't getopt).

The arguments::option class provides an interface for encapsulating
a set of command line options and connecting them to variables.

example usage :
  ...

  string outfile;
  int count;
  vector<string> infile ;
  list<int> ws;

  arguments::options args;
  
  args.option(outfile, "o", "outfile", "Output file name","out.dat" );
  args.option(count, "c", "count", "Number of loops", "13" );
  args.option(ws, "w", "w", "w list", nullptr  );
  args.option(infile, nullptr, nullptr, "Input file list", nullptr  );
  
  if ( args.populateWithHelp(argc, argv, std::cerr, 
          "Usage:\n "+string(argv[0])+" [options]\n" ) )
      exit(1);

  ...

This will accept a command line like :
  ./test --outfile foo -c 4 -w 4 -w5 -w=4 -w:1 --w 6 bar1 bar2 bar3

and produce:

  outfile = 'foo'
  count = 4
  ws = 4 5 4 1 6 
  infile = 'bar1' 'bar2' 'bar3' 


An option is declared by calling the member function option() :

  args.option(outfile, "o", "outfile", "Output file name","out.dat" );

where the first argument is the variable you want to populate with
value(s). The type will be inferred from this variable. See the
option() function for more details.

An option list is spotted when the variable is an STL container
such as list, or vector (it must have the push_back() method).

The hard work is done by the 'populate' member functions. In this case
populateWithHelp(). This adds the help option '-h' and some default boiler
plate to produce reasonable output. It calls the more flexible populate()
member function. See the specific the functions for details.

By default short options will match if the first part of the argument
string matches and will pass the remainder of the string on as value.
Long options much match completely (up to a delimiter).

By default the additional delimiters '=' and ':' are included. They really
just have the same meaning as a space and are included to improve
readability of complex argument lists.

By default the maximum option string length is 64 bytes.

These defaults can be changed by specifying template parameters when declaring
the options object, for example :

  arguments::options<32, '=', '+', '~'> args;

Would have a maximum option string length of 32, with delimiters '=', '+'
and '~'. To have no delimiters use '\0'.

For now the understood base types are : bool, int, float, string. Which, except for bool, all take one argument as value. Bool for now is true if present.

The understood container types are any that have a push_back() function.

Additional types can be added by implementing :

  static bool fromString(TYPE &v, const char* s)

for the type TYPE, returning true on success.


TODO:
* Fill out the type space a little better (need unsigned, double etc)

* Write a proper unit test program and script that tests everything in
  one go.

* Fix bool by...

* Add flags to the options so that the user can specify :
    if a bool takes an argument or is set by the presence of the option
    short or long strings match partially or completely.
    an argument must be present.
    an argument must not be duplicated.
  
* Add the ability to use '+option' style for bools.

* Currently all types can take at most one argument as value. The machinery
  is in place to allow user types, such as a structure, to take 
  multiple arguments by defining a trait :

    template<>
    struct number_of_arguments<TYPE>   { enum { n = <NUMBER_OF_ARGUMENTS> } ; };

  and will be converted from a string with :

    static bool fromString(TYPE &v, forward_list<const char*> l)

  but this isn't fully implemented.

* Direct handling of tuple types.

* the fromString() functions are a bit bloat-y. That is they are always
  compiled in even if unused. The obvious, making them simple templated
  functions makes it impossible to specialise std::string as a value
  not a container - need something better.

--ijm.

*/

#ifndef HH_CMDLINEARG_HH
#define HH_CMDLINEARG_HH

#include <forward_list>
#include <string>
#include <algorithm>

namespace arguments {
// Single string conversion types: int, bool, string.

bool fromString(std::string &v, const char* s)
   {
   v = std::string(s);
   return true;
   }

bool fromString(int &v,const char* s)
   {
   char* r;
   v = std::strtol(s, &r, 0);
   return (s != r);
   }

bool fromString(float &v,const char* s)
   {
   char* r;
   v = std::strtof(s, &r);
   return (s != r);
   }

bool fromString(bool &v, const char* _s)
  {
  std::string s(_s);
  std::transform(s.begin(), s.end(), s.begin(), ::tolower);

  bool isTrue  = s == "1" || s == "true" || s=="yes" || s=="enable" ;
  bool isFalse = s == "0" || s == "false" || s=="no" || s=="disable" ;

  return ( v = isTrue ) || isFalse;
  }

// Multi option types: vector, list.
template <typename T, template <typename,typename...> class V, typename... Ps>
bool fromString(V<T, Ps...> &v, const char* s)
  {
  T x;
  bool r;
 
  if ( (r = fromString(x, s)) )
    v.push_back(x);

  return r;
  }

// Trait that maps a type to the number of arguments it'll consume.
template<typename T> struct number_of_arguments         { enum { n = 1 } ; };
template<>           struct number_of_arguments<bool>   { enum { n = 0 } ; };

struct argStrings
  {
  const char *s, *l, *h, *d;
  };

// Error handling
enum errorstate_e { ok = 0, invalid, unknown };

struct errorState
  {
  errorstate_e state;
  const char *op, *val;

  bool isOk() { return state == ok ; }
  };

// The main workhorse object.
template<int max_string_length=64, int... delims>
struct options
  {
  // Component objects, one for each option given.
  // Responsible for virtualising on the type. It holds the long, short,
  // help, and default strings and a reference to the variable to set.
  struct argObjBase : public argStrings
    {
    bool seen;

    argObjBase(const char *_s, const char *_l, const char *_h, const char * _d)
      : argStrings{_s,_l,_h,_d}, seen(false) {};


    template<typename T, char... Ds>
    struct delimHelper { 
       bool isDelim(T c) const
        {
        for (auto x: {Ds...})
          if ( c == x )
            return true;
    
        return false;
        };
      };

    template<typename T>
    struct delimHelper<T> { 
       bool isDelim(T c) const
        {
        return ( c == '=' || c == ':' );
        };
      };

    template<typename T>
    bool isDelim(T c) const
      {
      return ( c == '=' || c == ':' );
      };

    bool isMe(const char* &d, const char* z, int sl)
      {
      const char* x = z;
      const char* y = (sl ? s : l);

      d = nullptr;

      if ( x == nullptr && y == nullptr )
        return true;

      if ( x == nullptr || y == nullptr )
        return false;

      while (*x != '\0' && *y != '\0' && (x-z) < max_string_length)
        if ( *x++ != *y++)
          return false;
      
      if (*y != '\0')
        return false;

      if (*x == '\0')
        return true;

      if ( sl )
        {
        if (delimHelper<char,delims...>().isDelim(*x))
          ++x;
        d = x;
        return true;
        }

      if (delimHelper<char,delims...>().isDelim(*x))
        {
        d = x+1;
        return true;
        }

      return false;
      }

    virtual bool setMe(const char* s) = 0;
    virtual int numArgs() = 0;
    };

  template<typename T>
  struct argObj : public argObjBase
    {
    T &v;
    virtual bool setMe(const char* s)
      {
      this->seen = true;
      return fromString(v, s);
      }

    virtual int numArgs()
      {
      return number_of_arguments<T>::n ;
      }

    argObj(const char* _s,const char* _l,const char* _h,const char* _d, T &_v)
      : argObjBase(_s, _l, _h, _d), v(_v) { };
    };

  struct noDefault : public argObjBase
    {
    virtual bool setMe(const char*) { return false ; }
    virtual int  numArgs()          { return 0; }
    noDefault() : argObjBase(nullptr, nullptr, nullptr, nullptr) {};
    };

  // This is the list of options to search.
  std::forward_list<argObjBase*> options;

  // Helper functions for setting defaults, and finding arguments.
  void setDefaults()
    {
    for (auto &a: options)
      if ( a->seen == false && a->d != nullptr )
        a->setMe(a->d);
    }

  argObjBase* findDefault()
    {
    static noDefault no;
    const char* dummy;

    for (auto &i : options)
      if (i->isMe(dummy,nullptr,0) && i->isMe(dummy,nullptr,1) )
        return i;

    return &no;
    }

  argObjBase* findArg(const char* &d, const char* s, int sl)
    {
    for (auto &i : options)
      if (i->isMe(d, s, sl) )
        return i;

    return nullptr;
    }

  // Try to process an argument, poping as many arguments as needed.
  errorState proc(std::forward_list<const char*> &l, argObjBase* defOp)
    {
    errorState allgood{ok, nullptr, nullptr};
    const char *op = l.front();

    l.pop_front(); 

    if ( op == nullptr || op[0] == '\0')
      return allgood;

    if ( op[0] == '-' )
      {
      if ( op[1] == '\0' )
        {
        while (!l.empty())
          {
          const char *val = l.front();
          l.pop_front();
          if (defOp->setMe(val) == false)
            return errorState{invalid, nullptr, val};
          }
        return allgood;
        }
      else 
        {
        const char * delm;

        argObjBase* a = (op[1]=='-') ? findArg(delm, &op[2],0) 
                                     : findArg(delm, &op[1],1);

        if ( a == nullptr )
          return errorState{unknown, op, nullptr};

        if (delm)
          l.push_front(delm);

        if ( a->numArgs() == 0 )
          return a->setMe("true") ? allgood : errorState{invalid, op, "true"};
        else
          {
          const char *val = l.front();
          l.pop_front();
          return a->setMe(val) ? allgood : errorState{invalid, op, val};
          }
        }
      }
    else
      return defOp->setMe(op) ? allgood : errorState{invalid, "default list", op};
    }
  /** Register a command line option.

      @variable The variable to be populated if this option is used.
      @s_short the short string for this option without the '-' (i.e. 'o')
      @s_long the long string for this option without the '--' (i.e. 'outfile')
      @s_help the help string for this option (i.e. 'Output file name' )
      @s_default the default value to use in a string as it would be on 
                 the command line.
  
  */
  template<typename T>
  void option(T &variable, const char* s_short, const char* s_long,
                const char* s_help, const char* s_default)
    {
    options.push_front( new argObj<T>(s_short, s_long, s_help, s_default, variable) ) ;
    }
  
  /** Populate variables given by argument() from the commandline options

      @param argc  Inbound argument count
      @param argv  Inbound argument vector
  */
  errorState populate(int c, const char *argv[]) 
    {
    std::forward_list<const char*> args;
    errorState r;

    while (c-- > 1)
      args.push_front( argv[c] );

    argObjBase* defOp = findDefault();

    while (!args.empty() && (r = proc(args, defOp)).state == ok )
      {}

    if (r.state == ok)
      setDefaults();
   
    return r;
    }

  /** Boiler plate for quick default usage and help (see populate)

      @param argc  Inbound argument count
      @param argv  Inbound argument vector
      @param os    Output stream to use for messages.
      @param usage Usage string to display before the list of options.
  */
  template<typename O>
  bool populateWithHelp(int argc, const char* argv[], O& os, const std::string usage="")
    {
    static bool help = false;

    option( help, "h", "help", "Display help.", nullptr );

    errorState e = populate(argc, argv);

    if (!e.isOk())
      { 
      os << e << "\n" ;
      os.flush();
      return true;
      }

    if (help)
      {
      if (usage != "")
        os << usage << "\n";

      os << *this << "\n" ;
      os.flush();

      return true;
      }

    return false;
    }


  };

// Stream operator to output the option, help and default.
template<typename O>
//O& operator<<(O& o, arguments::options::argObjBase* a)
O& operator<<(O& o, arguments::argStrings* a)
  {
  if (a->s == nullptr && a->l == nullptr)
    return o;

  if (a->s)
    o << "  -" << a->s ;
   
  if (a->s && a->l )
    o << ", " ;

  if (a->l)
    o << "--" << a->l ;

  if (a->h)
    o << "\t" << a->h ;

  if (a->d && (a->d)[0] != '\0' )
    o << " (default: '" << a->d << "')";

  o << "\n" ;

  return o;
  };

// Stream operator to output all the options.
template<typename O, int... Ns>
O& operator<<(O& o, arguments::options<Ns...> &args)
  {
  for (auto &a: args.options)
    o << a ;

  return o;
  }

// Stream operator to output an error. 
template<typename O>
O& operator<<(O& o, errorState &e)
  {
  switch(e.state)
    {
    case ok:
      o << "No error";
      break;

    case invalid:
      o << "Invalid Value: '" << (e.val ? e.val : "(null)")
         << "' for option '" << (e.op ? e.op : "(null)") << "'";
      break;

    case unknown:
      o << "Unknown Option: '" << (e.op ? e.op : "(null)") << "'";
      break;
    }

  return o;
  }
} // namespace arguments

//HH_CMDLINEARG_HH
#endif 


