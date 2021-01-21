#ifndef ARGPARSE_HPP_
#define ARGPARSE_HPP_

#if __cplusplus >= 201103L
#include <unordered_map>
typedef std::unordered_map<std::string, size_t> IndexMap;
#else
#include <map>
typedef std::map<std::string, size_t> IndexMap;
#endif
#include <string>
#include <vector>
#include <typeinfo>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <cassert>
#include <algorithm>

/*! @class ArgumentParser
 *  @brief A simple command-line argument parser based on the design of
 *  python's parser of the same name.
 *
 *  ArgumentParser is a simple C++ class that can parse arguments from
 *  the command-line or any array of strings. The syntax is familiar to
 *  anyone who has used python's ArgumentParser:
 *  \code
 *    // create a parser and add the options
 *    ArgumentParser parser;
 *    parser.addArgument("-a");
 *    parser.addArgument("-n", "--name");
 *    parser.addArgument("-i", "--input", 1, "123");
 *    parser.addArgument("--strings", '+');
 * 
 *    // parse the command-line arguments
 *    parser.parse(argc, argv);
 *
 *    // get the inputs and iterate over them
 *    std::string name = parser.retrieve("name");
 *    int input = parser.retrieve<int>("input");   // default 123
 *    vector<std::string> strings = parser.retrieve<vector<std::string>>("strings");
 *  \endcode
 *
 */
class ArgumentParser
{
private:
  class Any;
  class Argument;
  class PlaceHolder;
  class Holder;

  // --------------------------------------------------------------------------
  // Type-erasure internal storage
  // --------------------------------------------------------------------------
  class Any
  {
    template <typename T>
    struct identity
    {
      typedef T type;
    };

  public:
    // constructor
    Any() : content(0) {}
    // destructor
    ~Any() { delete content; }
    // INWARD CONVERSIONS
    Any(const Any &other) : content(other.content ? other.content->clone() : 0) {}
    template <typename ValueType>
    Any(const ValueType &other)
        : content(new Holder<ValueType>(other)) {}
    Any &swap(Any &other)
    {
      std::swap(content, other.content);
      return *this;
    }
    Any &operator=(const Any &rhs)
    {
      Any tmp(rhs);
      return swap(tmp);
    }
    template <typename ValueType>
    Any &operator=(const ValueType &rhs)
    {
      Any tmp(rhs);
      return swap(tmp);
    }
    // OUTWARD CONVERSIONS
    template <typename ValueType>
    ValueType castTo()
    {
      if (content->type_info() == typeid(ValueType))
        return static_cast<Holder<ValueType> *>(content)->held_;
      else
        castTo(identity<ValueType>());
    }

  private:
    template <typename ValueType>
    ValueType castTo(identity<ValueType>) { throw std::bad_cast(); }
    double castTo(identity<double>) { std::stod(static_cast<Holder<std::string> *>(content)->held_); }
    int castTo(identity<int>) { std::stoi(static_cast<Holder<std::string> *>(content)->held_); }

  private:
    // Inner placeholder interface
    class PlaceHolder
    {
    public:
      virtual ~PlaceHolder() {}
      virtual const std::type_info &type_info() const = 0;
      virtual PlaceHolder *clone() const = 0;
    };
    // Inner template concrete instantiation of PlaceHolder
    template <typename ValueType>
    class Holder : public PlaceHolder
    {
    public:
      ValueType held_;
      Holder(const ValueType &value) : held_(value) {}
      virtual const std::type_info &type_info() const { return typeid(ValueType); }
      virtual PlaceHolder *clone() const { return new Holder(held_); }
    };

    PlaceHolder *content;
  };

