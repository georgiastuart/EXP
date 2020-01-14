#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>

#include <boost/program_options.hpp>
#include <yaml-cpp/yaml.h>

namespace po = boost::program_options;

#define NAME_ID    "yaml_diff"
#define VERSION_ID "0.1"

const int indent = 2;

void recurse(YAML::Node& config1, YAML::Node& config2, int level)
{
  int cnt = 0;

  for (YAML::iterator it=config1.begin(); it!=config1.end(); it++) {

    YAML::Node cur1, cur2;

    if (config1.IsSequence()) {	// This is a YAML sequence
      std::cout << std::setw(level*indent) << "" << "- ";
      cur1 = config1[cnt];
      cur2 = config2[cnt];
      cnt++;
    } else {			// Otherwise, a YAML map
      std::cout << std::setw(level*indent) << "" << "+ " << it->first;
      std::ostringstream key; key << it->first;
      cur1 = it->second;
      cur2 = config2[key.str()];
    }

				// Check node type, if the fiducial
				// node exists
    if (cur1) {
      switch (cur1.Type()) {
      case YAML::NodeType::Null:
	break;
      case YAML::NodeType::Scalar:
	std::cout << ": " << cur1;
	if (cur2) {		// If the comparison node exists,
				// compare the values
	  std::ostringstream sout1; sout1 << cur1;
	  std::ostringstream sout2; sout2 << cur2;
	  if (sout2.str().find(sout1.str())) {
	    std::cout << "\e[1;31m != " << sout2.str() << "\e[0m";
	  }
	} else {		// Flag missing node in comparison
				// file
	  std::cout << "\e[1;36m [***]\e[0m";
	}
	std::cout << std::endl;
	break;
      case YAML::NodeType::Sequence:
      case YAML::NodeType::Map:
	if (cur2) {		// Recurse if the node is a container
	  std::cout << std::endl;
	  recurse(cur1, cur2, level+1);
	} else {
	  std::cout << "\e[1;34m [***]\e[0m" << std::endl;
	}
	break;
      case YAML::NodeType::Undefined:
	std::cout << ": Undefined" << std::endl;
      }
    } else {
      std::cout << std::endl;
    }
  }

  return;
}

int main(int argc, char** argv)
{
  po::options_description description(std::string(NAME_ID) + " usage");

  description.add_options()
    ("help,h", "Display this help message")
    ("version,v", "Display version number")
    ("input-files", po::value<std::vector<std::string>>(), "Input files");
  
  po::positional_options_description p;
  p.add("input-files", -1);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).options(description).positional(p).run(), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << description;
    
    std::cout << std::endl
	      << "This routine recursively checks every node in the 'fiducial file' against the " << std::endl
	      << "nodes in the 'comparison file' and missing nodes and different values." << std::endl
	      << "Different values in the comparison file will be printed as \e[1;31m != value\e[0m," << std::endl
	      << "while missing nodes in the comparison file will be denoted by the suffix \e[1;34m[***]\e[0m" << std::endl << std::endl
	      << "It may be useful to reverse the order of the files for a full comparsion." << std::endl << std::endl
	      << "Example: " << argv[0] << " file1.yaml file2.yaml | less -R" << std::endl << std::endl;
    return 0;
  }
  
  if (vm.count("version")) {
    std::cout << NAME_ID << " version " << VERSION_ID << std::endl;
    
    return 0;
  }
  
  
  std::vector<std::string> files;

  if (vm.count("input-files")){
    files = vm["input-files"].as<std::vector<std::string>>();
    if (files.size() != 2) {
      std::cout << std::endl
		<< "You must provide exactly 2 file names!"
		<< std::endl << std::endl << description << std::endl;
      return 0;
    }
  }
  
  std::ifstream InFile1, InFile2;
  InFile1.exceptions(std::ios::failbit);
  InFile2.exceptions(std::ios::failbit);

  try {
    InFile1.open(files[0]);
    InFile2.open(files[1]);

    YAML::Node      config1 = YAML::Load(InFile1);
    YAML::Node      config2 = YAML::Load(InFile2);
    std::string     key, val;

    recurse(config1, config2, 0);
  }
  catch (std::ios_base::failure& fail) {
    std::cout << "Failure to open <" << argv[1] << "> or <" << argv[2] << ">" << std::endl
	      << "Error: " << fail.what() << std::endl;
  }
  catch (YAML::Exception& error) {
    std::cout << "YAML failure" << std::endl
	      << "Error: " << error.what() << std::endl;
  }

  return 0;
};
