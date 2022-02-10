
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
namespace pt = boost::property_tree;

#include <assert.h>
#include <iostream>
#include <sstream>
#include <fstream>
using namespace std;

#if 0
class vm_builder {
    public:
        virtual vm_builder(string fp);
        virtual ~vm_builder();
        virtual string build_vm_args();
    private:
        string file;
}
#endif