  // --------------------------------------------------------------------------
  // Argument
  // --------------------------------------------------------------------------
  static std::string delimit(const std::string &name)
  {
    return std::string(std::min(name.size(), (size_t)2), '-').append(name);
  }
  static std::string strip(const std::string &name)
  {
    size_t begin = 0;
    begin += name.size() > 0 ? name[0] == '-' : 0;
    begin += name.size() > 3 ? name[1] == '-' : 0;
    return name.substr(begin);
  }
  static std::string upper(const std::string &in)
  {
    std::string out(in);
    std::transform(out.begin(), out.end(), out.begin(), ::toupper);
    return out;
  }
  static std::string escape(const std::string &in)
  {
    std::string out(in);
    if (in.find(' ') != std::string::npos)
      out = std::string("\"").append(out).append("\"");
    return out;
  }

  struct Argument
  {
    Argument() : short_name(""), name(""), required(false), default_value(""), help(""), fixed_nargs(0), fixed(true) {}
    Argument(const std::string &_short_name, const std::string &_name, bool _required, char nargs, std::string _default = "", std::string _help = "")
        : short_name(_short_name), name(_name), required(_required), default_value(_default), help(_help)
    {
      if (nargs == '+' || nargs == '*')
      {
        variable_nargs = nargs;
        fixed = false;
      }
      else
      {
        fixed_nargs = nargs;
        fixed = true;
      }
    }
    std::string short_name;
    std::string name;
    bool required;
    std::string default_value;
    std::string help;
    union
    {
      size_t fixed_nargs;
      char variable_nargs;
    };
    bool fixed;
    std::string canonicalName() const { return (name.empty()) ? short_name : name; }
    std::string toString(bool named = true) const
    {
      std::ostringstream s;
      std::string uname = name.empty() ? upper(strip(short_name)) : upper(strip(name));
      if (named && !required)
        s << "[";
      if (named)
        s << canonicalName();
      if (fixed)
      {
        size_t N = std::min((size_t)3, fixed_nargs);
        for (size_t n = 0; n < N; ++n)
          s << " " << uname;
        if (N < fixed_nargs)
          s << " ...";
      }
      if (!fixed)
      {
        s << " ";
        if (variable_nargs == '*')
          s << "[";
        s << uname << " ";
        if (variable_nargs == '+')
          s << "[";
        s << uname << "...]";
      }
      if (named && !required)
        s << "]";
      return s.str();
    }
  };

  void insertArgument(const Argument &arg)
  {
    size_t N = arguments_.size();
    arguments_.push_back(arg);
    if (arg.fixed && arg.fixed_nargs <= 1)
    {
      variables_.push_back(arg.default_value);
    }
    else
    {
      variables_.push_back(std::vector<std::string>());
    }
    if (!arg.short_name.empty())
      index_[arg.short_name] = N;
    if (!arg.name.empty())
      index_[arg.name] = N;
    if (arg.required && arg.default_value.empty())
      required_++;
  }

  // --------------------------------------------------------------------------
  // Error handling
  // --------------------------------------------------------------------------
  void argumentError(const std::string &msg, bool show_usage = false)
  {
    if (use_exceptions_)
      throw std::invalid_argument(msg);
    std::cerr << "ArgumentParser error: " << msg << std::endl;
    if (show_usage)
      std::cerr << usage() << std::endl;
    exit(-5);
  }

