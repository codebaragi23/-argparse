ArgumentParser
==============
A slimline C++ class for parsing command-line arguments, with an interface similar to python's class of the same name.

Usage
-----
An example says it best:
  
    int main(int argc, const char** argv) {

      // make a new ArgumentParser
      ArgumentParser parser;

      // add some arguments to search for
      parser.addArgument("-b");
      parser.addArgument("-n", "--name");
      parser.addArgument("-i", "--input", 1, "123", true);
      parser.addArgument("--strings", '+');
      parser.addFinalArgument("output");

      // parse the command-line arguments - throws if invalid format
      parser.parse(argc, argv);

      // if we get here, the configuration is valid
      std::string name = parser.retrieve("name");
      int input = parser.retrieve<int>("input");
      vector<std::string> strings = parser.retrieve<vector<std::string>>("strings");
      int output = parser.retrieve<int>("output");
      return output;
    }

If the supplied format is incorrect or we explicitly call `parser.usage()`, a usage string is printed to the terminal:

    Usage: app_name --input INPUT [-b] [--name]
                    [--string STRING [STRING...]] OUTPUT

Compiling
---------
Just grab the `argparse.hpp` header and go! The `ArgumentParser` is the only definition in `argparse.hpp`. Dependent classes are nested within `ArgumentParser`.

Format
------
**specifier**  
Arguments can be specified in a number of formats. They can have single character short names prefixed with a single '-':

    -b

or long name prefixed with '--':

    --bright

**number**  
The number of expected inputs trailing an argument can also be specified. This comes in two flavours:


1. fixed number arguments
2. variable number arguments

Fixed number arguments are simply specified with an integer which is `0` or greater. If that exact number of inputs trailing the argument is not found, the parser will fail with a `std::invalid_argument` exception. If the number is `1`, the input is stored as a string. If the number is greater than `1`, the input is stored as a vector of strings.


Variable number arguments allow for an undetermined number of inputs trailing an argument. The parser will attempt to consume as many arguments as possible until the next valid argument is encountered. There are two types of variable argument specifiers, and they use the same syntax as regular expressions:

1. `'+'` matches one or more inputs
2. `'*'` matches zero or more inputs

In both cases, the output is stored as a vector of strings. If the number of inputs is not specified, it defaults to `0`.

**required/optional**  
Arguments can be marked as either required or optional. All required arguments must appear before any optional arguments in the command-line input.

**final**  
Often UNIX command-line tools have an un-named final argument that collects all remaining inputs. The name that these inputs map to internally can be specified using the `addFinalArgument()` method of `ArgumentParser`. Along with its name, you can also specify the number of inputs to parse. Since it is un-named however, there are a number of restrictions:

1. The final argument can always require a fixed number of inputs
2. If a fixed number of inputs is specified, it must be `1` or greater
3. The final argument can only take the `'+'` specifier if an argument with variadic number of inputs has not already been specified. This restiction exists because arguments do not have a fixed ordering and a variadic argument just before the final (un-named) argument will consume all of the reminaing arguments unless the final argument requires a fixed number of inputs

Retrieving
----------
Inputs to an argument can be retrieved with the `retrieve()` method of `ArgumentParser`. Importantly, if the inputs are parsed as an array, they must be retrieved as an array. Failure to do so will result in a `std::bad_cast` exception. 

Arguments can also be cast to other types as long as the cast is trivial. For instance, we could retrieve the array of strings from the '--name' argument as an array of ints:

    std::vector<std::string> strings = parser.retrieve<std::vector<std::string>>>("strings");

or convert the required argument to a int:

    int input = parser.retrieve<int>("input");

Method Summary
--------------

    ArgumentParser()      default constructor
    useExceptions()       if true, parsing errors throw exceptions rather than printing to stderr and exiting
    appName()             set the name of the application
    addArgument()         specify an argument to search for
    addFinalArgument()    specify a final un-named argument
    ignoreFirstArgument() don't parse the first argument (usually the caller name on UNIX)
    parse()               invoke the parser on a `char**` array
    retrieve()            retrieve a set of inputs for an argument
    usage()               return a formatted usage string
    empty()               check if the set of specified arguments is empty
    clear()               clear all specified arguments
    exists()              check if an argument has been found
    count()               count the number of inputs for an argument