  // --------------------------------------------------------------------------
  // Member variables
  // --------------------------------------------------------------------------
  IndexMap index_;
  bool ignore_first_;
  bool use_exceptions_;
  size_t required_;
  std::string app_name_;
  std::string final_name_;
  std::vector<Argument> arguments_;
  std::vector<Any> variables_;

public:
  ArgumentParser() : ignore_first_(true), use_exceptions_(false), required_(0) {}
  // --------------------------------------------------------------------------
  // addArgument
  // --------------------------------------------------------------------------
  void appName(const std::string &name) { app_name_ = name; }
  void addArgument(const std::string &name, char nargs = 0,
                   std::string _default = "", bool required = false, std::string help = "")
  {
    if (name.size() > 2)
    {
      Argument arg("", verify(name), required, nargs);
      insertArgument(arg);
    }
    else
    {
      Argument arg(verify(name), "", required, nargs);
      insertArgument(arg);
    }
  }
  void addArgument(const std::string &short_name, const std::string &name, char nargs = 0,
                   std::string _default = "", bool required = false, std::string help = "")
  {
    Argument arg(verify(short_name), verify(name), required, nargs, _default, help);
    insertArgument(arg);
  }
  void addFinalArgument(const std::string &name, char nargs = 1, std::string _default = "", bool required = true, std::string help = "")
  {
    final_name_ = delimit(name);
    Argument arg("", final_name_, required, nargs);
    insertArgument(arg);
  }
  void ignoreFirstArgument(bool ignore_first) { ignore_first_ = ignore_first; }
  std::string verify(const std::string &name)
  {
    if (name.empty())
      argumentError("argument names must be non-empty");
    if ((name.size() == 2 && name[0] != '-') || name.size() == 3)
      argumentError(std::string("invalid argument '")
                        .append(name)
                        .append("'. Short names must begin with '-'"));
    if (name.size() > 3 && (name[0] != '-' || name[1] != '-'))
      argumentError(std::string("invalid argument '")
                        .append(name)
                        .append("'. Multi-character names must begin with '--'"));
    return name;
  }

  // --------------------------------------------------------------------------
  // Parse
  // --------------------------------------------------------------------------
  void parse(size_t argc, const char **argv) { parse(std::vector<std::string>(argv, argv + argc)); }

  void parse(const std::vector<std::string> &argv)
  {
    // check if the app is named
    if (app_name_.empty() && ignore_first_ && !argv.empty())
    {
      app_name_ = argv[0];
      app_name_ = app_name_.substr(app_name_.find_last_of("\\/") + 1, app_name_.length());
    }

    // set up the working set
    Argument active;
    Argument final = final_name_.empty() ? Argument() : arguments_[index_[final_name_]];
    size_t consumed = 0;
    size_t nrequired = !final.required ? required_ : required_ - 1;
    size_t nfinal = !final.required ? 0 : (final.fixed ? final.fixed_nargs : (final.variable_nargs == '+' ? 1 : 0));

    // iterate over each element of the array
    for (std::vector<std::string>::const_iterator in = argv.begin() + ignore_first_;
         in < argv.end() - nfinal; ++in)
    {
      std::string active_name = active.canonicalName();
      std::string el = *in;
      //  check if the element is a key
      if (index_.count(el) == 0)
      {
        // input
        // is the current active argument expecting more inputs?
        if (active.fixed && active.fixed_nargs <= consumed)
          argumentError(std::string("attempt to pass too many inputs to ").append(active_name),
                        true);
        if (active.fixed && active.fixed_nargs == 1)
        {
          variables_[index_[active_name]].castTo<std::string>() = el;
        }
        else
        {
          variables_[index_[active_name]].castTo<std::vector<std::string>>().push_back(el);
        }
        consumed++;
      }
      else
      {
        // new key!
        // has the active argument consumed enough elements?
        if ((active.fixed && active.fixed_nargs != consumed) ||
            (!active.fixed && active.variable_nargs == '+' && consumed < 1))
          argumentError(std::string("encountered argument ")
                            .append(el)
                            .append(" when expecting more inputs to ")
                            .append(active_name),
                        true);
        active = arguments_[index_[el]];
        // check if we've satisfied the required arguments
        if (!active.required && nrequired > 0)
          argumentError(std::string("encountered required argument ")
                            .append(el)
                            .append(" when expecting more required arguments"),
                        true);
        // are there enough arguments for the new argument to consume?
        if ((active.fixed && active.fixed_nargs > (argv.end() - in - nfinal - 1)) ||
            (!active.fixed && active.variable_nargs == '+' &&
             !(argv.end() - in - nfinal - 1)))
          argumentError(std::string("too few inputs passed to argument ").append(el), true);
        if (active.required && active.default_value.empty())
          nrequired--;
        consumed = 0;
      }
    }

    for (std::vector<std::string>::const_iterator in =
             std::max(argv.begin() + ignore_first_, argv.end() - nfinal);
         in != argv.end(); ++in)
    {
      std::string el = *in;
      // check if we accidentally find an argument specifier
      if (index_.count(el))
        argumentError(std::string("encountered argument specifier ")
                          .append(el)
                          .append(" while parsing final required inputs"),
                      true);
      if (final.fixed && final.fixed_nargs == 1)
      {
        variables_[index_[final_name_]].castTo<std::string>() = el;
      }
      else
      {
        variables_[index_[final_name_]].castTo<std::vector<std::string>>().push_back(el);
      }
      nfinal--;
    }

    // check that all of the required arguments have been encountered
    if (nrequired > 0 || nfinal > 0)
      argumentError(std::string("too few required arguments passed to ").append(app_name_), true);
  }

  // --------------------------------------------------------------------------
  // Retrieve
  // --------------------------------------------------------------------------
  template <typename T>
  T retrieve(const std::string &name)
  {
    if (index_.count(delimit(name)) == 0)
      throw std::out_of_range("Key not found");
    else if (count(name))
      throw std::out_of_range("Value not found");

    size_t N = index_[delimit(name)];
    return variables_[N].castTo<T>();
  }

  // --------------------------------------------------------------------------
  // Properties
  // --------------------------------------------------------------------------
  std::string usage()
  {
    // premable app name
    std::ostringstream help;
    help << "Usage: " << escape(app_name_);
    size_t indent = help.str().size();
    size_t linelength = 0;

    // get the required arguments
    for (std::vector<Argument>::const_iterator it = arguments_.begin(); it != arguments_.end(); ++it)
    {
      Argument arg = *it;
      if (!arg.required)
        continue;
      if (arg.name.compare(final_name_) == 0)
        continue;
      help << " ";
      std::string argstr = arg.toString();
      if (argstr.size() + linelength > 80)
      {
        help << "\n"
             << std::string(indent, ' ');
        linelength = 0;
      }
      else
      {
        linelength += argstr.size();
      }
      help << argstr;
    }

    // get the required arguments
    for (std::vector<Argument>::const_iterator it = arguments_.begin(); it != arguments_.end(); ++it)
    {
      Argument arg = *it;
      if (arg.required)
        continue;
      if (arg.name.compare(final_name_) == 0)
        continue;
      help << " ";
      std::string argstr = arg.toString();
      if (argstr.size() + linelength > 80)
      {
        help << "\n"
             << std::string(indent, ' ');
        linelength = 0;
      }
      else
      {
        linelength += argstr.size();
      }
      help << argstr;
    }

    // get the final argument
    if (!final_name_.empty())
    {
      Argument arg = arguments_[index_[final_name_]];
      std::string argstr = arg.toString(false);
      if (argstr.size() + linelength > 80)
      {
        help << "\n"
             << std::string(indent, ' ');
        linelength = 0;
      }
      else
      {
        linelength += argstr.size();
      }
      help << argstr;
    }

    return help.str();
  }
  void useExceptions(bool state) { use_exceptions_ = state; }
  bool empty() const { return index_.empty(); }
  void clear()
  {
    ignore_first_ = true;
    required_ = 0;
    index_.clear();
    arguments_.clear();
    variables_.clear();
  }
  bool exists(const std::string &name) const { return index_.count(delimit(name)) > 0; }
  size_t count(const std::string &name)
  {
    // check if the name is an argument
    if (index_.count(delimit(name)) == 0)
      return 0;
    size_t N = index_[delimit(name)];
    Argument arg = arguments_[N];
    Any var = variables_[N];
    // check if the argument is a vector
    if (arg.fixed)
    {
      return !var.castTo<std::string>().empty();
    }
    else
    {
      return var.castTo<std::vector<std::string>>().size();
    }
  }
};
#endif